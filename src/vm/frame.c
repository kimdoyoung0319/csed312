/* TODO : add frame table functions */
#include "frame.h"

struct list ft;
struct lock ft_lock;
struct list_elem *e;
// struct list_elem *ft_clock; -> 추후 swap out (eviction algorithm) 시 구현할 예정

struct bitmap *st;
struct lock st_lock;

void 
ft_init(void)
{
  list_init (&ft);
  lock_init (&ft_lock);
  e = NULL;
}

void *
ft_get_frame(void)
{
  void *kaddr = NULL;
  
  lock_acquire (&ft_lock);
  struct fte *this = (struct fte *) malloc (sizeof (struct fte));

  if (this == NULL)
    return NULL;
  
  this->process = thread_current ()->process;
  
  kaddr = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kaddr == NULL)
  {
    //swap_out 실행해줘야 한다

    kaddr = palloc_get_page (PAL_USER | PAL_ZERO);
    //CHECK : error handling?? 여기서 또 실패할 경우를 확인해야 하나?
  }

  this->kaddr = kaddr;

  list_push_back (&ft, &this->elem);
  lock_release (&ft_lock);

  return kaddr;
}

void
ft_free_frame (void *kaddr)
{
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

  // CHECK : 필요한 부분인지 확인
  // if (this == NULL)
  //   sys_exit (-1);
  
  list_remove (&this->elem);
  palloc_free_page (this->kaddr);
  free (this);
  // CHECK : pagedir_clear_page 

  lock_release (&ft_lock);
}

void
st_init (void)
{
  st = bitmap_create ((block_size(block_get_role(BLOCK_SWAP))) / 8);
  bitmap_set_all (st, false);
  lock_init (&st_lock);
}

void
st_in (block_sector_t index, void *kaddr)
{
  ASSERT (kaddr != NULL);

  lock_acquire (&st_lock);
  if (!bitmap_test (st, index))
  { 
    lock_release (&st_lock);
    return;
  }
  bitmap_set (st, index, false);
  lock_release (&st_lock);

  block_sector_t size = 0;
  while (size < 8)
    block_read(block_get_role(BLOCK_SWAP), 
              index * 8 + size, 
              kaddr + (size++ * 512));

}

block_sector_t
st_out (void *kaddr)
{
  ASSERT (kaddr != NULL)

  lock_acquire (&st_lock);
  block_sector_t index = bitmap_scan (st, 0, 1, false);
  if (index == BITMAP_ERROR)
  {
    lock_release (&st_lock);
    return -1;
  }
  bitmap_set (st, index, true);
  lock_release (&st_lock);

  block_sector_t size = 0;
  while (size < 8)
    block_write(block_get_role(BLOCK_SWAP), 
                index * 8 + size, 
                kaddr + (size++ * 512));

  return index;
}