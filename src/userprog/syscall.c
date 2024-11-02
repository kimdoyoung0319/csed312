#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);
static int dereference (const uint8_t *, int, int);
static struct file *retrieve_fp (int);

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
  int syscall_number = dereference ((uint8_t *) f->esp, 0, 4);

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

/* Dereference pointer BASE + OFFSET * INDEX, with a validity test. Returns
   int-sized chunk starting from BASE + OFFSET * INDEX if it passes the test. 
   Else, terminates current process. */
static int
dereference (const uint8_t *base, int offset, int index)
{
  void *uaddr = (void *) (base + offset * index);

  if (is_user_vaddr (uaddr))
    return *((int *) uaddr);
  else
    process_exit (-1);
}

/* Retrieve file pointer from file descriptor, FD. Returns NULL if it has
   failed to find a file with file descriptor of FD among current process's 
   opened files. */
static struct file *
retrieve_fp (int fd)
{
  struct list *opened = &thread_current ()->opened;
  struct list_elem *e = list_begin (opened);
  struct file *fp = NULL;

  while (e != list_end (opened) && fp->fd != fd)
    {
      fp = list_entry (e, struct file, elem); 
      e = list_next (e);
    }
  
  if (fp == NULL || fp->fd != fd)
    return NULL;
  
  return fp;
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
  int status = dereference (esp, 1, 4);
  process_exit (status);
}

/* System call handler for exec(). */
static uint32_t
exec (void *esp)
{
  char *file = (char *) dereference (esp, 1, 4);

  /* Checks if all addresses for characters of FILE is valid. This may not be 
     needed in order to pass the test. If so, just omit below. */
  for (int i = 0; 0xFF & dereference (file, i, 1) != '\0'; i++);

  return (uint32_t) process_execute (file);
}

/* System call handler for wait(). */
static uint32_t
wait (void *esp)
{
  pid_t pid = (pid_t) dereference (esp, 1, 4);
  return (uint32_t) process_wait (pid);
}

/* System call handler for create(). */
static uint32_t
create (void *esp)
{
  char *file = (char *) dereference (esp, 1, 4);
  unsigned initial_size = (unsigned) dereference (esp, 2, 4);  

  /* Checks if all addresses for characters of FILE is valid. This may not be 
     needed in order to pass the test. If so, just omit below. */
  for (int i = 0; 0xFF & dereference (file, i, 1) != '\0'; i++);

  return (uint32_t) filesys_create (file, initial_size);
}

/* System call handler for remove(). */
static uint32_t
remove (void *esp)
{
  char *file = (char *) dereference (esp, 1, 4);

  /* Checks if all addresses for characters of FILE is valid. This may not be 
     needed in order to pass the test. If so, just omit below. */
  for (int i = 0; 0xFF & dereference (file, i, 1) != '\0'; i++);

  return (uint32_t) filesys_remove (file);
}

/* System call handler for open(). */
static uint32_t
open (void *esp)
{
  char *file = (char *) dereference (esp, 1, 4);

  /* Checks if all addresses for characters of FILE is valid. This may not be 
     needed in order to pass the test. If so, just omit below. */
  for (int i = 0; 0xFF & dereference (file, i, 1) != '\0'; i++);

  return (uint32_t) filesys_open (file)->fd;
}

/* System call handler for filesize(). Return -1 if given file descriptor is 
   not a valid file descriptor. */
static uint32_t 
filesize (void *esp)
{
  int fd = dereference (esp, 1, 4);
  struct file *fp = retrieve_fp (fd);

  if (fp == NULL)
    return (uint32_t) -1;

  return (uint32_t) file_length (fp);
}

/* System call handler for read(). Returns -1 if given file descriptor is not 
   a valid file descriptor. */
static uint32_t
read (void *esp)
{
  int fd = dereference (esp, 1, 4);
  void *buffer = dereference (esp, 2, 4);
  unsigned size = dereference (esp, 3, 4);
  struct file *fp = retrieve_fp (fd);

  if (fp == NULL)
    return (uint32_t) -1;
  
  /* Checks if all addresses of BUFFER within SIZE is in user address space. */
  dereference (buffer, size, 1);

  return (uint32_t) file_read (fp, buffer, size);
}

/* System call handler for write(). Returns -1 if given file descriptor is not 
   a valid file descriptor. */
static uint32_t
write (void *esp)
{
  int fd = dereference (esp, 1, 4);
  const void *buffer = dereference (esp, 2, 4);
  unsigned size = dereference (esp, 3, 4);
  struct file *fp = retrieve_fp (fd);

  if (fp == NULL)
    return (uint32_t) -1;

  /* Checks if all addresses of BUFFER within SIZE is in user address space. */
  dereference (buffer, size, 1);

  return (uint32_t) file_write (fp, buffer, size);
}

/* System call handler for seek(). Returns -1 if given file descriptor is not 
   a valid file descriptor. */
static void
seek (void *esp)
{
  int fd = dereference (esp, 1, 4);
  unsigned position = dereference (esp, 2, 4);
  struct file *fp = retrieve_fp (fd);

  if (fp == NULL)
    return (uint32_t) -1;

  file_seek (fp, position);
}

/* System call handler for seek(). Returns -1 if given file descriptor is not 
   a valid file descriptor. */
static uint32_t
tell (void *esp)
{
  int fd = dereference (esp, 1, 4);
  struct file *fp = retrieve_fp (fd);

  if (fp == NULL)
    return (uint32_t) -1;

  return (uint32_t) file_tell (fp);
}

/* System call handler for close(). Does notihing if given file descriptor is 
   not a valid file descriptor. */
static void
close (void *esp)
{
  int fd = dereference (esp, 1, 4);
  struct file *fp = retrieve_fp (fd);

  file_close (fp);
}