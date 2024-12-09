#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <mapid.h>
#include <stdint.h>
#include <list.h>
#include "filesys/file.h"

/* A file-memory mapping. */
struct mapping
  {
    uint8_t *uaddr;          /* Starting user address. */
    int pages;               /* The number of pages belong to this mapping. */
    mapid_t mapid;           /* Mapping identifier. */
    struct file *file;       /* File for this mapping. */
    struct list_elem elem;   /* List element. */
  };

void syscall_init (void);

#endif /* userprog/syscall.h */
