#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>


#define FALSE 0
#define TRUE 1
#define INPUT_STRING_SIZE 80

#include "io.h"
#include "parse.h"
#include "process.h"
#include "shell.h"


void get_paths(char** paths);
void flush_stdin(void);

int cmd_quit(tok_t arg[]) {
  printf("Bye\n");
  exit(0);
  return 1;
}

int cmd_help(tok_t arg[]);
int cmd_cd(tok_t agr[]);
int cmd_pwd(tok_t arg[]);
int cmd_wait(tok_t arg[]);
int cmd_fg(tok_t arg[]);
int cmd_bg(tok_t arg[]);

int child_process_count = 0;
pid_t most_recent_process;

/* Command Lookup table */
typedef int cmd_fun_t (tok_t args[]); /* cmd functions take token array and return int */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_quit, "quit", "quit the command shell"},
	{cmd_cd, "cd", "change the directory"},
    {cmd_pwd, "pwd", "display current directory"},
    {cmd_wait, "wait", "wait for child processes to be finished"},
    {cmd_fg, "fg", "put process to foreground"},
    {cmd_bg, "bg", "put process to background"},
};

int cmd_fg(tok_t arg[]) {
    pid_t target;

    target = arg[1] != NULL ? (pid_t) *arg[1] : most_recent_process;
    printf("Set pid: %d to foreground...\n", target);
    tcsetpgrp(0, target);
    // And resume the process if it's stopped
    kill(target, SIGCONT);
    return 1;
}

int cmd_bg(tok_t arg[]) {
    pid_t shell_pgid;

    shell_pgid = getpgrp();
    tcsetpgrp(shell_terminal, shell_pgid);
    return 1;
}

int cmd_wait(tok_t arg[]) {
    while(child_process_count != 0) {
        waitpid(-1, NULL, 0);
        child_process_count--;
    }
    flush_stdin();
    return 1;
}

int cmd_help(tok_t arg[]) {
  int i;
  for (i=0; i < (sizeof(cmd_table)/sizeof(fun_desc_t)); i++) {
    printf("%s - %s\n",cmd_table[i].cmd, cmd_table[i].doc);
  }
  return 1;
}

int cmd_pwd(tok_t arg[]) {
    char buff[1024];

    if (getcwd(buff, sizeof(buff)) != NULL)
        printf("%s\n", buff);
    else
        perror("getcwd() error\n");
    return 1;
}

int cmd_cd(tok_t arg[]){
    if(chdir(arg[1]) == 0) {
        cmd_pwd(arg);
    }
    else {
        perror("chdir() error");
    }
    return 1;
}
int lookup(char cmd[]) {
  int i;
  for (i=0; i < (sizeof(cmd_table)/sizeof(fun_desc_t)); i++) {
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0)) return i;
  }
  return -1;
}

void init_shell()
{
    // set most recent process id to shell id
    most_recent_process = getpid();
  /* Check if we are running interactively */
  shell_terminal = STDIN_FILENO;

  /** Note that we cannot take control of the terminal if the shell
      is not interactive */
  shell_is_interactive = isatty(shell_terminal);

  if(shell_is_interactive){

    /* force into foreground */
    while(tcgetpgrp (shell_terminal) != (shell_pgid = getpgrp()))
      kill( - shell_pgid, SIGTTIN);

    shell_pgid = getpid();
    /* Put shell in its own process group */
    if(setpgid(shell_pgid, shell_pgid) < 0){
      perror("Couldn't put the shell in its own process group");
      exit(1);
    }

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);
    tcgetattr(shell_terminal, &shell_tmodes);
  }
  // assign signal handler to shell
  struct sigaction psac;
  void psac_handler(int i) {
    printf("TESTING... SIGINT => %d\n", i);
  }
  psac.sa_handler = psac_handler;
  printf("sigaction result %d\n", sigaction(SIGINT, &psac, NULL));

  /** YOUR CODE HERE */
	//signal(SIGINT,SIG_IGN);
	//signal(SIGQUIT,SIG_IGN);
	//signal(SIGTSTP,SIG_IGN);
	//signal(SIGTTIN,SIG_IGN);
	//signal(SIGTTOU,SIG_IGN);
	//signal(SIGCHLD,SIG_IGN);
}

/**
 * Add a process to our process list
 */
void add_process(process* p)
{
  /** YOUR CODE HERE */
}

/**
 * Creates a process given the inputString from stdin
 */
process* create_process(char* inputString)
{
  /** YOUR CODE HERE */
  return NULL;
}

void shift_string(char *str[],int i){
	while(str[i]){
		str[i] = str[i+1];
		i++;
	}
}

int shell (int argc, char *argv[]) {
  char *s = malloc(INPUT_STRING_SIZE+1);			/* user input string */
  int signals[] = {SIGINT, SIGQUIT, SIGKILL, SIGTERM, SIGTSTP, SIGCONT,
    SIGTTIN, SIGTTOU, -1};
  char cwd[1024];
  tok_t *t;			/* tokens parsed from input */
  int lineNum = 0;
  int fundex = -1;
  int status;
  int i;
  pid_t pid = getpid();		/* get current processes PID */
  pid_t ppid = getppid();	/* get parents PID */
  pid_t cpid;
  process* first_process = NULL;
  char* paths[256];
  char buf[256];
  tok_t original_t;
  tok_t* args;

  get_paths(paths);
  int pfd = -1;
  int inpfd = -1;
  init_shell();

  printf("%s running as PID %d under %d\n",argv[0],pid,ppid);

  lineNum=0;
  fprintf(stdout, "%d: ", lineNum++);
	if(getcwd(cwd,sizeof(cwd))){
		fprintf(stdout,"%s ",cwd);
	}
  while ((s = freadln(stdin))){
        status = 0;
		process* process_ptr = first_process;
		if(process_ptr){
			while(process_ptr->next){
				process_ptr = process_ptr->next;
			}
			process_ptr->next = (process*)malloc(sizeof(process));
			process_ptr = process_ptr->next;
		}
		else{
			first_process = (process*)malloc(sizeof(process));
			process_ptr = first_process;
		}

        t = getToks(s); /* break the line into tokens */
        fundex = lookup(t[0]); /* Is first token a shell literal */
        
        // allocate args
        args = malloc(MAXTOKS * sizeof(tok_t));
        for (i = 0; i < MAXTOKS; i++)
            args[i] = NULL;
        // seprate std redirect and args
        for(i = 0; t[i] != NULL && strcmp(t[i], "<") != 0 && strcmp(t[i], ">") != 0; i++) {
            args[i] = t[i];
        }
        args[i] = NULL;
        // Write to file
        if (t[i] != NULL && t[i + 1] != NULL && strcmp(t[i], ">") == 0) {
            pfd = open(t[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (pfd == -1) {
                fprintf(stderr, "Fail to open file: %s\n", t[i + 1]);
            }
            i += 2;
        }
        // Read from file
        else if (t[i] != NULL && t[i + 1] != NULL && strcmp(t[i], "<") == 0) {
            inpfd = open(t[i + 1], O_RDONLY, 0666);
            if (inpfd == -1){
                fprintf(stderr, "Fail to open file: %s\n", t[i + 1]);
            }
            i += 2;
        }
        // if t[i] is NULL, back to index that t[i] is not NULL
        if (t[i] == NULL && i > 0)
            i--;

        int run_in_background = 0;
        // Check & sign (run program in background)
        if (t[i] != NULL && strcmp(t[i], "&") == 0)
            run_in_background = 1;
        
        // ignore all signals
        for (i = 0 ; signals[i] != -1 ; i++) {
            signal(signals[i], SIG_IGN);
        }
        
        if (fundex != -1) {
            cmd_table[fundex].fun(t);        
        }
        else if (t[0] != NULL) {
            cpid = fork();
            most_recent_process = cpid;
            // Child process
            if (cpid == 0) {
                cpid = getpid();
                printf("Child %d start running...\n", cpid);
                // reset child process pgid
                setpgid(0, cpid);
                // set child process to foreground
                if (!run_in_background)
                    tcsetpgrp(0, cpid);
                
                // catch all signals
                for (i = 0 ; signals[i] != -1; i++)
                    signal(signals[i], SIG_DFL);

                int exit_num = 0;

                if (pfd != -1) {
                    close(1);
                    dup(pfd);
                }
                if (inpfd != -1) {
                    dup2(inpfd, STDIN_FILENO);
                }
                original_t = t[0];
                for (i = 0 ; strcmp(paths[i], "\0") != 0 ; i++) {
                    snprintf(buf, sizeof(buf), "%s/%s", paths[i], original_t);
                    t[0] = buf;
                    if (execv(buf, args) == 0)
                        exit(0);
                }
                t[0] = original_t;
                if (execv(t[0], args)){
                    exit_num = -1;
                }
                perror("execv error\n");
                if (pfd != -1)
                    close(pfd);
                if (inpfd != -1)
                    close(inpfd);
                exit(exit_num);
            }
            // Parent process
            else {
                child_process_count++;
                printf("parent pgid: %d\n", getpgrp());
                // wait if program not run in background (without & token)
                wait(&status);
                flush_stdin();
            }
        }

        // set shell to foreground
        tcsetpgrp(0, getpgrp());

        // catch all signals again
        for (i = 0 ; signals[i] != -1; i++) {
            struct sigaction sigact;

            sigact.sa_handler = SIG_DFL;
            sigaction(signals[i], &sigact, NULL);
        }

        fprintf(stdout, "%d: ", lineNum++);
	    if(getcwd(cwd, sizeof(cwd))){
	    	fprintf(stdout,"%s ",cwd);
	    }
        pfd = -1;
        inpfd = -1;
    }
  return 0;
}

void get_paths(char** paths){
    char* path_string;
    char c;
    int i = 0, j = 0, k = 0;
    char* p = (char*) malloc(sizeof(char) * 256);

    path_string = getenv("PATH");
    if (path_string != NULL) {
        while((c = path_string[i]) != '\0') {
            if (c != ':') {
                p[j] = c;
                j++;
            }
            else {
                p[j] = '\0';
                *(paths + (k++)) = p;
                p = (char*) malloc(sizeof(char) * 256);
                j = 0;
            }
            i++;
        }
    }
    *(paths + (k++)) = p;
    *(paths + k) = "\0";
}

void flush_stdin(void) {
    int c;
    while((c = getchar()) != EOF);
}
