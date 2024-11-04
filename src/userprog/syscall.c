#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/process.h"

#define WORD_SIZE (sizeof (intptr_t))

static void syscall_handler (struct intr_frame *);
static uint32_t dereference (const void *, int, int);
static struct file *retrieve_fp (int);
static void verify_string (const char *);

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
  int syscall_number = (int) dereference (f->esp, 0, WORD_SIZE);

  switch (syscall_number) {
    case SYS_HALT: halt (); break; 
    case SYS_EXIT: exit (f->esp); break;   
    case SYS_EXEC: f->eax = exec (f->esp); break;
    case SYS_WAIT: f->eax = wait (f->esp); break;
    case SYS_CREATE: f->eax = create (f->esp); break;
    case SYS_REMOVE: f->eax = remove (f->esp); break;
    case SYS_OPEN: f->eax = open (f->esp); break;
    case SYS_FILESIZE: f->eax = filesize (f->esp); break;
    case SYS_READ: f->eax = read (f->esp); break;
    case SYS_WRITE: f->eax = write (f->esp); break;  
    case SYS_SEEK: seek (f->esp); break;   
    case SYS_TELL: f->eax = tell (f->esp); break;   
    case SYS_CLOSE: close (f->esp); break;
  }
}

/* Dereference pointer BASE + OFFSET * INDEX, with a validity test. Returns
   4 byte chunk starting from BASE + OFFSET * INDEX if it passes the test. 
   Else, terminates current process. */
static uint32_t
dereference (const void *base, int offset, int index)
{
  const uint8_t *base_ = base;
  void *uaddr = (void *) (base_ + offset * index);

  if (is_user_vaddr (uaddr))
    return *((uint32_t *) uaddr);
  else
    process_exit (-1);

  NOT_REACHED ();
}

/* Retrieve file pointer from file descriptor, FD. Returns NULL if it has
   failed to find a file with file descriptor of FD among current process's 
   opened files. */
static struct file *
retrieve_fp (int fd)
{
  struct list *opened = &thread_current ()->opened;
  struct file *fp = NULL;
  struct list_elem *e;

  for (e = list_begin (opened); e != list_end (opened); e = list_next (e))
    {
      fp = list_entry (e, struct file, elem);
      if (fp->fd == fd)
        break;
    }
  
  if (fp == NULL || fp->fd != fd)
    return NULL;
  
  return fp;
}

/* Verifies null-terminated STR by dereferencing each character in STR until
   it reaches null character. If succeed, it would normally return. Else, it
   would cause page fault and eventually terminate faulting user process. */
static void
verify_string (const char *str)
{
  for (int i = 0; (0xFF & dereference ((void *) str, i, 1)) != '\0';
       i++);
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
  int status = (int) dereference (esp, 1, WORD_SIZE);
  process_exit (status);
}

/* System call handler for exec(). */
static uint32_t
exec (void *esp)
{
  char *file = (char *) dereference (esp, 1, WORD_SIZE);
  verify_string (file);

  return (uint32_t) process_execute (file);
}

/* System call handler for wait(). */
static uint32_t
wait (void *esp)
{
  tid_t tid = (tid_t) dereference (esp, 1, WORD_SIZE);
  return (uint32_t) process_wait (tid);
}

/* System call handler for create(). */
static uint32_t
create (void *esp)
{
  char *file = (char *) dereference (esp, 1, WORD_SIZE);
  unsigned initial_size = dereference (esp, 2, WORD_SIZE);  
  verify_string (file);

  return (uint32_t) filesys_create (file, initial_size);
}

/* System call handler for remove(). */
static uint32_t
remove (void *esp)
{
  char *file = (char *) dereference (esp, 1, WORD_SIZE);
  verify_string (file);

  return (uint32_t) filesys_remove (file);
}

/* System call handler for open(). */
static uint32_t
open (void *esp)
{
  char *file = (char *) dereference (esp, 1, WORD_SIZE);
  verify_string (file);
  struct file *fp = filesys_open (file);

  if (fp == NULL)
    return (uint32_t) FD_ERROR;

  return (uint32_t) fp->fd;
}

/* System call handler for filesize(). Return -1 if given file descriptor is 
   not a valid file descriptor. */
static uint32_t 
filesize (void *esp)
{
  int fd = (int) dereference (esp, 1, WORD_SIZE);
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
  int fd = (int) dereference (esp, 1, WORD_SIZE);
  void *buffer = (void *) dereference (esp, 2, WORD_SIZE);
  unsigned pos = 0, size = dereference (esp, 3, WORD_SIZE);
  struct file *fp = retrieve_fp (fd);

  /* Checks if all addresses of BUFFER within SIZE is in user address space. */
  dereference (buffer, size, 1);

  if (fd == STDIN_FILENO)
    {
      while (pos < size)
        ((char *) buffer)[pos++] = input_getc ();
      return (uint32_t) size;
    }

  if (fp == NULL)
    return (uint32_t) -1;

  return (uint32_t) file_read (fp, buffer, size);
}

/* System call handler for write(). Returns -1 if given file descriptor is not 
   a valid file descriptor. */
static uint32_t
write (void *esp)
{
  int fd = (int) dereference (esp, 1, WORD_SIZE);
  void *buffer = (void *) dereference (esp, 2, WORD_SIZE);
  unsigned size = dereference (esp, 3, WORD_SIZE);
  struct file *fp = retrieve_fp (fd);

  if (fd == STDOUT_FILENO)
    {
      putbuf ((char *) buffer, size);
      return (uint32_t) size;
    }

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
  int fd = (int) dereference (esp, 1, WORD_SIZE);
  unsigned position = dereference (esp, 2, WORD_SIZE);
  struct file *fp = retrieve_fp (fd);

  if (fp == NULL)
    return;

  file_seek (fp, position);
}

/* System call handler for seek(). Returns -1 if given file descriptor is not 
   a valid file descriptor. */
static uint32_t
tell (void *esp)
{
  int fd = (int) dereference (esp, 1, WORD_SIZE);
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
  int fd = (int) dereference (esp, 1, WORD_SIZE);
  struct file *fp = retrieve_fp (fd);

  file_close (fp);
}