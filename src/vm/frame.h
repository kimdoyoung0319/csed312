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

/* Frame Table Entry */
struct fte 
{
  void *kaddr;
  struct process *process;
  struct list_elem elem;
};

void ft_init(void);
void *ft_get_frame (void);
void ft_free_frame (void *kaddr);

/* Basic operations on ST */
void st_init (void);
void st_in (block_sector_t, void *);
block_sector_t st_out (void *);

#endif