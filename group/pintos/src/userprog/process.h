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
void process_activate (void);
void pexit (int status);

#endif /* userprog/process.h */
