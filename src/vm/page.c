#include "vm/page.h"
#include <hash.h>
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

struct pagerec 
  {
  };

/* Make a entry for page records, doing basic initializations on it. */
struct page *
page_make (void *uaddr, uint32_t *pagedir)
{
  struct page *page = (struct page *) malloc (sizeof (struct page));
  page->uaddr = uaddr;
  /* TODO: Set pagedir to current process's page directory. */
  page->pagedir = pagedir;

  return page;
}

/* Makes a new page that alreay lies in the memory, with address of UADDR. 
   The size of new page will be the maximum value of a page size. The page 
   directory of new page will be automatically set to the page directory of 
   current page. The new page will be inserted into current process's page 
   records. Returns a null pointer if it failed to allocate memory. */
struct page *
page_from_memory (void *uaddr, bool writable)
{
  struct page *page = (struct page *) malloc (sizeof (struct page));
  struct process *this = thread_current ()->process;

  ASSERT (this != NULL);

  if (page == NULL)
    return NULL;

  page->state = PAGE_LOADED;
  page->uaddr = uaddr;
  page->pagedir = this->pagedir;
  page->size = PGSIZE;
}

/* Destroys PAGE, mark the page as not present in its associated page 
   directory. After this, access on PAGE will cause page fault. */
void
page_destroy (struct page *page)
{
  ASSERT (page != NULL);

  pagedir_clear_page (page->pagedir, page->uaddr);
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