#include "vm/swap.h"
#include <bitmap.h>
#include "devices/block.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

/* Global swap table. */
struct bitmap *swap_table;

/* Lock used for swap table. */
struct lock swap_table_lock;

/* Initializes swap table. */
void
swap_init (void)
{
  block_sector_t sectors = block_size (block_get_role (BLOCK_SWAP));
  size_t slots = sectors * BLOCK_SECTOR_SIZE / PGSIZE;

  if ((swap_table = bitmap_create (slots)) == NULL)
    PANIC ("Failed to allocate swap table.");

  lock_init (&swap_table_lock);
}

/* Allocates a swap slot by scanning swap table. If there's no swap slot 
   available, panic the kernel since there's no way to return error code.
   (block_sector_t is unsigned, hence it's not able to set the return value to 
   -1.) */
block_sector_t
swap_allocate (void)
{
  lock_acquire (&swap_table_lock);
  size_t free_slot = bitmap_scan_and_flip (swap_table, 0, 1, false);
  lock_release (&swap_table_lock);

  if (free_slot == BITMAP_ERROR)
    PANIC ("Failed to allocate swap slot.");
  
  return free_slot * PGSIZE / BLOCK_SECTOR_SIZE;
}

/* Frees SLOT from swap table. SLOT should not be page aligned, but this
   function automatically truncates it into the nearest slot boundary, freeing
   all contents in the swap slot. */
void
swap_free (block_sector_t slot_)
{
  size_t slot = slot_ / BLOCK_SECTOR_SIZE;

  bitmap_reset (swap_table, slot);
}