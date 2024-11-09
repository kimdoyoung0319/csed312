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

/* Lock to ensure consistency of the file system. */
struct lock filesys_lock;

static void syscall_handler (struct intr_frame *);
static uint32_t dereference (const void *, int, int);
static struct file *retrieve_fp (int);
static bool verify_string (const char *);
static bool verify_read (char *, int);
static bool verify_write (char *, int);
static int get_user (const uint8_t *);
static bool put_user (uint8_t *, uint8_t);

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
  lock_init (&filesys_lock);
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
   opened files. This must be called within user process context. */
static struct file *
retrieve_fp (int fd)
{
  struct process *this = thread_current ()->process;
  struct file *fp = NULL;
  struct list_elem *e;

  ASSERT (this != NULL);

  for (e = list_begin (&this->opened); e != list_end (&this->opened); 
       e = list_next (e))
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
   it reaches null character. Return true if and only if all characters in STR
   is valid. */
static bool
verify_string (const char *str_)
{
  char ch;
  uint8_t *str = (uint8_t *) str_;
  for (int i = 0; ; i++)
    {
      if (!is_user_vaddr (str + i) || (ch = get_user (str + i)) == -1)
        return false;
      
      if (ch == '\0')
        break;
    }

  return true;
}

/* Verifies read buffer BUF whose size is SIZE by trying to read a character
   on each bytes of BUF. Return true if and only if all bytes in BUF is 
   readable. */
static bool
verify_read (char *buf_, int size)
{ 
  uint8_t *buf = (uint8_t *) buf_;
  for (int i = 0; i < size; i++)
      if (!is_user_vaddr (buf + i) && (get_user (buf + i) == -1))
        return false;
  
  return true;
}

/* Verifies write buffer BUF whose size is SIZE by trying to put a character
   on each bytes of BUF. Return true if and only if all bytes in BUF is 
   writable. This function fills 0 on BUF. */
static bool
verify_write (char *buf_, int size)
{
  uint8_t *buf = (uint8_t *) buf_;
  for (int i = 0; i < size; i++)
      if (!is_user_vaddr (buf + i) && put_user (buf + i, 0))
        return false;
  
  return true;
}

/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}

/* System call handler for halt(). */
static void
halt (void)
{
  shutdown_power_off ();  
}

/* System call handler for exit(). */
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
  char *cmd_line = (char *) dereference (esp, 1, WORD_SIZE);
  if (!verify_string (cmd_line))
    return (uint32_t) TID_ERROR;

  return (uint32_t) process_execute (cmd_line);
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
  uint32_t retval;
  char *file = (char *) dereference (esp, 1, WORD_SIZE);
  unsigned initial_size = dereference (esp, 2, WORD_SIZE);  

  if (!verify_string (file))
    return (uint32_t) false;

  lock_acquire (&filesys_lock);
  retval = (uint32_t) filesys_create (file, initial_size);
  lock_release (&filesys_lock);

  return retval;
}

/* System call handler for remove(). */
static uint32_t
remove (void *esp)
{
  uint32_t retval;
  char *file = (char *) dereference (esp, 1, WORD_SIZE);

  if (!verify_string (file))
    return (uint32_t) false;

  lock_acquire (&filesys_lock);
  retval = (uint32_t) filesys_remove (file);
  lock_release (&filesys_lock);

  return retval;
}

/* System call handler for open(). */
static uint32_t
open (void *esp)
{
  struct file *fp;
  char *file = (char *) dereference (esp, 1, WORD_SIZE);

  lock_acquire (&filesys_lock);
  if (!verify_string (file) || (fp = filesys_open (file)) == NULL)
    {
      lock_release (&filesys_lock);
      return (uint32_t) FD_ERROR;
    }
  lock_release (&filesys_lock);

  return (uint32_t) fp->fd;
}

/* System call handler for filesize(). Return -1 if given file descriptor is 
   not a valid file descriptor. */
static uint32_t 
filesize (void *esp)
{
  uint32_t retval;
  int fd = (int) dereference (esp, 1, WORD_SIZE);
  struct file *fp = retrieve_fp (fd);

  if (fp == NULL)
    return (uint32_t) -1;

  lock_acquire (&filesys_lock);
  retval = (uint32_t) file_length (fp);
  lock_release (&filesys_lock);

  return retval;
}

/* System call handler for read(). Returns -1 if given file descriptor is not 
   a valid file descriptor. Kills current process with exit status -1 if given
   buffer pointer is invalid. */
static uint32_t
read (void *esp)
{
  uint32_t retval;
  int fd = (int) dereference (esp, 1, WORD_SIZE);
  void *buffer = (void *) dereference (esp, 2, WORD_SIZE);
  unsigned pos = 0, size = dereference (esp, 3, WORD_SIZE);
  struct file *fp = retrieve_fp (fd);

  if (!verify_write (buffer, size))
    process_exit (-1);

  if (fp == NULL && fp != STDIN_FILENO)
    return (uint32_t) -1;

  if (fd == STDIN_FILENO)
    {
      while (pos < size)
        ((char *) buffer)[pos++] = input_getc ();
      return (uint32_t) size;
    }

  lock_acquire (&filesys_lock);
  retval = (uint32_t) file_read (fp, buffer, size);
  lock_release (&filesys_lock);

  return retval;
}

/* System call handler for write(). Returns 0 if it cannot write any byte at 
   for some reason. */
static uint32_t
write (void *esp)
{
  uint32_t retval;
  int fd = (int) dereference (esp, 1, WORD_SIZE);
  void *buffer = (void *) dereference (esp, 2, WORD_SIZE);
  unsigned size = dereference (esp, 3, WORD_SIZE);
  struct file *fp = retrieve_fp (fd);

  if ((fp == NULL && fd != STDOUT_FILENO) || !verify_read (buffer, size))
    return (uint32_t) 0;

  if (fd == STDOUT_FILENO)
    {
      putbuf ((char *) buffer, size);
      return (uint32_t) size;
    }

  lock_acquire (&filesys_lock);
  retval = (uint32_t) file_write (fp, buffer, size);
  lock_release (&filesys_lock);

  return retval;
}

/* System call handler for seek(). */
static void
seek (void *esp)
{
  int fd = (int) dereference (esp, 1, WORD_SIZE);
  unsigned position = dereference (esp, 2, WORD_SIZE);
  struct file *fp = retrieve_fp (fd);

  if (fp == NULL)
    return;

  lock_acquire (&filesys_lock);
  file_seek (fp, position);
  lock_release (&filesys_lock);
}

/* System call handler for tell(). Returns -1 if given file descriptor is not 
   a valid file descriptor. */
static uint32_t
tell (void *esp)
{
  uint32_t retval;
  int fd = (int) dereference (esp, 1, WORD_SIZE);
  struct file *fp = retrieve_fp (fd);

  if (fp == NULL)
    return (uint32_t) -1;

  lock_acquire (&filesys_lock);
  retval = (uint32_t) file_tell (fp);
  lock_release (&filesys_lock);

  return retval;
}

/* System call handler for close(). Does notihing if given file descriptor is 
   not a valid file descriptor. */
static void
close (void *esp)
{
  int fd = (int) dereference (esp, 1, WORD_SIZE);
  struct file *fp = retrieve_fp (fd);

  lock_acquire (&filesys_lock);
  file_close (fp);
  lock_release (&filesys_lock);
}