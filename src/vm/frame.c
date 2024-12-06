#include "vm/frame.h"
#include <stdbool.h>
#include <list.h>
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

/* Global frame table. */
struct list frames;

/* A physical frame. */
struct frame
  {
    void *kaddr;
    struct page *page;
    bool accessed;
    struct list_elem elem;
  };

static void *evict (struct frame *);

/* Initialize global frame table. */
void
frame_init (void)
{
  list_init (&frames);
}

/* Allocates a frame from user pool associated with PAGE, evicting one if 
   needed. New frame will be filled with zeros if is_zero flag is set. The
   newly made frame will be installed into page directory of PAGE. */
void *
frame_allocate (struct page *page, bool is_zero)
{
  ASSERT (page != NULL);

  struct frame *frame;
  void *kaddr = is_zero ? palloc_get_page (PAL_USER | PAL_ZERO) 
                        : palloc_get_page (PAL_USER);

  /* Luckily, there's room for a new frame. Set basic informations and 
     register it to the frame table. */
  if (kaddr != NULL) 
    {
      frame = (struct frame *) malloc (sizeof (struct frame));
      frame->accessed = false;
      frame->page = page;
      frame->kaddr = kaddr;
      list_push_back (&frames, &frame->elem);
      pagedir_set_page (page->pagedir, page->uaddr, kaddr, page->writable);

      return kaddr;
    }
  /* TODO: Remove below two lines after implementing supplemental page table. */
  else
    PANIC ("Not enough physical frames.");


  /* Initial attempt to allocate a frame has failed. Find a frame to be evicted
     by clock algorithm. */
  struct page *p;
  struct list_elem *e;

  /* Copies accessed bits from page table entries. */
  for (e = list_begin (&frames); e != list_end (&frames); e = list_next (e))
    {
      frame = list_entry (e, struct frame, elem);
      p = frame->page;

      frame->accessed = page_is_accessed (p);
    }

  /* Find the frame to be evicted by clock algorithm. */
  e = list_begin (&frames);
  while (true)
    {
      frame = list_entry (e, struct frame, elem);

      if (frame->accessed)
        frame->accessed = false;
      else
        break;

      if (e == list_end (&frames))
        e = list_begin (&frames);
      else
        e = list_next (&frames);
    }

  /* At this point, there's at least one free frame. Reset frame informations 
     and re-insert it into frame table. */
  palloc_free_page(evict (frame));
  frame->kaddr = palloc_get_page (PAL_USER);
  frame->page = page;
  frame->accessed = false;
  list_push_back (&frames, frame);
  pagedir_set_page (page->pagedir, page->uaddr, kaddr, page->writable);

  return frame->kaddr;
}

/* Frees a physical frame associated with KADDR. If the frame for KADDR is
   not registered into the frame table, does nothing. */
void
frame_free (void *kaddr)
{
  struct list_elem *e;
  struct frame *frame;

  for (e = list_begin (&frames); e != list_end (&frames); e = list_next (e))
    {
      frame = list_entry (e, struct frame, elem);

      if (frame->kaddr == kaddr)
        break;
    }

  if (frame->kaddr != kaddr)
    return;
  
  palloc_free_page(evict (frame));
  free (frame);
}

/* Evicts FRAME from frame table, marks associated user page as not present 
   from its page directory. Returns the kernel address of the frame to be 
   free'd after all these task. Notice that this does not free the struct frame
   itself. You can use it freely again with new kernel virtual address. */
/* TODO: Implement swap out. */
static void *
evict (struct frame *frame)
{
  struct page *page = frame->page;
  uint32_t *pagedir = page->pagedir;

  pagedir_clear_page (pagedir, page->uaddr);
  list_remove (&frame->elem);

  return frame->kaddr;
}