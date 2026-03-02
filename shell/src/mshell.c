#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "builtins.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#define READ_BUF_SIZE (4*MAX_LINE_LENGTH)
#define CHILD_PROCESS_STATUS_BUF_SIZE MAX_LINE_LENGTH
volatile int fg_count = 0;
volatile pid_t foreground_process[MAX_LINE_LENGTH];
typedef struct
{
	pid_t pid;
	int status;
} process;
volatile process child_process_status[CHILD_PROCESS_STATUS_BUF_SIZE];
volatile int child_process_size = 0;

//Mateusz Wojaczek

void exec_error_handler(char *program_name){
	if (errno == ENOENT)
        fprintf(stderr, "%s: no such file or directory\n", program_name);
    else if (errno == EACCES)
        fprintf(stderr, "%s: permission denied\n", program_name);
    else
        fprintf(stderr, "%s: exec error\n", program_name);
}

void file_error_handler(char *filename){
	if(errno == ENOENT)
		fprintf(stderr, "%s: no such file or directory\n", filename);
	else if(errno == EACCES)
		fprintf(stderr, "%s: permission denied\n", filename);
}

int number_of_arguments(command *com){
	//count number of arguments in command
	argseq *first_arg = com->args;
	argseq *cur_arg = com->args;
	
	int argc = 0;
	do{
		argc++;
		cur_arg = cur_arg->next;
	}while(cur_arg != first_arg);

	return argc;
}

void apply_redirections(command *com){
	if(com->redirs == NULL){
		return;
	}

	redirseq *first_redir = com->redirs;
	redirseq *cur_redir = com->redirs;

	do{
		redir *r = cur_redir->r;

		int fd;
		if(IS_RIN(r->flags)){ //Input
			fd = open(r->filename, O_RDONLY);
			if(fd < 0){ //If open error 
				file_error_handler(r->filename);
				exit(EXEC_FAILURE);
			}
			dup2(fd, 0);
			close(fd);
		}
		else if(IS_ROUT(r->flags) || IS_RAPPEND(r->flags)){ //Output
			int flags = O_WRONLY | O_CREAT;
			if(IS_ROUT(r->flags)){
				flags |= O_TRUNC;
			}
			else{
				flags |= O_APPEND;
			}
			fd = open(r->filename, flags, 0666);
			if(fd < 0){ //If open error
				file_error_handler(r->filename);
				exit(EXEC_FAILURE);
			}
			dup2(fd, 1);
			close(fd);
		}

		cur_redir = cur_redir->next;
	}while(cur_redir != first_redir);
}

void run_command(command *com){
	int argc = number_of_arguments(com);

	//convert argsq to argv list
	char *argv[argc+1];
	argseq *cur_arg = com->args;

	for(int i = 0; i < argc; i++){
		argv[i] = cur_arg->arg;
		cur_arg = cur_arg->next;
	}
	
	argv[argc] = NULL;

	apply_redirections(com);
	
	execvp(argv[0], argv);
	exec_error_handler(argv[0]);
}

//Return function in builtin_table if found, NULL if not found
typedef int (*builtin_func)(char **);
builtin_func builtin_find(char *command_name) {
    for (int i = 0; builtins_table[i].name != NULL; i++) {
        if (strcmp(command_name, builtins_table[i].name) == 0) {
            return builtins_table[i].fun;  //Return function pointer
        }
    }
    return NULL;  //Not found
}

void builtin_run(command *com, builtin_func func){
	int argc = number_of_arguments(com);

	//convert argsq to argv list
	char *argv[argc+1];
	argseq *cur_arg = com->args;

	for(int i = 0; i < argc; i++){
		argv[i] = cur_arg->arg;
		cur_arg = cur_arg->next;
	}
	
	argv[argc] = NULL;

	int builtin_exec_code = func(argv);
	if(builtin_exec_code == BUILTIN_ERROR){
		fprintf(stderr, "Builtin %s error.\n", argv[0]);
	}
}

//Returns -1 if the pipeline contains NULL command  
int number_of_commands(pipeline *p){
	commandseq *first_command = p->commands;
	commandseq *cur_command = p->commands;

	int n = 0;
	do{
		n++;
		cur_command = cur_command->next;
	}while(cur_command != first_command);

	return n;
}

int does_pipeline_contain_null(pipeline *p){
	commandseq *first_command = p->commands;
	commandseq *cur_command = p->commands;

	do{
		if(cur_command->com == NULL){
			return 1;
		}
		cur_command = cur_command->next;
	}while(cur_command != first_command);

	return 0;
}

void save_process_status(pid_t pid, int status){
	if(child_process_size >= CHILD_PROCESS_STATUS_BUF_SIZE) return;
	child_process_status[child_process_size].pid = pid;
	child_process_status[child_process_size].status = status;
	++child_process_size;
}

int is_foreground_process(pid_t pid){
	for(int i = 0; i < fg_count; i++){
		if(foreground_process[i] == pid){
			foreground_process[i] = foreground_process[fg_count-1];
			return 1;
		}
	}
	return -1;
}

void sigchild_handler(int sig){
	pid_t pid;
	int status;

	while((pid = waitpid(-1, &status, WNOHANG)) > 0){
		if(is_foreground_process(pid) == 1){ //foreground child
			--fg_count;
		}
		else{ //background child
			save_process_status(pid, status);
		}
	}
}

void block(int sig) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, sig);
    sigprocmask(SIG_BLOCK, &mask, NULL);
}

void unblock(int sig) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, sig);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

void run_pipeline(pipeline *p){
	commandseq *commands = p->commands;
	command *com = commands->com;
	int fg = !(p->flags & INBACKGROUND);
	int n = number_of_commands(p);
	if(com == NULL && n == 1){
		return;
	}
	//Check if the command contain a null command | |
	if(does_pipeline_contain_null(p)){
		fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
		return;
	}

	
	//Check if command is builtin command 
	if(n == 1){
		builtin_func func = builtin_find(com->args->arg);
		if(func != NULL){
			builtin_run(com, func);
			return;
		}
	}

	int pipefd[2][2];
	pid_t chpid[n];
	pid_t shell_pgrp = getpgrp();
	int prev_read = -1;

	for(int i = 0; i < n; i++){
		com = commands->com;
		commands = commands->next;

		if(i < n-1){
			pipe(pipefd[i%2]);
		}

		block(SIGCHLD);
		chpid[i] = fork();
		if(chpid[i] == 0){
			unblock(SIGCHLD);
			if(!fg){
				setsid();
			}
			else{
				signal(SIGINT, SIG_DFL);
			}

			if(prev_read != -1){
				dup2(prev_read, 0);
				close(prev_read);
			}

			if(i < n-1){
				dup2(pipefd[i%2][1], 1);
				close(pipefd[i%2][1]);
				close(pipefd[i%2][0]);
			}
			run_command(com);
			exit(EXEC_FAILURE);
		}
		else{
			if(fg){
				foreground_process[fg_count++] = chpid[i];
			}
			unblock(SIGCHLD);

			if(prev_read != -1){
				close(prev_read);
			}

			if(i < n-1){
				close(pipefd[i%2][1]);
				prev_read = pipefd[i%2][0];
			}
		}
	}

	if(fg){
		sigset_t mask;
		sigfillset(&mask); //block all signals
		sigdelset(&mask, SIGCHLD); //unblock sigchild

		block(SIGCHLD); 
		while(fg_count > 0){
			sigsuspend(&mask);
		}
		unblock(SIGCHLD);
	}
}

void process_line(char* newline, const size_t newline_len){
	if(newline_len > MAX_LINE_LENGTH){
		fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
		return;
	}


	// Parse line
	pipelineseq* ln = parseline(newline);
	//If parseline returns NULL write SYNTAX_ERROR_STR
	if(ln == NULL){
		fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
		return;
	}

	pipelineseq *cur_pipeline = ln;

	do{
		run_pipeline(cur_pipeline->pipeline);
		cur_pipeline = cur_pipeline->next;
	}while(cur_pipeline != ln);
}


void print_child_process_status(){
	while(child_process_size > 0){
		--child_process_size;
		process chld = child_process_status[child_process_size];
		if(WIFEXITED(chld.status)){
			printf("Background process %d terminated. (exited with status %d)\n",
                   chld.pid, WEXITSTATUS(chld.status));
		}
		else if(WIFSIGNALED(chld.status)){
			printf("Background process %d terminated. (killed by signal %d)\n",
                   chld.pid, WTERMSIG(chld.status));
		}
	}
}

int 
main(int argc, char *argv[])
{
	struct stat st;
	int print_prompt = 0;
	if (fstat(STDIN_FILENO, &st) == 0 && S_ISCHR(st.st_mode)) {
    	print_prompt = 1;
	}

	char buf[READ_BUF_SIZE];
	ssize_t br; //bytes read 
	size_t prompt_str_len = strlen(PROMPT_STR);
	size_t buf_len = 0; //actual lenght of the data in the buffer 
	size_t remaining_line = 0;

	//Setup SIGCHLD handler
	struct sigaction sa;
	sa.sa_handler = sigchild_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGCHLD, &sa, NULL);

	//Setup SIGINT handler 
	struct sigaction sa_int;
    sa_int.sa_handler = SIG_IGN;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa_int, NULL);

	//Main shell while
	while(1){
		//print child process status
		block(SIGCHLD);
		if(print_prompt){
			print_child_process_status();
		}
		unblock(SIGCHLD);

		//print prompt symbol
		if(print_prompt){
			write(1, PROMPT_STR, prompt_str_len);
		}
		//read input 
		br = read(0, buf+buf_len, READ_BUF_SIZE-buf_len);
		if(br == 0) break; 
		buf_len += br;
		
		char *line_start = buf;
		while(buf_len > 0){
			char *newline = memchr(line_start, '\n', buf_len);
			if(newline != NULL){
				*newline = '\0';
				size_t proccesed = (newline - line_start) + 1;
				process_line(line_start, proccesed+remaining_line-1);

				remaining_line = 0;
				buf_len -= proccesed;
				line_start = newline + 1;
			}
			else{
				//No found '\n' 
				//Move reamining data to the begining of buffer
				if(remaining_line + buf_len <= MAX_LINE_LENGTH){
					memmove(buf, line_start, buf_len);
				}
				else{
					remaining_line += buf_len; 
					buf_len = 0;
				}
				break;
			}
		}
	}

	//process EOF
	if (buf_len > 0) {
        buf[buf_len] = '\0';
        process_line(buf, buf_len+remaining_line);
    }

	return 0;
}
