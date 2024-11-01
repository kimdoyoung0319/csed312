#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <syscall.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);
static int dereference (const int *, int);
static void halt (void);
static void exit (void *);
static uint32_t exec (void *);
static uint32_t wait (void *);
static uint32_t create (void *);
static uint32_t remove (void *);
static uint32_t open (void *);
static uint32_t filesize (void *);
static uint32_t read (void *);
static uint32_t write (void *);
static void seek (void *);
static uint32_t tell (void *);
static void close (void *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  uint32_t retval;
  int syscall_number = dereference ((int *) f->esp, 0);

  switch (syscall_number) {
    case SYS_HALT: halt (); break; 
    case SYS_EXIT: exit (f->esp); break;   
    case SYS_EXEC: retval = exec (f->esp); break;
    case SYS_WAIT: retval = wait (f->esp); break;
    case SYS_CREATE: retval = create (f->esp); break;
    case SYS_REMOVE: retval = remove (f->esp); break;
    case SYS_OPEN: retval = open (f->esp); break;
    case SYS_FILESIZE: retval = filesize (f->esp); break;
    case SYS_READ: retval = read (f->esp); break;
    case SYS_WRITE: retval = write (f->esp); break;  
    case SYS_SEEK: seek (f->esp); break;   
    case SYS_TELL: retval = tell (f->esp); break;   
    case SYS_CLOSE: close (f->esp); break;
  }

  f->eax = retval;
}

/* Dereference pointer UADDR + OFFSET, with a validity test. Returns
   UADDR[OFFSET] if UADDR + OFFSET passes the test. Else, terminates current
   process. */
static int
dereference (const int *uaddr, int offset)
{
  if (is_user_vaddr ((void *) (uaddr + offset)))
    return uaddr[offset];
  else
    /* TODO: What happens to acquired locks when exiting due to invalid 
             address? */
    exit (NULL);
}

/* System call handler for halt(). */
static void
halt (void)
{
  shutdown_power_off ();  
}

/* System call handler for exit(). If ESP is a null pointer, it automatically
   sets the exit status passed to the parent thread as -1. */
static void
exit (void *esp)
{
  struct list_elem *e;
  struct thread *cur = thread_current (), *par = cur->parent;

  /* This is for maintain consistency of thread structures. Interrupt will be 
     enabled after thread_exit() call which causes context switch. */
  intr_disable ();

  if (esp == NULL)
    par->status = -1;
  else 
    par->status = dereference ((int *) esp, 1);
  
  for (e = list_begin (&cur->children); e != list_end (&cur->children);
       e = list_next (e))
    list_entry (e, struct thread, childelem)->orphaned = true;

  if (cur->waited)
    thread_unblock (par);

  list_remove (&cur->childelem);
  thread_exit ();
}

/* System call handler for exec(). */
static uint32_t
exec (void *esp)
{
  char *file = dereference ((int *) esp, 1);

  /* Checks if all addresses for characters of FILE is valid. This may not be 
     needed in order to pass the test. If so, just omit below. */
  for (int i = 0; dereference ((int *) file, i) != '\0'; i++);

  return (uint32_t) process_execute (file);
}

/* System call handler for wait(). */
static uint32_t
wait (void *esp)
{
  pid_t pid = (pid_t) dereference ((int *) esp, 1);
  return (uint32_t) process_wait (pid);
}

static uint32_t
create (void *esp)
{
}