#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdbool.h>
#include "vm/page.h"

void frame_init (void);
void *frame_allocate (struct page *);
void frame_free (void *);

#endif /* vm/frame.h */