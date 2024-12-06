#include "vm/page.h"
#include <hash.h>
#include "filesys/file.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "vm/frame.h"

/* A page records. */
struct pagerec 
  {
    struct hash records;
  };

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

/* Inserts PAGE into REC. If such PAGE already exists on REC, destroy it and
   insert new one. */
void
pagerec_set_page (struct pagerec *rec, struct page *page)
{
  ASSERT (rec != NULL);
  ASSERT (page != NULL);

  struct hash_elem *e;

  if ((e = hash_insert (&rec->records, &page->elem)) != NULL);
    {
      struct page *old = hash_entry (e, struct page, elem);
      page_destroy (old);
      hash_insert (&rec->records, &page->elem);
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
   current page. The new page will be inserted into current process's page 
   records. 
   
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

  page->state = PAGE_LOADED;
  page->uaddr = uaddr;
  page->pagedir = this->pagedir;
  page->size = PGSIZE;
  page->writable = writable;
  page->sector = 0;

  pagerec_set_page (&this->pagerec, page);
}

/* Makes a new page from a file, which will demand paged from FILE, starting 
   from OFFSET, with WRITABLE flag.

   The size of new page will be automatically set to PGSIZE if the remaining
   size from OFFSET to the length of FILE is greater than PGSIZE. Else, the size
   will be set to the remaining bytes of the file. The page directory of the 
   page will be automatically set to current process's page directory. The new 
   page will be inserted into current process's page records. 
   
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

  pagerec_set_page (&this->pagerec, page);
}

/* Makes a new page from the swap block, with address of UADDR and WRITABLE 
   flag, and the sector number SECTOR within the swap block. 
   
   the size of new page will be the maximum value of a page size. The page 
   directory of new page will be automatically set to the page directory of 
   current page. The new page will be inserted into current process's page 
   records.

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

  pagerec_set_page (&this->pagerec, page);
}

/* Destroys PAGE, mark the page as not present in its associated page 
   directory, deleting it from current process's page record. This also evicts
   physical page frame associated with it, if such exists. After this, access 
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
  pagerec_clear_page (&this->pagerec, page);
  free (page);
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