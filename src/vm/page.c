#include "vm/page.h"
#include <hash.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "vm/frame.h"
#include "vm/swap.h"

/* A page record. */
struct pagerec 
  {
    struct hash records;
  };

/* Lock to ensure consistency of the file system. Definition can be found in
   userprog/syscall.c. */
extern struct lock filesys_lock;

static void free_page (struct hash_elem *, void * UNUSED);
static unsigned page_hash (const struct hash_elem *, void * UNUSED);
static bool page_less_func (const struct hash_elem *, const struct hash_elem *,
                            void * UNUSED);

/* Creates page records. Returns a null pointer if it failed to allocate 
   memory.*/
struct pagerec *
pagerec_create (void)
{
  struct pagerec *rec = (struct pagerec *) malloc (sizeof (struct pagerec));

  if (rec == NULL)
    return NULL;

  hash_init (&rec->records, page_hash, page_less_func, NULL);
  return rec;
}

/* Destroys page records, destroying all pages belong to it. */
void
pagerec_destroy (struct pagerec *rec)
{
  hash_destroy (&rec->records, free_page);
  free (rec);
}

/* Inserts PAGE into REC. If such PAGE already exists on REC, destroy it and
   insert new one. */
void
pagerec_set_page (struct pagerec *rec, struct page *page)
{
  ASSERT (rec != NULL);
  ASSERT (page != NULL);

  struct hash_elem *e;

  if ((e = hash_replace (&rec->records, &page->elem)) != NULL)
    {
      struct page *old = hash_entry (e, struct page, elem);
      page_destroy (old);
    }
}

/* Retrieves page to which UADDR belongs in REC. Returns a null pointer
   if there's no page with UADDR in the given REC. */
struct page *
pagerec_get_page (struct pagerec *rec, const void *uaddr)
{
  ASSERT (rec != NULL);

  struct page p = {.uaddr = pg_round_down (uaddr)};
  struct hash_elem *e = hash_find (&rec->records, &p.elem);

  return e == NULL ? NULL : hash_entry (e, struct page, elem);
}

/* Clears mapping associated with PAGE in REC. Does nothing when there's no 
   such mapping. */
void
pagerec_clear_page (struct pagerec *rec, struct page *page)
{
  ASSERT (rec != NULL);

  struct hash_elem *e = hash_find (&rec->records, &page->elem);
  
  if (e != NULL)
    hash_delete (&rec->records, e);
}

/* Makes a new page that already lies in the memory, with address of UADDR and
   WRITABLE flag. Since this assumes the new page is already in the physical
   memory, the caller must allocate a frame for this page right after calling 
   it.
   
   The page directory of new page will be automatically set to the page 
   directory of current process.    

   Returns a null pointer if it failed to allocate memory. UADDR must be page
   aligned. */
struct page *
page_from_memory (void *uaddr, bool writable)
{
  struct page *page = (struct page *) malloc (sizeof (struct page));
  struct process *this = thread_current ()->process;

  ASSERT (this != NULL);
  ASSERT (pg_ofs (uaddr) == 0);

  if (page == NULL)
    return NULL;

  page->state = PAGE_PRESENT;
  page->uaddr = uaddr;
  page->pagedir = this->pagedir;
  page->writable = writable;
  page->sector = 0;
  page->file = NULL;
  page->offset = 0;
  page->size = 0;

  return page;
}

/* Makes a new page from a file, which will demand paged from FILE, with 
   WRITABLE flag and SIZE, OFFSET within the file.

   The page directory of the page will be automatically set to current
   process's page directory.    

   Returns a null pointer if it failed to allocate memory. UADDR must be page
   aligned. FILE must not be a null pointer and OFFSET + SIZE must be less than 
   the length of FILE. SIZE must be less than or equal to PGSIZE. */ 
struct page *
page_from_file (void *uaddr, bool writable, struct file *file, 
                off_t offset, size_t size)
{
  struct page *page = (struct page *) malloc (sizeof (struct page));
  struct process *this = thread_current ()->process;

  ASSERT (this != NULL);
  ASSERT (pg_ofs (uaddr) == 0);
  ASSERT (file != NULL);
  ASSERT (file_length (file) >= offset + (off_t) size);
  ASSERT (size <= PGSIZE);

  if (page == NULL)
    return NULL;

  page->state = PAGE_UNLOADED;
  page->uaddr = uaddr;
  page->pagedir = this->pagedir;
  page->writable = writable;
  page->sector = 0;
  page->file = file;
  page->offset = offset;
  page->size = size;

  return page;
}

/* Destroys PAGE, mark the page as not present in its associated page 
   directory, deleting it from current process's page record. This also evicts
   physical page frame associated with it, if such exists. 
   
   After this, accessing on PAGE will cause page fault, and the kernel won't 
   recover from it. This does NOT close a opened file. */
void
page_destroy (struct page *page)
{
  struct process *this = thread_current ()->process;

  ASSERT (page != NULL);
  ASSERT (this != NULL);

  void *kaddr;
  if ((kaddr = pagedir_get_page (page->pagedir, page->uaddr)) != NULL)
    frame_free (kaddr);

  pagedir_clear_page (page->pagedir, page->uaddr);
  pagerec_clear_page (this->pagerec, page);
  free (page);
}

/* Fetchs PAGE from the swap device. The state of the page must be swapped out 
   state. This function adds mapping from PAGE's user address to newly 
   allocated frame to associated page directory. Returns newly allocated kernel 
   address if it succeed, a null pointer if failed. */
void *
page_swap_in (struct page *page)
{
  ASSERT (page != NULL);
  ASSERT (page->state == PAGE_SWAPPED);

  struct block *block = block_get_role (BLOCK_SWAP);
  uint8_t *kpage = (uint8_t *) frame_allocate (page);
  uint8_t *kaddr = kpage;

  if (kaddr == NULL)
    return NULL;

  /* Fetch the page from the block device. */
  for (block_sector_t sector = page->sector; 
       sector < page->sector + PGSIZE / BLOCK_SECTOR_SIZE;
       sector++)
    {
      block_read (block, sector, kaddr);
      kaddr += BLOCK_SECTOR_SIZE;
    }

  /* Fetched a frame for the page. Change state, free the swap slot, and make a 
     mapping in page directory. */
  page->state = PAGE_PRESENT;
  swap_free (page->sector);
  pagedir_set_page (page->pagedir, page->uaddr, kpage, page->writable);

  return kaddr;
}

/* Swaps PAGE out from the memory to the swap device. It evicts PAGE from the
   page directory associated with it. PAGE must be in the present state and 
   must not be a null pointer. */
void
page_swap_out (struct page *page)
{
  ASSERT (page != NULL);
  ASSERT (page->state == PAGE_PRESENT);

  uint8_t *kpage = pagedir_get_page (page->pagedir, page->uaddr);
  uint8_t *kaddr = kpage;

  struct block *block = block_get_role (BLOCK_SWAP);
  block_sector_t slot = swap_allocate ();

  for (block_sector_t sector = slot; 
       sector < slot + PGSIZE / BLOCK_SECTOR_SIZE; 
       sector++)
    {
      block_write (block, sector, kaddr);
      kaddr += BLOCK_SECTOR_SIZE;
    }

  page->state = PAGE_SWAPPED;
  page->sector = slot;
  pagedir_clear_page (page->pagedir, page->uaddr);
  frame_free (kpage);
}

/* Loads PAGE into a physical frame. PAGE must be in the unloaded state and
   must not be a null pointer. Returns newly allocated kernel address if it 
   succeed, a null pointer if failed. */
void *
page_load (struct page *page)
{
  ASSERT (page != NULL);
  ASSERT (page->state == PAGE_UNLOADED);

  uint8_t *kpage = frame_allocate (page);

  if (kpage == NULL)
    return NULL;

  lock_acquire (&filesys_lock);
  file_read_at (page->file, kpage, page->size, page->offset);
  lock_release (&filesys_lock);

  page->state = PAGE_PRESENT;
  pagedir_set_page (page->pagedir, page->uaddr, kpage, page->writable);

  return kpage;
}

/* Unloads, or writes back PAGE into the underlying file. It evicts PAGE from 
   the page directory associated with it. PAGE must be in the present state and 
   must not be a null pointer, and must have file associated with it. */
void
page_unload (struct page *page)
{
  ASSERT (page != NULL);
  ASSERT (page->state == PAGE_PRESENT);
  ASSERT (page->file != NULL);

  void *buffer = palloc_get_page (0);
  uint8_t *kpage = pagedir_get_page (page->pagedir, page->uaddr);
  memcpy (buffer, kpage, page->size);
  
  if (page->writable && page_is_dirty (page))
    {
      lock_acquire (&filesys_lock);
      file_write_at (page->file, buffer, page->size, page->offset);
      lock_release (&filesys_lock);
    }

  palloc_free_page (buffer);
  page->state = PAGE_UNLOADED;
  pagedir_clear_page (page->pagedir, page->uaddr);
  frame_free (kpage);
}

/* Returns true if this page is accessed. */
bool
page_is_accessed (const struct page *page)
{
  ASSERT (page != NULL);

  return pagedir_is_accessed (page->pagedir, page->uaddr);
}

/* Returns true if this page is accessed. */
bool
page_is_dirty (const struct page *page)
{
  ASSERT (page != NULL);

  return pagedir_is_dirty (page->pagedir, page->uaddr);
}

/* Destroys a single page associated with E. Used to destroy all pages in a 
   page record. */
static void
free_page (struct hash_elem *e, void *aux UNUSED)
{
  struct page *p = hash_entry (e, struct page, elem);

  /* TODO: What if P is in state of PAGE_SWAPPED? Should we bring it back to 
           the memory? */
  if (p->state == PAGE_PRESENT && p->file != NULL)
    page_unload (p);

  page_destroy (p);
}

/* Hash function for pages. */
static unsigned
page_hash (const struct hash_elem *e, void *aux UNUSED)
{
  const void *uaddr = hash_entry (e, struct page, elem)->uaddr;
  return hash_bytes (&uaddr, sizeof uaddr);
}

/* Hash less function for pages. */
static bool
page_less_func (const struct hash_elem *a_, const struct hash_elem *b_, 
               void *aux UNUSED)
{
  const struct page *a = hash_entry (a_, struct page, elem);
  const struct page *b = hash_entry (b_, struct page, elem);

  return a->uaddr < b->uaddr;
}