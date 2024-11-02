#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

/* Process identifier. */
typedef int pid_t;
#define PID_ERROR ((pid_t) -1)

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_destory (void);
void process_activate (void);
void process_exit (int);

#endif /* userprog/process.h */
