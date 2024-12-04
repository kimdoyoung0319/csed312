/* Frame table implementation. */
#include "frame.h"
#include "threads/vaddr.h"

/* List of frame table, frame table lock and cursor for clock algorithm  */
struct list ft;
struct lock ft_lock;
struct list_elem *ft_clock;

/* Swap table bitmap and Swap Table Lock */
struct bitmap *st;
struct lock st_lock;

void 
ft_init (void)
{
  list_init (&ft);
  lock_init (&ft_lock);
  ft_clock = NULL;
}

void *
ft_get_frame (void *uaddr)
{
  ASSERT (uaddr != NULL);

  void *kaddr = NULL;

  lock_acquire (&ft_lock);

  struct fte *this = (struct fte *) malloc (sizeof (struct fte));
  
  if (this == NULL)
    {
      lock_release (&ft_lock);
      return NULL;
    }

  kaddr = palloc_get_page (PAL_USER | PAL_ZERO);
  /* If user pool is full */
  if (kaddr == NULL)
    {
      /* Implement based on clock algorithm */
      struct fte *out_fte;
      for (;;ft_clock = list_next (ft_clock))
        {
          if (ft_clock == list_end (&ft))
            ft_clock = list_begin (&ft);

          out_fte = list_entry (ft_clock, struct fte, elem);
          if (!pagedir_is_accessed (out_fte->process->pagedir, out_fte->uaddr))
            {
              /* Update position for clock algorithm */
              ft_clock = list_next (ft_clock);
              if (ft_clock == list_end (&ft))
                ft_clock = list_begin (&ft);
              break;
            }

          pagedir_set_accessed (out_fte->process->pagedir, out_fte->uaddr, false);
        }

      /* Check disk number for swap out and write to actual block */

      block_sector_t index = st_out ();
      if (index == BLOCK_FAILED) 
        {
          lock_release (&ft_lock);
          return NULL;
        }
      index = index * (PGSIZE / BLOCK_SECTOR_SIZE);

      block_sector_t size = 0;
      for (size = 0; size < PGSIZE / BLOCK_SECTOR_SIZE;)
        block_write (block_get_role (BLOCK_SWAP), 
                    index + size, 
                    out_fte->kaddr + (size++ * BLOCK_SECTOR_SIZE));

      /* Update spte and free the frame */
      struct spte *out_spte = spt_lookup (out_fte->uaddr);
      out_spte->swapped = true;
      out_spte->index = index;
      ft_free_frame (out_fte->kaddr);

      /* Try allocation again */
      kaddr = palloc_get_page (PAL_USER | PAL_ZERO);

      /* Return NULL if allocation fails */
      if (kaddr == NULL)
      {
        lock_release (&ft_lock);
        return NULL;
      }
    }
  
  this->process = thread_current ()->process;
  this->uaddr = uaddr;
  this->kaddr = kaddr;
  list_push_back (&ft, &this->elem);

  /* Initialize ft_clock if it's NULL */
  if (ft_clock == NULL)
    ft_clock = list_begin (&ft);

  lock_release (&ft_lock);

  return kaddr;
}

void
ft_free_frame (void *kaddr)
{
  bool chk = lock_held_by_current_thread (&ft_lock);

  if (!chk)
    lock_acquire (&ft_lock);

  struct fte *this;
  struct list_elem *e;
  for (e = list_begin (&ft); e != list_end (&ft); 
       e = list_next (e))
    {
      this = list_entry (e, struct fte, elem);

      if (this->kaddr == kaddr)
        break;
    }

  /* CHECK : Verify if this section is necessary */
  /* if (this == NULL)
     sys_exit (-1); */
  
  list_remove (&this->elem);
  palloc_free_page (this->kaddr);
  pagedir_clear_page (this->process->pagedir, this->uaddr);
  free (this);

  if (!chk)
    lock_release (&ft_lock);
}

void
st_init (void)
{
  st = bitmap_create ((block_size (block_get_role (BLOCK_SWAP))) / (PGSIZE / BLOCK_SECTOR_SIZE));
  bitmap_set_all (st, false);
  lock_init (&st_lock);
}

void
st_in (block_sector_t index)
{
  lock_acquire (&st_lock);
  if (!bitmap_test (st, index))
    { 
      lock_release (&st_lock);
      return;
    }
  bitmap_set (st, index, false);
  lock_release (&st_lock);
}

block_sector_t
st_out (void)
{
  lock_acquire (&st_lock);
  block_sector_t index = bitmap_scan (st, 0, 1, false);
    
  if ((size_t) index == BITMAP_ERROR)
    {
      lock_release (&st_lock);
      return BLOCK_FAILED;
    }
  bitmap_set (st, index, true);
  lock_release (&st_lock);

  return index;
}