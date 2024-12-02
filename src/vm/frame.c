/* TODO : add frame table functions */
#include "frame.h"
struct lock ft_lock;
struct list ft;
struct list_elem *ft_clock;

void 
ft_init(void)
{
  lock_init (&ft_lock);
  lock_init (&ft);
  ft_clock = NULL;
}

void *
ft_get_frame(void *upage, enum palloc_flags flag)
{
  ASSERT (upage != NULL);

  void *kpage = NULL;

  // lock_acquire (&ft_lock);
  struct fte *this = (struct fte *) malloc (sizeof (struct fte));

  if (this == NULL)
    return NULL;

  while (kpage == NULL)
  {
    kpage = palloc_get_page (flag);

    if (kpage == NULL && !swap_out())
    {
      free (kpage);
      return NULL;
    }
  }
  
  this->kpage = kpage;
  this->process = thread_current ()->process;
  list_push_back (&ft, &this->elem);

  // lock_release (&ft_lock);
  return kpage;
}

void
ft_free_frame (void *kpage)
{
  ASSERT (kpage != NULL);

  struct fte *this;
  // lock_acquire (&ft_lock);

  //list 순회하면서 this 찾아주기

  if (this == NULL)
    sys_exit (-1);
  
  list_remove (&this->elem);
  palloc_free_page (this->kpage);

  // pagedir_clear_page 를 해줘야 하지 않을까?

  free (this);
  // lock_release (&ft_lock);
}
