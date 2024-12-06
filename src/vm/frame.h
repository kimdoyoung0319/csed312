/* TODO : add frame table definitions */
#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "userprog/process.h"
#include <bitmap.h>
#include <lib/stdbool.h>
#include "devices/block.h"
#include "vm/spt.h"

/* Frame Table Entry structure */
struct fte 
{
  void *kaddr;                  /* Kernel virtual address of the allocated frame. */
  void *uaddr;                  /* User virtual address this frame is mapped to. Used for SPT lookups. */
  struct process *process;      /* Process that owns this frame. Required for page table operations. */
  struct list_elem elem;        /* List element for frame table */
};

/* Basic operations on Frame Table. */
void ft_init(void);
void *ft_get_frame (void *);
void ft_free_frame (void *);

/* Basic operations on Swap Table */
void st_init (void);
void st_in (block_sector_t);
block_sector_t st_out (void);

#endif