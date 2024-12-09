#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdbool.h>
#include "vm/page.h"

/* Basic operations on frames. */
void frame_init (void);
void *frame_allocate (struct page *);
void frame_free (void *);

/* Pinning and unpinning. */
void frame_pin (void *);
void frame_unpin (void *);

#endif /* vm/frame.h */