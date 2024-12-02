/* TODO : add frame table functions */
#include "frame.h"
struct lock ft_lock;
struct list ft;
// struct list_elem *ft_clock; -> 추후 swap out (eviction algorithm) 시 구현할 예정

void 
ft_init(void)
{
  list_init (&ft);
  lock_init (&ft_lock);
  // ft_clock = NULL; -> TODO : swap out (eviction algorithm)
}

void *
ft_get_frame(void)
{
  void *kaddr = NULL;
  
  lock_acquire (&ft_lock);
  struct fte *this = (struct fte *) malloc (sizeof (struct fte));

  if (this == NULL)
    return NULL;

  while (kaddr == NULL)
  {
    kaddr = palloc_get_page (PAL_USER | PAL_ZERO);

    if (kaddr == NULL) //여기서 swap out 부분 구현해야 한다
    {
      free (kaddr);
      lock_release (&ft_lock);
      return NULL;
    }
  }
  
  this->kaddr = kaddr;
  this->process = thread_current ()->process;

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
  //list 순회하면서 this 찾아주기
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
