#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

struct start_info {
    char *cmd;
    bool success;
    struct semaphore sema;
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_init (void);
void process_activate (void);
void pexit (int status);
void get_filefd_from_fd(int fd, struct file_fd **ffdp);

#endif /* userprog/process.h */
