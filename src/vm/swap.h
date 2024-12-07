#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "devices/block.h"

void swap_init (void);
block_sector_t swap_allocate (void);
void swap_free (block_sector_t);

#endif /* vm/swap.h */