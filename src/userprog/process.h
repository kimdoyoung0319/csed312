#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include <hash.h>
#include "threads/thread.h"
#include "userprog/pid.h"

/* States in a process's life cycle. An orphaned process is responsible to
   destory itself. For alive processes, their parent is responsible to destroy
   them. */
enum process_state
  {
    PROCESS_DEAD,           /* Dead process with its exit status. */
    PROCESS_ALIVE           /* Alive process. */
  };

/* An user process. */
struct process
  {
    /* Owned by process.c. */
    pid_t pid;                    /* Process identifier. */
    int status;                   /* Exit status. */
    bool waited;                  /* Is its parent waiting on this? */
    char name[16];                /* Name. */
    enum process_state state;     /* Process state. */
    uint32_t *pagedir;            /* Page directory. */
    struct semaphore sema;        /* Semaphore to order parent and child. */
    struct thread *thread;        /* Thread of this process.*/
    struct thread *parent;        /* Parent thread. */
    struct list children;         /* List of child processes. */
    struct list_elem elem;        /* List element. */

    /* Shared between filesys/file.c and process.c.*/
    struct list opened;           /* List of opened files. */

    /* Shared between vm/spt.c and exceptions.c. */
    struct hash spt;

    /* Using for stack pointer when page fault while handling systemcalls */
    void *uesp;
  };

void process_init (void);
pid_t process_execute (const char *file_name);
int process_wait (pid_t);
void process_activate (void);
void process_exit (int);
bool process_install_page (void *, void *, bool);

#endif /* userprog/process.h */
