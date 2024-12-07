#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "vm/page.h"
#include "vm/frame.h"

#define WORD_SIZE (sizeof (intptr_t))

/* Lock to ensure consistency of the file system. Definition can be found in
   userprog/syscall.c. */
extern struct lock filesys_lock;

/* Frame needed to execute a process from command line. */
struct process_exec_frame
  {
    char **argv;
    struct process *parent;
    bool *success;
  };

static struct process *make_process (struct process *, struct thread *);
static void destroy_process (struct process *);
static thread_func start_process NO_RETURN;
static bool load (const char *file_name, void (**eip) (void), void **esp);
static void *push (void *, void *, int);

/* Initialize top level process. This should be called after thread_init(). */
void
process_init (void)
{
  make_process (NULL, thread_current ());
}

/* Starts a new thread running a user program loaded from
   CMD_LINE.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   process id, or PID_ERROR if the thread cannot be created. 
   This must be executed in user process's context. */
pid_t
process_execute (const char *cmd_line) 
{
  int i = 0;
  char *cmd_line_copy, *pos, *token, **argv;
  struct process *this = thread_current ()->process;
  struct process_exec_frame frame;
  bool success = false;
  tid_t tid;

  ASSERT (this != NULL);

  /* Make a copy of CMD_LINE.
     Otherwise there's a race between the caller and load(). */
  cmd_line_copy = palloc_get_page (0);
  if (cmd_line_copy == NULL)
    return PID_ERROR;
  strlcpy (cmd_line_copy, cmd_line, PGSIZE);

  /* Tokenize the copy of CMD_LINE, 
     and stores each address of tokenized string in ARGV. */
  argv = palloc_get_page (PAL_ZERO);
  if (argv == NULL)
    return PID_ERROR;
  for (token = strtok_r (cmd_line_copy, " ", &pos); token != NULL;
       token = strtok_r (NULL, " ", &pos))
    argv[i++] = token;

  /* Create a new thread to be executed with ARGV. */
  frame.argv = argv;
  frame.parent = this;
  frame.success = &success;

  tid = thread_create (argv[0], PRI_DEFAULT, start_process, &frame);
  if (tid == TID_ERROR)
    {
      palloc_free_page (cmd_line_copy); 
      palloc_free_page (argv);
    }

  sema_down (&this->sema);
  if (!success)
    return PID_ERROR;

  return (pid_t) tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *frame_)
{
  int argc, padding, len, i;
  struct process_exec_frame *frame = frame_;
  char **argv = frame->argv;
  struct process *par = frame->parent;
  void *esp, *cmd_line = pg_round_down (argv[0]);
  struct intr_frame if_;
  struct thread *cur = thread_current ();
  bool load_success, *success = frame->success;

  /* Initialize interrupt frame. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  /* Load executable and make process. */
  cur->process = make_process (par, cur);
  load_success = load (argv[0], &if_.eip, &if_.esp);
  *success = load_success && (cur->process != NULL);
  sema_up (&par->sema);

  /* If load or process making failed, quit. */
  if (!(*success))
    {
      palloc_free_page (argv);
      palloc_free_page (cmd_line);
      cur->process = NULL;
      destroy_process (cur->process);
      thread_exit ();
    }

  /* Get ARGC. */
  for (argc = 0; argv[argc] != NULL; argc++);

  /* Push arguments in ARGV in reverse order, calculate padding, and
     store pushed arguments's address on ARGV since original address
     is no longer needed. */
  padding = 0;
  esp = if_.esp;
  for (i = argc - 1; i >= 0; i--)
    {
      len = strlen (argv[i]) + 1;
      esp = push (esp, argv[i], len);
      argv[i] = esp;
      padding = WORD_SIZE - (len + padding) % WORD_SIZE;
    }
  
  /* Push padding and null pointer indicating the end of argument vector. */
  esp = push (esp, NULL, padding);
  esp = push (esp, NULL, sizeof (char *));

  /* Push argument vector. */
  for (i = argc - 1; i >= 0; i--)
    esp = push (esp, &argv[i], sizeof (char *));
  esp = push (esp, &esp, sizeof (char **));

  /* Push ARGC and dummy return address. */
  esp = push (esp, &argc, sizeof (int));
  esp = push (esp, NULL, sizeof (void (*) (void)));
  if_.esp = esp;

  palloc_free_page (argv);
  palloc_free_page (cmd_line);

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for process CHILD_PID to die and returns its exit status.  
   If it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If CHILD_PID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given PID, returns -1
   immediately, without waiting. It must be called in user process
   context. */
int
process_wait (pid_t child_pid) 
{
  int status;
  struct process *child = NULL, *this = thread_current ()->process;
  struct list_elem *e;

  ASSERT (this != NULL);

  /* Find child to wait. */
  for (e = list_begin (&this->children); e != list_end (&this->children);
       e = list_next (e))
    {
      child = list_entry (e, struct process, elem);
      if (child->pid == child_pid)
        break;
    }

  /* If there's no such child or the child is already waited, return -1. */
  if (child == NULL || child->pid != child_pid || child->waited)
    return -1;

  /* If the child to wait is still alive, wait for it to exit. */
  child->waited = true;
  if (child->state == PROCESS_ALIVE)
    sema_down (&this->sema);

  /* The child has exited. Get its exit status and clean it up. */
  status = child->status;
  list_remove (&child->elem);
  destroy_process (child);
  return status;
}

/* Make process whose parent is PAR and whose associated kernel thread is T. 
   Also, does basic initializations on it. Returns null pointer if memory
   allocation has failed or page directory creation has failed. */
static struct process *
make_process (struct process *par, struct thread *t)
{
  struct process *this = (struct process *) malloc (sizeof (struct process));

  ASSERT (t != NULL);

  if (this == NULL)
    return NULL;
  
  this->pid = t->tid;
  this->state = PROCESS_ALIVE;
  strlcpy (this->name, t->name, sizeof this->name);
  sema_init (&this->sema, 0);
  this->thread = t;
  this->waited = false;
  list_init (&this->children);
  list_init (&this->opened);
  this->pagerec = pagerec_create ();

  t->process = this;

  if (par != NULL)
    {
      list_push_back (&par->children, &this->elem);
      this->parent = par->thread;
    }

  if ((this->pagedir = pagedir_create ()) == NULL)
    {
      free (this);
      return NULL;
    }

  return this;
}

/* Free the resources of P. It neither exits its associated kernel thread,
   nor modifies its parent or children. It only frees memorys taken to 
   represent process structure. If P is a null pointer, does nothing. */
static void
destroy_process (struct process *p)
{
  struct process *this = thread_current ()->process;

  if (p == NULL)
    return;

  uint32_t *pd = p->pagedir;
  struct pagerec *pr = p->pagerec;

  /* Destroys page record. The ordering is important here since destroying 
     page records involves accessing the page directory. */
  if (pr != NULL)
    pagerec_destroy (pr);

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory when the destroyed process is current
     process. */
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         p->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory. We must activate the base page
         directory before destroying own process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      p->pagedir = NULL;
      if (this == p)
        pagedir_activate (NULL);
      pagedir_destroy (pd);
    }

  free (p);
}

/* Sets up the CPU for running user code in the current thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct process *this = thread_current ()->process;

  /* Activate thread's page tables. If it is kernel which does not have
     associated user process, activate initial one that has only kernel 
     mapping.*/
  if (this == NULL)
    pagedir_activate (NULL);
  else
    pagedir_activate (this->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* Exits current process with exit code of STATUS. This must be executed in
   user process's context. */
void 
process_exit (int status)
{
  struct file *fp;
  struct list_elem *e, *next;
  struct thread *cur = thread_current ();
  struct process *child, *this = cur->process;

  ASSERT (this != NULL);

  struct thread *par;
  par = this->parent;
  this->state = PROCESS_DEAD;

  ASSERT (!(this->waited && par == NULL));
  ASSERT (!(this->waited && par->process == NULL));

  this->status = status;

  /* This is to maintain consistency of process structures. Interrupt will be 
     enabled after thread_exit() call which causes context switch. */
  intr_disable ();

  /* Exiting process's children are orphaned. Destroys dead children that 
     their parents are responsible for destroying. */
  for (e = list_begin (&this->children); e != list_end (&this->children); )
    {
      child = list_entry (e, struct process, elem);
      next = list_remove (e);
      child->parent = NULL;

      if (child->status == PROCESS_DEAD)
        destroy_process (child);

      e = next;
    }
  
  /* Close opened files. */
  for (e = list_begin (&this->opened); e != list_end (&this->opened); )
    {
      next = list_remove (e);
      fp = list_entry (e, struct file, elem);
      file_close (fp);
      e = next;
    }

  printf ("%s: exit(%d)\n", this->name, status);

  /* For processes who are orphaned, they are responsible to destory themselves.
     For those who are not orphaned, their parents are responsible to destroy 
     children. */
  if (this->waited)
    sema_up (&par->process->sema);
  else if (par == NULL || par->process == NULL)
    destroy_process (this);

  cur->process = NULL;
  thread_exit ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  if (thread_current ()->process == NULL)
    goto done;

  /* Activate page directory. */
  process_activate ();

  /* Open executable file. */
  lock_acquire (&filesys_lock);
  file = filesys_open (file_name);
  lock_release (&filesys_lock);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }
  file_deny_write (file);

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  if (!success)
    file_close (file);

  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *uaddr,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  struct process *this = thread_current ()->process;

  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (uaddr) == 0);
  ASSERT (ofs % PGSIZE == 0);
  ASSERT (this != NULL);

  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Make a page to be inserted to the page record. */
      struct page *upage = 
        page_from_file (uaddr, writable, file, ofs, page_read_bytes);

      /* TODO: Shouldn't we clean all the pages allocated? */
      if (upage == NULL)
        return false;

      /* Register this page onto current process's page record. */
      pagerec_set_page (this->pagerec, upage);

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      uaddr += PGSIZE;
      ofs += page_read_bytes;
    }

  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  struct process *this = thread_current ()->process;

  ASSERT (this != NULL);

  uint8_t *kaddr, *stack_base = ((uint8_t *) PHYS_BASE) - PGSIZE;
  bool success = false;
  struct page *upage = page_from_memory (stack_base, this->pagedir);

  kaddr = frame_allocate (upage, true);
  if (kaddr != NULL) 
    {
      pagerec_set_page (this->pagerec, upage);
      success = install_page (stack_base, kaddr, true);
      if (success)
        *esp = PHYS_BASE;
      else
        {
          frame_free (kaddr);
          page_destroy (upage);
        }
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct process *this = thread_current ()->process;

  ASSERT (this != NULL);

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (this->pagedir, upage) == NULL
          && pagedir_set_page (this->pagedir, upage, kpage, writable));
}

/* Pushes SIZE bytes of data from SRC at the top of the stack specified
   with TOP. TOP must be a pointer to somewhere in user virtual address 
   space. Or PHYS_BASE if the stack is empty. Returns an address that refers 
   to new top of the stack. If SRC is a null pointer, then pushes SIZE bytes 
   of zeros on the stack. */
static void *
push (void *top, void *src, int size)
{
  ASSERT (is_user_vaddr (top) || top == PHYS_BASE);

  char *new = (char *) top - size;

  if (src == NULL)
    memset (new, 0, size);
  else
    memcpy ((void *) new, src, size);

  return (void *) new;
}
