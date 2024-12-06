#ifndef VM_SPT_H
#define VM_SPT_H

#include <hash.h>
#include <lib/stdbool.h>
#include "userprog/mapid_t.h"
#include "userprog/pagedir.h"
#include "devices/block.h"
#include "vm/frame.h"

typedef int32_t off_t;
typedef int32_t off_t;
/* Supplemental page table element that represents a page that is either 
   swapped out of not loaded. */
struct spte 
  {
    size_t size;             /* Total size of this page. */
    bool swapped;            /* Has this page swapped out? */
    bool writable;           /* Is this page writable? */
    void *uaddr;             /* The starting address of this page. */
    mapid_t mapid;           /* Map identifier for memory-mapped files. */
    struct file *file;
    int index;
    bool lazy;
    off_t ofs;
    struct hash_elem elem;   /* Hash element for SPT. */
  };

/* Basic operations on SPT. */
struct spte *spt_make_entry (void *, int);
void spt_free_entry (struct spte *);
struct spte *spt_lookup (void *);

/* Functions for all elements in a SPT. */
void spt_free_hash (struct hash_elem *, void * UNUSED);

/* Auxiliary functions to be used for hash table. */
unsigned spt_hash (const struct hash_elem *, void *);
bool spt_less_func (const struct hash_elem *, const struct hash_elem *, 
                    void *);

#endif /* vm/spt.h. */