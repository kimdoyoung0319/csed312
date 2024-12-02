#ifndef VM_SPT_H
#define VM_SPT_H

#include <hash.h>
#include <lib/user/syscall.h>
#include <lib/stdbool.h>
#include "devices/block.h"

/* Supplemental page table element that represents a page that is either 
   swapped out of not loaded. */
struct spte 
  {
    size_t size;             /* Total size of this page. */
    bool swapped;            /* Has this page swapped out? */
    bool writable;           /* Is this page writable? */
    void *uaddr;             /* The starting address of this page. */
    mapid_t mapid;           /* Map identifier for memory-mapped files. */
    block_sector_t index;    /* Starting sector's index of this block. */
    struct hash_elem elem;   /* Hash element for SPT. */
  };

/* Basic operations on SPT. */
struct spte *spt_make_entry (void *, int, block_sector_t);
void spt_free_entry (struct spte *);
struct spte *spt_lookup (void *);

/* Auxiliary functions to be used for hash table. */
unsigned spt_hash (const struct hash_elem *, void *);
bool spt_less_func (const struct hash_elem *, const struct hash_elem *, 
                    void *);

#endif /* vm/spt.h. */