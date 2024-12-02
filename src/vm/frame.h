/* TODO : add frame table definitions */
#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "userprog/process.h"


/* Frame Table Entry */
struct fte 
{
  void *kpage;
  struct process *process;
  struct list_elem elem;
};

void ft_init(void);
void *ft_get_frame (void *upage, enum palloc_flags palloc_flags);
void ft_free_frame (void *kpage);
struct fte *ft_get_oldFrame (void);

#endif