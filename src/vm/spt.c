#include <lib/user/syscall.h>
#include <hash.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/spt.h"

/* Makes basic SPTE which is from UADDR, refer to a disk sector INDEX, with 
   size of SIZE. */
struct spte *
spt_make_entry (void *uaddr, int size, block_sector_t index)
{
  ASSERT (size < PGSIZE);
  ASSERT (pg_round_down (uaddr) == uaddr);

  struct spte *new = (struct spte *) malloc (sizeof (struct spte));

  new->size = size;
  new->swapped = true;
  new->uaddr = uaddr;
  new->mapid = MAP_FAILED;
  new->index = index;

  return new;
}

/* Frees allocated space for SPTE. */
void
spt_free_entry (struct spte *spte)
{
  free (spte);
}

/* Finds SPTE with user address UADDR, under the current process's context. 
   Returns found SPTE if such user address exists on the SPTE of current 
   process. Returns null pointer if such user address does not exist. */
struct spte *
spt_lookup (void *uaddr)
{
  struct process *this = thread_current ()->process;

  ASSERT (this != NULL);

  struct spte entry;
  struct hash_elem *e;

  entry.uaddr = uaddr;
  e = hash_find (&this->spt, &entry.elem);

  return e ? hash_entry (e, struct spte, elem) : NULL;
}

/* Hash function to be used for SPT, which essensially computes hash for the
   UADDR for the SPTE. */
unsigned
spt_hash (const struct hash_elem *e, void *aux UNUSED)
{
  const struct spte *entry = hash_entry (e, struct spte, elem);
  int uaddr = (int) entry->uaddr;

  return hash_int (uaddr);
}

/* Less function for SPT. Returns true if A's address precedes B's. */
bool
spt_less_func (const struct hash_elem *a_, const struct hash_elem *b_, 
               void *aux UNUSED)
{
  const struct spte *a = hash_entry (a_, struct spte, elem);
  const struct spte *b = hash_entry (b_, struct spte, elem);

  return a->uaddr < b->uaddr;
}
