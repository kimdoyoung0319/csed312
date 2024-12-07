#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <hash.h>
#include "devices/block.h"
#include "filesys/file.h"
#include "filesys/off_t.h"

/* Page records. */
struct pagerec;

/* States of a page. */
enum page_state
  {
    PAGE_PRESENT,     /* In memory, backing device is swap block. */
    PAGE_SWAPPED,     /* In swap block device. */
    PAGE_LOADED,      /* In memory, backing device is file system block. */
    PAGE_UNLOADED,    /* In file system block device. */
  };

/* A virtual page. */
struct page 
  {
    enum page_state state;     /* State of this page. */
    void *uaddr;               /* User virtual address of this page. */
    uint32_t *pagedir;         /* Page directory where this page lies in. */
    int size;                  /* Size of this page within PGSIZE. */
    int offset;                /* Offset within a sector in bytes. */
    bool writable;             /* Is this page writable? */
    block_sector_t sector;     /* The sector number of underlying block. */
    struct hash_elem elem;     /* Hash element. */
  };

/* Basic operations on page records. */
struct pagerec *pagerec_create (void);
void pagerec_destroy (struct pagerec *);

/* Modification and access for page records. */
void pagerec_set_page (struct pagerec *, struct page *);
struct page *pagerec_get_page (struct pagerec *, const void *);
void pagerec_clear_page (struct pagerec *, struct page *);

/* Basic operations on pages. */
struct page *page_from_memory (void *, bool);
struct page *page_from_file (void *, bool, struct file *, off_t, size_t);
struct page *page_from_swap (void *, bool, block_sector_t);
void page_destroy (struct page *);

/* Modifications on pages. */
void *page_swap_in (struct page *);
void page_swap_out (struct page *);

/* States of a page, stored in the page directory. */
bool page_is_accessed (const struct page *);
bool page_is_dirty (const struct page *);

#endif /* vm/page.h */