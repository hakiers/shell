#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>

#include "builtins.h"

int echo(char*[]);
int undefined(char*[]);
int lexit(char*[]);
int lcd(char*[]);
int lkill(char*[]);
int lls(char*[]);

builtin_pair builtins_table[]={
	{"exit",	&lexit},
	{"lecho",	&echo},
	{"lcd",		&lcd},
	{"lkill",	&lkill},
	{"lls",		&lls},
	{NULL,NULL}
};

int 
count_args(char * argv[]){
	int i = 0;
	while(argv[i] != NULL){
		i++;
	}
	return i;
}

int 
parse_long(const char *arg, long *out){
	char *end;
	errno = 0;
    long val = strtol(arg, &end, 10);
    if (*end != '\0' || errno == ERANGE){
		return -1;
	}
    *out = val;
    return 0;
}

int 
echo(char * argv[])
{
	int i =1;
	if (argv[i]) printf("%s", argv[i++]);
	while  (argv[i])
		printf(" %s", argv[i++]);

	printf("\n");
	fflush(stdout);
	return 0;
}

int 
undefined(char * argv[])
{
	fprintf(stderr, "Command %s undefined.\n", argv[0]);
	return BUILTIN_ERROR;
}

int 
lexit(char * argv[]){
	int argc = count_args(argv);

	//To many arguments
	if(argc > 2){ 
		return BUILTIN_ERROR;
	} 
	
	int status = 0;
	if(argc == 2){
		long val;
		//If not valid number
		if(parse_long(argv[1], &val) != 0){
			return BUILTIN_ERROR;
		}
		status = (int)val;
	}

	exit(status);
}

int 
lcd(char * argv[]){
	int argc = count_args(argv);

	//To many arguments
	if(argc > 2){
		return BUILTIN_ERROR;
	} 
	
	char* target;
	if(argc == 1){
		target = getenv("HOME");
	}
	else{
		target = argv[1];
	}

	if(chdir(target) == -1){
		return BUILTIN_ERROR;
	}
	
	return 0;
}


int 
lkill(char * argv[]){
	int argc = count_args(argv);

	if(argc == 1 || argc > 3){
		return BUILTIN_ERROR;
	}

	pid_t pid;
	int sig = 15;

	if(argc == 3){
		long val;
		//If wrong signal signature or not valid number
		if(argv[1][0] != '-' || parse_long(argv[1]+1, &val)){
			return BUILTIN_ERROR;
		}
		if(val > INT_MAX || val < INT_MIN){
			return BUILTIN_ERROR;
		}
		sig = (int)val;
		//If not valid number
		if(parse_long(argv[2], &val)){
			return BUILTIN_ERROR;
		}
		pid = (pid_t)val;
	}
	else if(argc == 2){
		long val;
		//If not valid number
		if(parse_long(argv[1], &val)){
			return BUILTIN_ERROR;
		}
		pid = (pid_t)val;
	}


	if(kill(pid, sig) != 0){
		return BUILTIN_ERROR;
	}

	return 0;
}

int
lls(char * argv[]){
	int argc = count_args(argv);

	if(argc > 1){
		return BUILTIN_ERROR;
	}

	const char *path = ".";
	DIR *dir = opendir(path);
	if(dir == NULL){
		return BUILTIN_ERROR;
	}

	struct dirent *read = readdir(dir);

	while(read != NULL){
		if(read->d_name[0] != '.'){
			fprintf(stdout, "%s\n", read->d_name);
		}
		read = readdir(dir);
	}

	closedir(dir);
	fflush(stdout);
	return 0;
}
