#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <mcheck.h>

#include "parser.h"
#include "shell.h"

/**
 * Program that simulates a simple shell.
 * The shell covers basic commands, including builtin commands (cd and exit only), 
 * standard I/O redirection and piping (|). 
 * 
 * You should not have to worry about more complex operators such as "&&", ";", etc.
 * Your program does not have to address environment variable substitution (e.g., $HOME), 
 * double quotes, single quotes, or back quotes.
 */

#define MAX_DIRNAME 100
#define MAX_COMMAND 1024
#define MAX_TOKEN 128

/* Functions to implement, see below after main */
int execute_cd(char** words);
int execute_nonbuiltin(simple_command *s);
int execute_simple_command(simple_command *cmd);
int execute_complex_command(command *cmd);


int main(int argc, char** argv) {
	
	char cwd[MAX_DIRNAME];           /* Current working directory */
	char command_line[MAX_COMMAND];  /* The command */
	char *tokens[MAX_TOKEN];         /* Command tokens (program name, parameters, pipe, etc.) */

	while (1) {

		/* Display prompt */		
		getcwd(cwd, MAX_DIRNAME-1);
		printf("%s> ", cwd);
		
		/* Read the command line */
		gets(command_line);
		
		/* Parse the command into tokens */
		parse_line(command_line, tokens);

		/* Empty command */
		if (!(*tokens))
			continue;
		
		/* Exit */
		if (strcmp(tokens[0], "exit") == 0)
			exit(0);
				
		/* Construct chain of commands, if multiple commands */
		command *cmd = construct_command(tokens);
    
		int exitcode = 0;
		if (cmd->scmd) {
			exitcode = execute_simple_command(cmd->scmd);
			if (exitcode == -1)
				break;
		}
		else {
			exitcode = execute_complex_command(cmd);
			if (exitcode == -1)
				break;
		}
		release_command(cmd);
	}
    
	return 0;
}


int execute_cd(char** words) {

	if(words == NULL){
		return EXIT_FAILURE;
	}
	if(words[0] == NULL){
		return EXIT_FAILURE;
	}
	if(words[1] == NULL){
		return EXIT_FAILURE;
	}
	if(strcmp(words[0], "cd")!= 0){
		return EXIT_FAILURE;
	}


	char path[MAX_DIRNAME];
	strcpy(path, words[1]);
	char currentdir[MAX_DIRNAME];

	if(is_relative(words[1]) == 1){
		getcwd(currentdir, sizeof(currentdir));
		strcat(currentdir, "/");
		strcat(currentdir, words[1]);
		chdir(currentdir);
		return 0;
	}
	else if(is_relative(words[1]) == 0){
		chdir(words[1]);
		return 0;
	}
	else{
		return EXIT_FAILURE;
	}
}


int execute_command(char **tokens) {
	
	if(execvp(tokens[0], tokens) == -1){
		perror(tokens[0]);
		exit(EXIT_FAILURE);
	}
	
	return EXIT_SUCCESS;
}


int execute_nonbuiltin(simple_command *s)
{
	int filedes;


	if(s->out != NULL && s->err != NULL){
		filedes = open(s->out, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
		fcntl(filedes, F_SETFD, 1);
		dup2(filedes, fileno(stdout));
		dup2(fileno(stdout), fileno(stderr));
		close(filedes);
	}
	else if(s->out != NULL){
		filedes = open(s->out, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
		fcntl(filedes, F_SETFD, 1);
		dup2(filedes, fileno(stdout));
		close(filedes);
	}
	else if(s->in != NULL){
		filedes = open(s->in, O_RDONLY);
		fcntl(filedes, F_SETFD, 1);
		dup2(filedes, fileno(stdin));
		close(filedes);
	}
	else if(s->err != NULL){
		filedes = open(s->err, O_CREAT | O_TRUNC | O_WRONLY, 0644);
		fcntl(filedes, F_SETFD, 1);
		dup2(filedes, fileno(stderr));
		close(filedes);
	}
	
	int status = execute_command(s->tokens);

	close(filedes);
	return status;
}

int execute_simple_command(simple_command *cmd) {
	pid_t pid;
	int status;
	int result = 0;



	if(cmd->builtin == BUILTIN_CD){
		return execute_cd(cmd->tokens);
	}
	else if(cmd->builtin == BUILTIN_EXIT){
		exit(0);
	}
	else if(cmd->builtin == 0){
		if((pid = fork()) == -1){
			perror("fork");
			exit(EXIT_FAILURE);
		}
		else if(pid > 0){
			waitpid(pid, &status, 0);
			return status;
		}
		else if(pid == 0){
			execute_nonbuiltin(cmd);
			exit(0);
		}
	}
	else{
		printf("execute_simple error");
		exit(EXIT_FAILURE);
	}
	return result;
}


int execute_complex_command(command *c) {
	
	if(c->scmd != NULL){
		if(c->scmd->builtin == 0){
			execute_nonbuiltin(c->scmd);
		}
	}

	if (!strcmp(c->oper, "|")) {

		int pfd[2];
		if(pipe(pfd) == -1){
			perror("pipe");
			exit(EXIT_FAILURE);
		}

		
		pid_t pid1, pid2;
		int status;
		pid1 = fork();

		if(pid1 < 0){
			perror("fork");
			exit(EXIT_FAILURE);
		}
		else if(pid1 == 0){
			close(pfd[0]);
			close(fileno(stdout));
			dup2(pfd[1], fileno(stdout));
			execute_complex_command(c->cmd1);
			exit(0);
		}
		else{
			pid2 = fork();
			if(pid2 < 0){
				perror("fork");
				exit(EXIT_FAILURE);
			}
			else if(pid2 == 0) {
				close(pfd[1]);
				close(fileno(stdin));
				dup2(pfd[0], fileno(stdin));
				execute_complex_command(c->cmd2);
				exit(0);
			}
			else{
				close(pfd[0]);
				close(pfd[1]);
				waitpid(pid1, &status, 0);
				waitpid(pid2, &status, 0);
			}
		}
	}
	return 0;
}
