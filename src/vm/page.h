#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "devices/block.h"

/* Page records. */
struct pagerec;

/* States of a page. */
enum page_state
  {
    PAGE_LOADED,
    PAGE_SWAPPED,
    PAGE_UNLOADED
  };

/* A virtual page. */
struct page 
  {
    enum page_state state;
    void *uaddr;
    uint32_t *pagedir;
    int size;
    bool writable;
    block_sector_t sector;
    struct hash_elem elem;
  };

/* Basic operations on pages. */
struct page *page_from_memory (void *, bool writable);
struct page *page_from_file (void);
struct page *page_from_swap (void);
void page_destroy (struct page *);

void page_mark_present (struct page *);

/* States of a page, stored in page directory. */
bool page_is_accessed (struct page *);
bool page_is_dirty (struct page *);

#endif /* vm/page.h. */