#include "vm/page.h"
#include <hash.h>
#include <string.h>
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "vm/frame.h"
#include "vm/swap.h"

/* A page records. */
struct pagerec 
  {
    struct hash records;
  };

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

/* Retrieves page whose virtual address is UADDR in REC. Returns a null pointer
   if there's no page with UADDR in the given REC. */
struct page *
pagerec_get_page (struct pagerec *rec, void *uaddr)
{
  ASSERT (rec != NULL);

  struct page p = {.uaddr = uaddr};
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

/* Makes a new page that alreay lies in the memory, with address of UADDR and
   WRITABLE flag. 
   
   The size of new page will be the maximum value of a page size. The page 
   directory of new page will be automatically set to the page directory of 
   current page.    

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
  page->size = PGSIZE;
  page->writable = writable;
  page->sector = 0;

  return page;
}

/* Makes a new page from a file, which will demand paged from FILE, starting 
   from OFFSET, with WRITABLE flag.

   The size of new page will be automatically set to PGSIZE if the remaining
   size from OFFSET to the length of FILE is greater than PGSIZE. Else, the size
   will be set to the remaining bytes of the file. The page directory of the 
   page will be automatically set to current process's page directory.    

   Returns a null pointer if it failed to allocate memory. UADDR must be page
   aligned. FILE must not be a null pointer and OFFSET must be less than the 
   length of FILE. If OFFSET is not sector aligned, it automatically truncates
   it to nearest sector boundary. */ 
struct page *
page_from_file (void *uaddr, bool writable, struct file *file, off_t offset)
{
  struct page *page = (struct page *) malloc (sizeof (struct page));
  struct process *this = thread_current ()->process;

  ASSERT (this != NULL);
  ASSERT (pg_ofs (uaddr) == 0);
  ASSERT (file != NULL);
  ASSERT (file_length (file) >= offset);
  ASSERT (offset % BLOCK_SECTOR_SIZE == 0);

  if (page == NULL)
    return NULL;

  page->state = PAGE_UNLOADED;
  page->uaddr = uaddr;
  page->pagedir = this->pagedir;
  page->writable = writable;

  page->sector = inode_get_sector (file_get_inode (file)) 
                 + offset / BLOCK_SECTOR_SIZE;

  page->size = file_length (file) - offset > PGSIZE 
               ? PGSIZE 
               : file_length (file) - offset;

  return page;
}

/* Makes a new page from the swap block, with address of UADDR and WRITABLE 
   flag, and the sector number SECTOR within the swap block. 
   
   the size of new page will be the maximum value of a page size. The page 
   directory of new page will be automatically set to the page directory of 
   current page. 

   Returns a null pointer if it failed to allocate memory. UADDR must be page
   aligned. 
*/
struct page *
page_from_swap (void *uaddr, bool writable, block_sector_t sector)
{
  struct page *page = (struct page *) malloc (sizeof (struct page));
  struct process *this = thread_current ()->process;

  ASSERT (this != NULL);
  ASSERT (pg_ofs (uaddr) == 0);

  if (page == NULL)
    return NULL;

  page->state = PAGE_SWAPPED;
  page->uaddr = uaddr;
  page->pagedir = this->pagedir;
  page->size = PGSIZE;
  page->writable = writable;
  page->sector = sector;

  return page;
}

/* Destroys PAGE, mark the page as not present in its associated page 
   directory, deleting it from current process's page record. This also evicts
   physical page frame associated with it, if such exists. After this, accessing 
   on PAGE will cause page fault, and the kernel won't recover from it. */
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

/* Fetch PAGE from the block device. The state of the page must be either 
   unloaded or swapped out. This function adds mapping from PAGE's user address 
   to newly allocated frame to associated page directory. Returns newly 
   allocated kernel address if it succeed, a null pointer if failed. */
void *
page_swap_in (struct page *page)
{
  ASSERT (page != NULL);
  ASSERT (page->state == PAGE_SWAPPED || page->state == PAGE_UNLOADED);

  int offset = 0;
  struct block *block;
  bool is_zero = (page->size == 0);
  void *kaddr = frame_allocate (page, is_zero);

  if (kaddr == NULL)
    return NULL;

  /* If the size of actual bytes within the page, fetching is not needed. */
  if (is_zero)
    goto finish;

  /* Determine the block to be fetched according to PAGE's state. */
  switch (page->state) 
    {
    case PAGE_SWAPPED: block = block_get_role (BLOCK_SWAP); break;
    case PAGE_UNLOADED: block = block_get_role (BLOCK_FILESYS); break;
    default: PANIC ("Loaded page cannot be swapped in again."); NOT_REACHED ();
    }

  /* Actually fetch the page from the block device. */
  for (block_sector_t sector = page->sector; 
       sector < PGSIZE / BLOCK_SECTOR_SIZE;
       sector++)
    {
      if (page->size - offset >= BLOCK_SECTOR_SIZE)
        block_read (block, sector, kaddr + offset);
      else if (BLOCK_SECTOR_SIZE > page->size - offset 
               && page->size - offset > 0)
        {
          /* If remaining bytes are not page aligned, zero out additionally 
             fetched bytes in the sector. */
          int zero_bytes = offset + BLOCK_SECTOR_SIZE - page->size; 
          uint8_t *zero_addr = (uint8_t *) kaddr + page->size; 

          block_read (block, sector, kaddr + offset);
          memset (zero_addr, 0, zero_bytes);
        }
      else
        memset (kaddr + offset, 0, BLOCK_SECTOR_SIZE);

      offset += BLOCK_SECTOR_SIZE;
    }

finish:
  /* Fetched, or at least allocated a frame for the page. Change state and
     make a mapping in page directory. */
  if (page->state == PAGE_SWAPPED)
    {
      page->state = PAGE_PRESENT;
      swap_free (page->sector);
    }
  else if (page->state == PAGE_UNLOADED)
    page->state = PAGE_LOADED;

  pagedir_set_page (page->pagedir, page->uaddr, kaddr, page->writable);
  return kaddr;
}

/* Swap PAGE out from the memory. This function writes the sector number 
   to which the page is swap out into PAGE, so the caller does not need to take 
   care of it. Also, it sets the present bit of the page directory entry 
   associated with PAGE to 0. PAGE must be in either present or loaded state. */
void
page_swap_out (struct page *page)
{
  ASSERT (page->state == PAGE_LOADED || page->state == PAGE_PRESENT);

  struct block *block;
  block_sector_t slot, sector;
  uint8_t *base = page->uaddr;
  void *kaddr = pagedir_get_page (page->pagedir, page->uaddr);
  bool should_write_back;

  /* For pages that are not from a file, it should be written back into swap 
     device, with fresh swap slot. For pages that are from a file, it should
     be written back only when it's writable and dirty. The sector number in
     this case will be the underlying sector number of the file. */
  if (page->state == PAGE_PRESENT)
    {
      block = block_get_role (BLOCK_SWAP);
      slot = swap_allocate ();
      should_write_back = true;
    }
  else
    {
      block = block_get_role (BLOCK_FILESYS);
      slot = page->sector;
      should_write_back = page->writable && page_is_dirty (page);
    }

  if (should_write_back)
    for (sector = 0; sector < PGSIZE / BLOCK_SECTOR_SIZE; sector++)
      block_write (block, slot + sector, base + sector * BLOCK_SECTOR_SIZE);

  pagedir_clear_page (page->pagedir, page->uaddr);
  frame_free (kaddr);
}

/* Returns true if this page is accessed. */
bool
page_is_accessed (struct page *page)
{
  ASSERT (page != NULL);

  return pagedir_is_accessed (page->pagedir, page->uaddr);
}

/* Returns true if this page is accessed. */
bool
page_is_dirty (struct page *page)
{
  ASSERT (page != NULL);

  return pagedir_is_dirty (page->pagedir, page->uaddr);
}

static void
free_page (struct hash_elem *e, void *aux UNUSED)
{
  struct page *p = hash_entry (e, struct page, elem);
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