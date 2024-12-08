/* TODO: Prevent page faults when some resource is acquired by the kernel. */
#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <mapid.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "vm/page.h"

#define WORD_SIZE (sizeof (intptr_t))

/* A file-memory mapping. */
struct mapping
  {
    uint8_t *uaddr;          /* Starting user address. */
    int pages;               /* The number of pages belong to this mapping. */
    mapid_t mapid;           /* Mapping identifier. */
    struct file *file;       /* File for this mapping. */
    struct list_elem elem;   /* List element. */
  };

/* Lock to ensure consistency of the file system. */
struct lock filesys_lock;

/* Lock used by allocate_mapid(). */
static struct lock mapid_lock;

static void syscall_handler (struct intr_frame *);
static uint32_t dereference (const void *, int, int);
static struct file *retrieve_fp (int);
static bool verify_string (const char *);
static bool verify_read (char *, int);
static bool verify_write (char *, int);
static int get_user (const uint8_t *);
static bool put_user (uint8_t *, uint8_t);
static mapid_t allocate_mapid (void);
static bool is_mapping_valid (void *, struct file *);

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
static uint32_t mmap (void *);
static void munmap (void *);

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

  switch (syscall_number) 
    {
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
    case SYS_MMAP: f->eax = mmap (f->esp); break;
    case SYS_MUNMAP: munmap (f->esp); break;
    }
}

/* Dereferences pointer BASE + INDEX * OFFSET, with a validity test. Returns
   4 byte chunk starting from BASE + OFFSET * INDEX if it passes the test. 
   Else, terminates current process. */
static uint32_t
dereference (const void *base, int index, int offset)
{
  const uint8_t *base_ = base;
  void *uaddr = (void *) (base_ + offset * index);

  if (is_user_vaddr (uaddr))
    return *((uint32_t *) uaddr);
  else
    process_exit (-1);

  NOT_REACHED ();
}

/* Retrieves file pointer from file descriptor, FD. Returns NULL if it has
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
   are valid. */
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
   on each bytes of BUF. Return true if and only if all bytes in BUF are 
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
   on each bytes of BUF. Return true if and only if all bytes in BUF are 
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

/* Returns an map identifier to use for a new mapping. */
static mapid_t
allocate_mapid (void) 
{
  static mapid_t next_mapid = 1;
  mapid_t mapid;

  lock_acquire (&mapid_lock);
  mapid = next_mapid++;
  lock_release (&mapid_lock);

  return mapid;
}

/* Checks a file-memory mapping for FILE starting from ADDR is valid or not. */
static bool
is_mapping_valid (void *addr, struct file *file)
{
  if (file == NULL 
      || file_length (file) == 0 
      || pg_ofs (addr) != 0 
      || addr == NULL)
    return false;

  struct process *this = thread_current ()->process;
  int pages = file_length (file) / PGSIZE + 1;

  for (int i = 0; i < pages; i++)
    {
      void *upage = addr + i * PGSIZE;

      if (pagerec_get_page (this->pagerec, upage) != NULL)
        return false;
    }

  return true;
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
  int fd = (int) dereference (esp, 1, WORD_SIZE);
  struct file *fp = retrieve_fp (fd);

  if (fp == NULL)
    return (uint32_t) -1;

  return (uint32_t) file_length (fp);
}

/* System call handler for read(). Returns -1 if given file descriptor is not 
   a valid file descriptor. Kills current process with exit status -1 if given
   buffer pointer is invalid. */
static uint32_t
read (void *esp)
{
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

  /* Since page fault might occur when we directly pass BUFFER into 
     file_read(), we make another buffer in kernel region and fetch on it 
     first. */
  uint32_t result;
  void *buffer_ = malloc (size);
  result = (uint32_t) file_read (fp, buffer_, size);

  memcpy (buffer, buffer_, size);
  free (buffer_);
  
  return result;
}

/* System call handler for write(). Returns 0 if it cannot write any byte at 
   for some reason. */
static uint32_t
write (void *esp)
{
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
  
  uint32_t result;
  void *buffer_ = (void *) malloc (size);

  memcpy (buffer_, buffer, size);
  result = (uint32_t) file_write (fp, buffer_, size);
  free (buffer_);

  return result;
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

  file_seek (fp, position);
}

/* System call handler for tell(). Returns -1 if given file descriptor is not 
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

/* System call handler for mmap(). */
static uint32_t
mmap (void *esp)
{
  int fd = (int) dereference (esp, 1, WORD_SIZE);
  uint8_t *addr = (uint8_t *) dereference (esp, 2, WORD_SIZE);

  struct file *file = file_reopen (retrieve_fp (fd));
  struct process *this = thread_current ()->process;

  ASSERT (this != NULL);

  if (!is_mapping_valid (addr, file))
    return MAP_FAILED;

  struct mapping *mapping = (struct mapping *) malloc (sizeof (struct mapping));

  mapping->uaddr = addr;
  mapping->file = file;
  mapping->mapid = allocate_mapid ();
  mapping->pages = file_length (file) / PGSIZE + 1;

  for (off_t offset = 0; offset < file_length (file); offset += PGSIZE)
    {
      size_t size = file_length (file) - offset >= PGSIZE 
                    ? PGSIZE 
                    : file_length (file) - offset;

      /* TODO: Is it safe to set writable flag to true? */
      struct page *page = page_from_file (addr + offset, true, file, offset, size);
      pagerec_set_page (this->pagerec, page);
    }
  
  list_push_back (&this->mappings, &mapping->elem);
  return mapping->mapid;
}

/* System call handler for munmap(). */
static void
munmap (void *esp)
{
  mapid_t mapid = (mapid_t) dereference (esp, 1, WORD_SIZE);

  struct mapping *mapping = NULL;
  struct process *this = thread_current ()->process;

  for (struct list_elem *e = list_begin (&this->mappings); 
       e != list_end (&this->mappings);
       e = list_next (e))
    {
      mapping = list_entry (e, struct mapping, elem);

      if (mapping->mapid == mapid)
        break;
    }

  if (mapping == NULL || mapping->mapid != mapid)
    return;

  for (int i = 0; i < mapping->pages; i++)
    {
      struct page *page = 
        pagerec_get_page (this->pagerec, mapping->uaddr + i * PGSIZE);

      page_swap_out (page);
      pagerec_clear_page (this->pagerec, page);
    }
  
  file_close (mapping->file);
  free (mapping);
}