#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "devices/block.h"
#include "userprog/gdt.h"
#include "userprog/process.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/spt.h"
#include "vm/frame.h"

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* Lock to ensure consistency of the file system. Definition can be found in
   userprog/syscall.c. */
extern struct lock filesys_lock;

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      process_exit (-1);

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

  uint8_t *upage = pg_round_down (fault_addr);
  struct spte *entry = spt_lookup (upage);
  struct process *tmp = thread_current ()->process;
  uint8_t *uesp = user ? f->esp : tmp->uesp;

  bool growthable = uesp != NULL 
                    && (uint8_t *)uesp - 32 <= fault_addr 
                    && is_user_vaddr (fault_addr) 
                    && is_user_limit (fault_addr) ? true : false;

  // printf ("\n not_present : %d ", not_present);
  // printf ("\n write : %d ", write);
  // printf ("\n growthable : %d \n", growthable);

  if (entry == NULL)
    {
      /* 정상적인 stack growth 상황 */
      if (not_present && growthable)
        {
          /* frame table 에 추가하기 */
          void *kpage = ft_get_frame (upage);
          if (kpage == NULL)
            {
              process_exit (-1);
            }

          /* spt 엔트리 생성하여 할당하기 */
          entry = spt_make_entry (upage, PGSIZE, BLOCK_FAILED);
          if (entry == NULL)
            {
              ft_free_frame (kpage);
              process_exit (-1);
            }

          /* spt 에 spte 추가하기 */
          entry->swapped = false;
          entry->writable = true;
          hash_insert (&thread_current ()->process->spt, &entry->elem);
          if (!process_install_page (upage, kpage, true))
            {

              ft_free_frame (kpage);
              spt_free_entry (entry);
              process_exit (-1);
            }
        }

      /* 잘못된 상황 */
      else
        {
          process_exit (-1);
        }
    }
  else /* Lazy Loading or Swap in or File Memory Mapped??*/
    {      
      /* Lazy Loading 인 경우 */
      if (entry->lazy)
        {
          uint8_t *kpage = ft_get_frame (entry->uaddr);
          if (kpage == NULL)
            process_exit (-1);
          
          /* Load this page. */
          lock_acquire (&filesys_lock);
          file_seek (entry->file, entry->ofs);
          bool chk = file_read (entry->file, kpage, entry->size) == (int) entry->size;
          lock_release (&filesys_lock);

          if (chk)
            {
              ft_free_frame (kpage);
              process_exit (-1);
            }

          /* Add the page to the process's address space. */
          if (!process_install_page (entry->uaddr, kpage, entry->writable)) 
            {
              ft_free_frame (kpage);
              process_exit (-1);
            }
          
          entry->lazy = false;
        }
      /* swap in 하는 경우 */
      else if (entry->index != BLOCK_FAILED 
              && entry->swapped)
        {
          void *kpage = ft_get_frame (upage); 
          if (kpage == NULL)
            process_exit (-1);

          if (!process_install_page (upage, kpage, true))
            {
              ft_free_frame (kpage);
              process_exit (-1);
            }

          block_sector_t size = 0;
          for (size = 0; size < PGSIZE / BLOCK_SECTOR_SIZE;)
            block_read (block_get_role (BLOCK_SWAP), 
                        entry->index + size, 
                        kpage + (size++ * BLOCK_SECTOR_SIZE));
          st_in (entry->index / (PGSIZE / BLOCK_SECTOR_SIZE));
          entry->swapped = false;
          entry->index = BLOCK_FAILED;
        }
      /* file memory mapped */
      else if (entry->mapid != BLOCK_FAILED)
        {
          process_exit (-1);
          /* 메모리 매핑은 되었지만 아직 로딩되지 않은 경우 */
          /* 대응되는 mmap 을 참고하여 (이전에 생성해줘야 함) */
          /* memory mapped 된 uaddr과 map id 를 기반으로 block_read 수행 */
        }
      /* error */
      else
        {
          process_exit (-1);
        }
    } 
}
