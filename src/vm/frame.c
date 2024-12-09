/* TODO: Change frame table implementation into array-based. */
#include "vm/frame.h"
#include <stdbool.h>
#include <list.h>
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

/* Global frame list. */
struct list frames;

/* Lock for frame table. */
struct lock frames_lock;

/* List element Pointer for clock algorithm */
struct list_elem *hand;

/* A physical frame. */
struct frame
  {
    void *kaddr;                /* Kernel address of this frame. */
    struct page *page;          /* Page associated with this frame. */
    bool accessed;              /* Is accessed? */
    bool pinned;                /* Is pinned? */
    struct list_elem elem;      /* List element for global frame list. */
  };

/* Initialize global frame table. */
void
frame_init (void)
{
  list_init (&frames);
  lock_init (&frames_lock);
}

/* Allocates a frame from user pool associated with PAGE, evicting one if 
   needed. New frame will be filled with zeros. */
void *
frame_allocate (struct page *page)
{
  ASSERT (page != NULL);

  struct frame *frame;
  void *kaddr = palloc_get_page (PAL_USER | PAL_ZERO); 

  /* Luckily, there's room for a new frame. Set basic informations and 
     register it to the frame table. */
  if (kaddr != NULL) 
    {
      frame = (struct frame *) malloc (sizeof (struct frame));
      frame->accessed = false;
      frame->page = page;
      frame->kaddr = kaddr;
      frame->pinned = false;

      lock_acquire (&frames_lock);
      list_push_back (&frames, &frame->elem);

      if (hand == NULL)
        hand = list_begin (&frames);

      lock_release (&frames_lock);

      return kaddr;
    }

  /* Initial attempt to allocate a frame has failed. Find a frame to be evicted
     by clock algorithm. */
  struct page *p;
  struct list_elem *e;

  /* Copies accessed bits from page table entries. */
  lock_acquire (&frames_lock);
  for (e = list_begin (&frames); e != list_end (&frames); e = list_next (e))
    {
      frame = list_entry (e, struct frame, elem);
      p = frame->page;

      frame->accessed = page_is_accessed (p);
    }

  /* Find the frame to be evicted by clock algorithm. */
  e = hand;
  while (true)
    {
      frame = list_entry (e, struct frame, elem);

      if (e == list_rbegin (&frames))
        e = list_begin (&frames);
      else
        e = list_next (e);

      if (frame->accessed)
        frame->accessed = false;
      else if (!frame->pinned)
        break;
    }
  hand = e;
  list_remove (&frame->elem);
  lock_release (&frames_lock);

  page_swap_out (frame->page);
  palloc_free_page (frame->kaddr);

  /* At this point, there's at least one free frame. Reset frame 
     informations. */
  frame->kaddr = palloc_get_page (PAL_USER | PAL_ZERO);
  frame->page = page;
  frame->accessed = false;

  lock_acquire (&frames_lock);
  list_push_back (&frames, &frame->elem);
  lock_release (&frames_lock);

  return frame->kaddr;
}

/* Frees a physical frame associated with KADDR. If the frame for KADDR is
   not registered to the frame table, does nothing. KADDR must be 
   page-aligned.*/
void
frame_free (void *kaddr)
{
  struct list_elem *e;
  struct frame *frame = NULL;

  lock_acquire (&frames_lock);
  for (e = list_begin (&frames); e != list_end (&frames); e = list_next (e))
    {
      frame = list_entry (e, struct frame, elem);

      if (frame->kaddr == kaddr)
        break;
    }

  if (frame == NULL || frame->kaddr != kaddr)
    {
      lock_release (&frames_lock);
      return;
    }
  
  list_remove (&frame->elem);
  lock_release (&frames_lock);

  palloc_free_page (frame->kaddr);
  free (frame);
}

/* Atomically pin a frame associated with KADDR. If there's no such frame 
   asssociated with KADDR, does nothing. KADDR must be page-aligned. */
void
frame_pin (void *kaddr)
{
  lock_acquire (&frames_lock);
  
  struct list_elem *e;
  struct frame *frame = NULL;

  for (e = list_begin (&frames); e != list_end (&frames); e = list_next (e))
    {
      frame = list_entry (e, struct frame, elem);

      if (frame->kaddr == kaddr)
        break;
    }

  if (frame == NULL || frame->kaddr != kaddr)
    {
      lock_release (&frames_lock);
      return;
    }
  
  frame->pinned = true;

  lock_release (&frames_lock);
}

/* Atomically unpin a frame associated with KADDR. If there's no such frame 
   asssociated with KADDR, does nothing. */
void
frame_unpin (void *kaddr)
{
  lock_acquire (&frames_lock);
  
  struct list_elem *e;
  struct frame *frame = NULL;

  for (e = list_begin (&frames); e != list_end (&frames); e = list_next (e))
    {
      frame = list_entry (e, struct frame, elem);

      if (frame->kaddr == kaddr)
        break;
    }

  if (frame == NULL || frame->kaddr != kaddr)
    {
      lock_release (&frames_lock);
      return;
    }
  
  frame->pinned = false;

  lock_release (&frames_lock);
}
