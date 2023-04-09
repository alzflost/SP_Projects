/* $begin shellmain */
// #include "csapp.h"
#include"myshell.h"
// #include<errno.h>
// #include<string.h>
// #include<stdlib.h>
// #define MAXARGS   128

/* Function prototypes */
// void eval(char *cmdline);
// int parseline(char *buf, char **argv);
// int builtin_command(char **argv); 
// void pipe_eval(char *cmdline);
// void unix_error(char *msg);
// pid_t Fork(void);
// int Dup2(int fd1, int fd2);

int pipe_n = 0;
int total_com = 0;
int cur_fd = 0;
int fd[2];

int main() 
{
    char cmdline[MAXLINE]; /* Command line */

    while (1) {
	/* Read */
	printf("CSE4100-SP-P#1> ");                   
	fgets(cmdline, MAXLINE, stdin); 
	if (feof(stdin))
	    exit(0);
	for (int i=0; i<strlen(cmdline); i++){
	    if (cmdline[i] == '|'){
		pipe_n++;
	    }
	}
	total_com = pipe_n + 1;
	if (pipe_n) pipe_eval(cmdline);
	else eval(cmdline);
	/* Evaluate */
    } 
}
/* $end shellmain */
  
void pipe_eval(char *cmdline){
    //
    pipe_n++;
    char *next_command = strtok(cmdline, "|");
    while (pipe_n > 0){
	eval(next_command);
	pipe_n--;
	if (!pipe_n) break;
        next_command = strtok(NULL, "|");
    }
    total_com = 0;
    pipe_n = 0;
    cur_fd = 0;
}
/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    strcpy(buf, cmdline);
    bg = parseline(buf, argv); 
    if (argv[0] == NULL)
	return;   /* Ignore empty lines */
    if (!builtin_command(argv)) {
	if (strcmp(argv[0], "cd")==0){
	    chdir(argv[1]);
	}
	else {
	    if (total_com >= 2) pipe(fd);
	    if ((pid = Fork())  == 0){
		if (pipe_n > 0 && pipe_n != total_com) Dup2(cur_fd, STDIN_FILENO);
		if (pipe_n > 1) Dup2(fd[1], STDOUT_FILENO);
                if (execvp(argv[0], argv) < 0) {
                    printf("%s: Command not found.\n", argv[0]);
                    exit(0);
	        }
            }
	    else {
		int status;
		if (waitpid(pid, &status, 0) < 0)
		    unix_error("waitfg: waitpid error");
	    }
	    if(pipe_n) {
		cur_fd = fd[0];
		close(fd[1]);
	    }
	}
    }
    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
    if (!strcmp(argv[0], "quit")) /* quit command */
	exit(0);
    if (!strcmp(argv[0], "exit")) exit(0); 
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
	return 1;
    return 0;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv)
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */
    int quote = 0;
    if (buf[strlen(buf)-1] != '\n') {
	strcat(buf, "\n");
    }
    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
	if(!quote) argv[argc++] = buf;
	char* temp = buf;
	while (temp != delim) {
	    if (*temp == '\'' || *temp == '\"'){
		quote = 1 - quote;
		strcpy(temp, temp+1);
		temp--;
		delim--;
	    }
	    temp++;
	}
	if(!quote) *delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
	return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
	argv[--argc] = NULL;

    return bg;
}
/* $end parseline */

void unix_error(char *msg){
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}

pid_t Fork(void){
    pid_t pid;

    if((pid = fork())<0)
	unix_error("Fork error");
}

int Dup2(int fd1, int fd2){
    int rc;
    if ((rc = dup2(fd1, fd2))<0)
	unix_error("Dup2 error");
    return rc;
}
