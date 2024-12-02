#ifndef VM_SPT_H
#define VM_SPT_H

#include <hash.h>
#include <lib/user/syscall.h>
#include <lib/stdbool.h>
#include "devices/block.h"

/* Supplemental page table element. */
struct spte 
  {
    int size;
    bool swapped;
    void *uaddr;
    mapid_t mapid;
    block_sector_t index;
    struct hash_elem elem;
  };

/* Basic operations on SPT. */
struct spte *spt_make_entry (void *, int, block_sector_t);
void spt_free_entry (struct spte *);
struct spte *spt_lookup (void *);

/* Auxiliary functions to be used for hash table. */
unsigned spt_hash (const struct hash_elem *, void *);
bool spt_less_func (const struct hash_elem *, const struct hash_elem *, 
                    void *);

#endif