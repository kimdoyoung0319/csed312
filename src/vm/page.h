#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <hash.h>
#include "devices/block.h"
#include "filesys/off_t.h"

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

/* Basic operations on page records. */
struct pagerec *pagerec_create (void);
void pagerec_destroy (struct pagerec *);

/* Modification and access for page records. */
void pagerec_set_page (struct pagerec *, struct page *);
struct page *pagerec_get_page (struct pagerec *, void *);
void pagerec_clear_page (struct pagerec *, struct page *);

/* Basic operations on pages. */
struct page *page_from_memory (void *, bool);
struct page *page_from_file (void *, bool, struct file *, off_t);
struct page *page_from_swap (void);
void page_destroy (struct page *);

/* Modifications on pages. */
void page_set_present (struct page *, bool);
void page_swap_out (struct page *);
void *page_swap_in (struct page *);

/* States of a page, stored in page directory. */
bool page_is_accessed (struct page *);
bool page_is_dirty (struct page *);

#endif /* vm/page.h. */