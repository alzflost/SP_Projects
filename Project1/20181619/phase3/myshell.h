#include<errno.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<signal.h>
#define MAXARGS 128
#define MAXLINE 8192

void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);
void pipe_eval(char *cmdline);
void unix_error(char *msg);
pid_t Fork(void);
int Dup2(int fd1, int fd2);
