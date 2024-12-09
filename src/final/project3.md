# Project 3 - Final Report (Team 28)

## Task 1 - Frame Table
### Improvement
<!-- TODO: To be filled by Taeho. -->


### Newly Introduced Functions and Data Structures
#### `struct frame`
```C
/* Global frame list. */
struct list frames;

/* Lock for frame table. */
struct lock frames_lock;

/* A physical frame. */
struct frame
  {
    void *kaddr;
    struct page *page;
    bool accessed;
    struct list_elem elem;
  };
```
 우선 Frame Table은 실제 물리 RAM의 사용을 보다 효율적으로 하기 위함이다.
또한, RAM은 모든 프로세스 상관 없이 접근이 가능한 공유되는 자원이므로 전역적으로 
선언했으며, 전체 프레임을 저장하기 위한 list frames 를 선언하였다. 또한, 전역적으로 
관리되는 만큼 당연히 접근 시에 sync 가 중요하므로 frames_lock 을 함께 선언하여 lock을 
통해서 여러 프로세스의 동시 접근을 관리하고자 한다.

 또한, frame을 관리하기 위해서 필요한 정보를 담고 있는 struct frame 을 선언하였다.
해당 frame 에서는 실제 RAM 에 할당된 페이지의 시작 주소를 가르키기 위한 *kadd,
User Page를 저장하기 위한 page pointer, 이후 clock 알고리즘의 구현을 위한 
accessed (최근 사용되었는지 여부 체크) 그리고 전역적으로 관리되는 frame 리스트에
원소로 저장하기 위한 elem으로 선언하였다.

#### `frame_init ()`
```C
/* Initialize global frame table. */
void
frame_init (void)
{
  list_init (&frames);
  lock_init (&frames_lock);
}
```
 가장 먼저 frame을 초기화하기 위한 frame_init() 이다. 해당 함수에서는 핀토스가 
부팅되는 과정에서 호출되어야 하며, 다른 user process 들이 frame 할당을 시작하기 전에
호출되어 리스트와 락을 초기화해준다. 실제 호출은 init.c 의 mian() 에서 호출되낟.

#### `frame_allocate ()`
```C
/* Allocates a frame from user pool associated with PAGE, evicting one if 
   needed. New frame will be filled with zeros. */
void *
frame_allocate (struct page *page)
{
  ASSERT (page != NULL);

  struct frame *frame;
  void *kaddr = palloc_get_page (PAL_USER | PAL_ZERO); 

  /* Luckily, there's room for a new frame. Set basic informations and 
     register it to the frame table. */
  if (kaddr != NULL) 
    {
      frame = (struct frame *) malloc (sizeof (struct frame));
      frame->accessed = false;
      frame->page = page;
      frame->kaddr = kaddr;

      lock_acquire (&frames_lock);
      list_push_back (&frames, &frame->elem);
      lock_release (&frames_lock);

      return kaddr;
    }
  ...
}
```

 다음으로 구현한 함수는 새로운 프레임을 할당하는 함수이다. 입력으로는 user page의 주소를 
입력받아 user pool 에서 남아있는 공간을 할당하여 새로운 프레임을 생성하게 된다.
함수의 구현이 길어져 두 부분으로 나눠서 소개하고자 한다. 첫 부분은 새로운 페이지를 할당하는 것이다.
PAL_USER | PAL_ZERO 를 통해서 user pool 내에서 페이지를 할당하며, 0으로 초기화한다.
여기서 확인해야 할 부분이 바로 남은 영역이 있는지 여부이다. 만약 유저풀에서 남아있는 메모리가
존재한다면 할당이 되었으므로 새로운 frame 을 선언하고, frames 에 추가해주게 된다.
만약 남아있지 않은 경우는 밑에서 소개하고자 한다.

```C
void *
frame_allocate (struct page *page)
{
  ...

  /* Initial attempt to allocate a frame has failed. Find a frame to be evicted
     by clock algorithm. */
  struct page *p;
  struct list_elem *e;

  /* Copies accessed bits from page table entries. */
  lock_acquire (&frames_lock);
  for (e = list_begin (&frames); e != list_end (&frames); e = list_next (e))
    {
      frame = list_entry (e, struct frame, elem);
      p = frame->page;

      frame->accessed = page_is_accessed (p);
    }

  /* Find the frame to be evicted by clock algorithm. */
  e = list_begin (&frames);
  while (true)
    {
      frame = list_entry (e, struct frame, elem);

      if (frame->accessed)
        frame->accessed = false;
      else
        break;

      if (e == list_rbegin (&frames))
        e = list_begin (&frames);
      else
        e = list_next (e);
    }
  list_remove (&frame->elem);
  lock_release (&frames_lock);

  page_swap_out (frame->page);
  palloc_free_page (frame->kaddr);

  /* At this point, there's at least one free frame. Reset frame 
     informations. */
  frame->kaddr = palloc_get_page (PAL_USER | PAL_ZERO);
  frame->page = page;
  frame->accessed = false;

  lock_acquire (&frames_lock);
  list_push_back (&frames, &frame->elem);
  lock_release (&frames_lock);

  return frame->kaddr;
}
```
 만약 유저풀에 메모리가 남아있지 않다면, 기존에 사용되고 있는 메모리 영역 중에서 
사용되지 않았거나, 최근까지도 사용되지 않았던 데이터를 swap disk 상으로 옮기고, 
RAM 을 비워줘야 새로운 frame 을 할당할 수 있게 된다. 이를 위해서 먼저
현재 프로세스의 페이지 디렉토리가 가지고 있는 access 정보로 업데이트 해줘야 한다.
대부분 frame 에 있는 accessed 를 통해서 기록이 될 수 있지만, 혹시 기존에 
user process에서 해당 주소를 호출했던 기록으로 최신화해주기 위해서 for 를 수행한다.

 이후에는 clock 알고리즘에 따라서 방출될 프레임을 선정하는 단계이다. 리스트를 순회하며
accessed bit 를 확인하고 만약에 사용되었다면 false 로 바꿔주는 방식으로 LRU를
모방하여 가장 최근까지 사용되지 않았던 프레임을 선택하고, 제거하게 된다. 제거하는 것이
지운다는 것이 아니라 정확히는 swap disk 상에 데이터를 옮겨주고, 대신에 RAM 은 사용할
수 있도록 하는 것이므로, page_swap_out () 을 통해 user page의 데이터를 
옮겨준다. 또한, 이 때 swap table 에 업데이트가 동반되며, 해당 함수의 구현은 
하단에서 자세히 소개하고자 한다. 이렇게 최소 한 개의 프레임이 비워져 있으므로, 
새로운 프레임을 할당하고, 이 프레임을 리스트에 등록하게 되며 새로운 프레임을 할당하게 된다.

```C
/* userprog/process.c */
setup_stack ()
{
  /* original - project #2 */
  kapge = palloc_get_page (PAL_USER | PAL_ZERO);

  /* modified - project #3 */
  kaddr = frame_allocate (upage);
}

```

 이렇게 선언된 frame_allocate () 함수는 Lazy loading과도 함께 고려하여, 
실제 해당 주소가 필요한 경우에만 호출된다. 따라서 직접적으로 호출되는 부분을 살펴본다면
기존 구현에서는 process.c 의 setup_stack () 내에서 실행될 user process의
실행 파일을 읽어와서 저장하게 되는 부분에서 작동되며, 위와 같이 대체되어, 간접적으로 
frame table을 통해 RAM 에서 page를 할당하게 된다. 이외에도 page_load() 와
page_swap_in () 등 직접적으로 해당 주소에 쓰거나 읽기가 실행되는 부분에서 
호출되어 새로운 프레임을 할당하게 된다.

#### `frame_free ()`
```C
/* Frees a physical frame associated with KADDR. If the frame for KADDR is
   not registered into the frame table, does nothing. */
void
frame_free (void *kaddr)
{
  struct list_elem *e;
  struct frame *frame = NULL;

  lock_acquire (&frames_lock);
  for (e = list_begin (&frames); e != list_end (&frames); e = list_next (e))
    {
      frame = list_entry (e, struct frame, elem);

      if (frame->kaddr == kaddr)
        break;
    }

  if (frame == NULL || frame->kaddr != kaddr)
    {
      lock_release (&frames_lock);
      return;
    }
  
  list_remove (&frame->elem);
  lock_release (&frames_lock);

  palloc_free_page (frame->kaddr);
  free (frame);
}
```

 상대적으로 간단한 함수이다. 기존에 할당된 프레임을 해제하는 함수로, kernel 
에 할당된 프레임의 주소를 입력받아 그 주소를 바탕으로 프레임 테이블에 저장되어 있는 
프레임을 삭제해주고, 실제 유저 풀에 할당된 페이지 또한 해제해주게 된다. 
위에서 구현된 free와 allocate 모두 frmaes_lock 을 통해서 
공유자원인 frames에 접근을 통제하게 된다.

### Differences with Design Reports
 디자인 레포트에서는 이 부분에서 조금 더 많은 역할을 담아야 할 것이라고 
생각했다. 예를 들어 process 를 저장하여 process 내에 저장되어 있는
페이지 디렉토리 업데이트를 이 함수 내부에서 진행을 해줘야 한다고 생각했는데,
실제로 구현하는 과정에서는 supplement page table 과 더불어 봤을 때
호출하는 함수에서 구현하는 것이 조금 더 역할에 충실한 것이라 생각되어 
분할하게 되었다.

### Things We Have Learned & Limitations
 디자인 레포트를 작성하면서도 고민을 해봤지만, 핀토스에서 어떻게 메모리를
할당하고, 할당된 메모리를 사용하는지에 대해서 고민해보게 되었다. 
유저 풀에서 할당되는 방식과 더불어 유저풀에서 할당되었을 때 어떻게 
이를 관리해 줄 것인지에 대한 고민을 거듭하게 되었다. 또한, 다른 프로젝트3
의 구현과 함께하였을 때 setup_stack() 에서는 직접적으로 할당하지 않는다면,
애초에 유저 프로세스가 실행 자체가 되지 않으므로 이를 위해서도 직접 호출하여
할당하는 방식으로 구현을 마무리할 수 있었다.

## Task 2 - Lazy Loading
<!-- TODO: To be filled by Doyoung. -->
### Improvement
<!-- TODO: To be filled by Doyoung. -->
### Newly Introduced Functions and Data Structures
<!-- TODO: To be filled by Doyoung. -->
### Differences with Design Reports
<!-- TODO: To be filled by Doyoung. -->
### Things We Have Learned
<!-- TODO: To be filled by Doyoung. -->
### Limitations
<!-- TODO: To be filled by Doyoung. -->

## Task 3 - Supplemental Page Table
<!-- TODO: To be filled by Doyoung. -->
### Improvement
<!-- TODO: To be filled by Doyoung. -->
### Newly Introduced Functions and Data Structures
<!-- TODO: To be filled by Doyoung. -->
### Differences with Design Reports
<!-- TODO: To be filled by Doyoung. -->
### Things We Have Learned
<!-- TODO: To be filled by Doyoung. -->
### Limitations
<!-- TODO: To be filled by Doyoung. -->

## Task 4 - Stack Growth
### Improvement
 기존 프로젝트2 에서는 page fault 가 발생하게 된다면 단순히 프로세스를
종료하게 되었다. 즉, 유저 프로세스 상에서 할당되지 않은 영역에 접근을 
시도하기만 해도 종료되었던 반면, 프로젝트 3에서 요구했던 것은 유저 프로세스가
유저 스택 포인터와 비교하여 4 byte ~ 32 byte 까지 더 큰 경우에는
stack 을 키워주도록 권고하고 있다. push, pusha 과 같은 명령어가 실행된다면
실제 user stack 을 키워줘야 하는 것이다. 하지만 여기서 유념할 부분은
사용하고자 하는 영역의 주소가 정말 위에서 말한 경우에서만 해당되는지 잘 확인하여
이를 통해서 stack growth 를 구현하였다.

### Newly Introduced Functions and Data Structures
```C
/* userprog/exception.c */
/* Limitation of user stack. */
#define USER_STACK_BOUNDARY ((uint8_t *) PHYS_BASE - 8 * 1024 * 1024)

static void
page_fault (struct intr_frame *f) 
{
  ...

  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

  void *uaddr = pg_round_down (fault_addr);  
  struct process *this = thread_current ()->process; 
  struct page *upage = pagerec_get_page (this->pagerec, uaddr);

  /* Save stack pointer on the initial transition to kernel mode. */
  if (is_user_vaddr (f->esp))
    this->esp = f->esp;
  
  /* Extend stack when stack growth is needed. */
  if (upage == NULL
      && fault_addr >= USER_STACK_BOUNDARY 
      && this->esp - fault_addr <= 32 
      && is_user_vaddr (fault_addr))
    {
      struct page *st = page_from_memory (uaddr, true);
      pagerec_set_page (this->pagerec, st);
      pagedir_set_page (this->pagedir, uaddr, frame_allocate (st), true);
      
      return;
    }
  
  ...
}
```
 사실 이미 page_fault() 함수 자체는 굉장히 자주 등장했고, 등장할 예정이기에
일부분만 발췌해서 소개하고자 한다. 결국 stack growth 상황에서는 항상 인터럽트가
발생하여 page_fault() 가 실행되어야 한다. 따라서 page_fault() 내에서는 
if else 를 통해서 여러 상황 (lazy loading, stack growth 등)을 구별하고
이를 토대로 처리하게 된다. stack growth 가 실행되는 상황은 다음과 같다.
우선 pagerec_get_page() 를 통해서 supplement page table을 확인하고
기존에 할당되지 않았는지 확인하게 된다. 이외에도 핀토스 문서에서 제시했던 8 MB의 
크기 제한과 최대 32 bytes 까지의 제한, 그리고 user address 인지를 확인하여 
전부 만족하는 경우에만 스택을 키우게 된다.

 여기서 중요한 부분이 있는데, 인터럽트 프레임에 저장되어 있는 esp가 유저 프로세스의
esp와 동일한지가 미지수일 수 있다. 만약 Page_fault () 실행 중 page_fault가
다시 발생한다던가 하는 경우에는 f의 esp는 커널 스텍을 지칭하고 있게 된다. 따라서 
이를 위해서는 process의 esp 를 따로 백업해주는 과정이 필요한데 f->esp가
유저 스텍인 경우에는 this->esp 에 저장하는 방식으로 페이지 폴트가 발생하게 되면
항상 업데이트해주게 된다. 따라서 이후에 커널 모드에서 페이지 폴트가 재발생한 경우에도
this->esp 를 참조하게 된다면, 유저 스텍의 주소를 불러올 수 있다.

 이제 자세한 실행은 page_frome_memory() 를 통해 새로운 구조체 page를 할당하고,
이를 현재 process 상에 기록해주게 된다. 보다 자세히는 struct pagerec을 통해
저장하고, 해당 해시 테이블에 page를 등록하게 된다. 다음으로 생각해보면, 이 경우에는
바로 사용해야 하는 페이지가 필요하다. 즉, stack 에서 즉시 프레임을 필요로 하므로,
frame_allocate() 와 paggedir_set_page() 를 바로 호출하여 유저풀에서
페이지를 할당하고, 프레임을 할당하고 즉시 stack에 write가 가능하도록 구현했다.
마지막으로 당연히 유저 스텍이므로 쓰기가 가능해야 하므로, writable을 true로 구현했다.
 
### Differences with Design Reports
 디자인 레포트 상에서는 esp를 백업해주기 위해서 다양한 것들을 고민했었다. 
정확히는 syscall_handler() 를 통해 항상 user -> kernel 모드로 변경이
이뤄지기 때문에, 이 함수가 호출될 때마다 업데이트해주도록 디자인을 했었지만
실질적으로 페이지 폴트가 발생한 경우 외에는 f->esp 를 사용한다고 해서 오류나
문제가 발생한 경우가 없다는 것을 알게 되었고, 따라서 페이지 폴트 함수 내에서
업데이트를 해주고, 이후 실행하는 방식으로 esp 를 백업해주게 되었다.

### Limitations
 보다 더 많은 체크가 필요할 수도 있겠다는 생각을 하기도 했다. 현재 테스트 케이스
상에서는 모든 테스트를 만족하지만 상황에 따라 fault_address를 보다
명확하게 확인해야 페이지 폴트를 정확하게 핸들링할 수 있을 것이라는 생각이 든다. 
스택이 커져야 하는 상황이지만, 저 if 문을 통과하지 못했던 경우와
스택이 커져야 하는 상황이지만, if 문을 통과하는 경우 등 반례를 찾지는 못했지만,
일부 상황에서 혹은 실행 환경에 따라 문제 발생의 여지가 있다고 생각된다.

## Task 5 - File Memory Mapping
<!-- TODO: To be filled by Doyoung. -->
### Improvement
<!-- TODO: To be filled by Doyoung. -->
### Newly Introduced Functions and Data Structures
<!-- TODO: To be filled by Doyoung. -->
### Differences with Design Reports
<!-- TODO: To be filled by Doyoung. -->
### Things We Have Learned
<!-- TODO: To be filled by Doyoung. -->
### Limitations
<!-- TODO: To be filled by Doyoung. -->

## Task 6 - Swap Table
 스왑 테이블은 매우 간단히 스왑 디스크를 관리해주기 위한 도구이다.
당연하게도 스왑 테이블은 전역적으로 관리해야 하므로, 이를 고려하여 
스왑 테이블을 구현하였다.

### Improvement
 기존 프로젝트 2 까지는 아예 스왑 테이블 자체가 존재하지 않았다.
스왑 디스크를 사용하지 않았으며, 스왑 디스크를 활용하여 메모리를
확장하여 사용하지 못했다. 따라서 이번에 구현한 스왑 테이블을 통해서 
프레임(페이지)이 사용되지 않아 비워지는 경우 스왑 디스크에 저장하고
이를 스왑 테이블에 기록하여 비어있는 스왑 디스크 영역을 관리하도록 구현하였다.
이를 통해 효과적으로 스왑 디스크를 사용할 수 있다.

### Newly Introduced Functions and Data Structures
#### `struct swap_table`
```C
/* Global swap table. */
struct bitmap *swap_table;

/* Lock used for swap table. */
struct lock swap_table_lock;
```
 우선 스왑 테이블은 공식 문서에 따라서 bitmap 으로 구현하였다. 스왑 디스크를
페이지 크기 단위로 나눠 각 슬롯이 이용되고 있는지 여부만 저장하면 되므로,
비트맵이 가장 효과적인 데이터 타입이라고 생각했다. 또한, 전역적으로 설정되어 
있는 구조체이므로 sync를 위해서는 lock 을 통해서 마찬가지로 관리해주게 된다.

#### `swap_init()`
```C
/* vm/swap.c */
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
```
 스왑 테이블을 초기화해주기 위한 함수이다. 전역적으로 관리되므로 프레임 테이블과 마찬가지로
init.c 의 main() 에서 함께 호출해주며, 초기화 시에는 BLOCK_SWAP으로 등록되어 있는
모든 섹터 수를 들고와서 한 페이지를 이루기 위한 섹터의 크기 (현재 핀토스 상에서는 8개)를
바탕으로 slots 을 계산한다. 이후 슬롯의 개수만큼 스왑 테이블을 선언하게 된다.

#### `swap_allocate()`
```C
/* vm/swap.c */
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
```
 스왑 디스크에 프레임이 swap out 되어 저장되는 경우에 호출되는 함수로, 비어있는
스왑 슬롯을 계산하여 반환하게 되는 방식으로 구현했다. 마찬가지로 전역적으로 선언되어 있으므로
Lock 을 통해서 관리해준다. 


```C
/* vm/swap.c */
/* Swaps PAGE out from the memory to the swap device. It evicts PAGE from the
   page directory associated with it. PAGE must be in the present state and 
   must not be a null pointer. */
void
page_swap_out (struct page *page)
{
  ASSERT (page != NULL);
  ASSERT (page->state == PAGE_PRESENT);

  uint8_t *kpage = pagedir_get_page (page->pagedir, page->uaddr);
  uint8_t *kaddr = kpage;

  struct block *block = block_get_role (BLOCK_SWAP);
  block_sector_t slot = swap_allocate ();

  for (block_sector_t sector = slot; 
       sector < slot + PGSIZE / BLOCK_SECTOR_SIZE; 
       sector++)
    {
      block_write (block, sector, kaddr);
      kaddr += BLOCK_SECTOR_SIZE;
    }

  page->state = PAGE_SWAPPED;
  page->sector = slot;
  pagedir_clear_page (page->pagedir, page->uaddr);
  frame_free (kpage);
}
```

 실제 함수의 호출은 page_swap_out() 에서 호출된다. swap out 과정에서는 
입력된 페이지를 바탕으로 실제 커널에 저장된 페이지의 주소를 찾아서 그 주소에 있는
데이터를 swap disk 로 옮겨서 저장하는 과정을 수행하게 되어야 한다. 
실질적으로 swap out 의 작동 자체는 page_swap_out() 에서 수행되는데,
swap disk 의 block 을 입력받아서, 위에서 구현된 swap_allocate() 를 통해
swap out 을 수행할 slot 을 확인하게 된다. 이를 토대로 block에 써서
page의 내용을 기록하게 되고, 페이지의 현재 상태와 섹터를 기록하게 된다. 

#### `swap_free()`
```C
/* vm/swap.c */
/* Frees SLOT from swap table. SLOT should not be page aligned, but this
   function automatically truncates it into the nearest slot boundary, freeing
   all contents in the swap slot. */
void
swap_free (block_sector_t slot_)
{
  size_t slot = slot_ * BLOCK_SECTOR_SIZE / PGSIZE;

  bitmap_reset (swap_table, slot);
}
```
 swap free 함수의 경우에는 swap slot 을 할당 해제해주는 함수로, bitmap_reset()을
통해 입력받은 slot의 bit를 false로 설정하게 된다. swap_allocate()와 swap_free()는
모두 인터페이스에 가까운 함수로 구현하고, 실제 swap in 이 수행되는 과정은 밑의 함수를 통해
구현했다. bitmap_reset () 함수 자체가 atomic 하게 작동되므로 따로 lock 을 통해서 
관리하지 않았다.

```C
/* vm/page.c */
/* Fetchs PAGE from the swap device. The state of the page must be swapped out 
   state. This function adds mapping from PAGE's user address to newly 
   allocated frame to associated page directory. Returns newly allocated kernel 
   address if it succeed, a null pointer if failed. */
void *
page_swap_in (struct page *page)
{
  ASSERT (page != NULL);
  ASSERT (page->state == PAGE_SWAPPED);

  struct block *block = block_get_role (BLOCK_SWAP);
  uint8_t *kpage = (uint8_t *) frame_allocate (page);
  uint8_t *kaddr = kpage;

  if (kaddr == NULL)
    return NULL;

  /* Fetch the page from the block device. */
  for (block_sector_t sector = page->sector; 
       sector < page->sector + PGSIZE / BLOCK_SECTOR_SIZE;
       sector++)
    {
      block_read (block, sector, kaddr);
      kaddr += BLOCK_SECTOR_SIZE;
    }

  /* Fetched a frame for the page. Change state, free the swap slot, and make a 
     mapping in page directory. */
  page->state = PAGE_PRESENT;
  swap_free (page->sector);
  pagedir_set_page (page->pagedir, page->uaddr, kpage, page->writable);

  return kaddr;
}
```
 이 함수가 바로 실제 swap in 을 수행하는 함수로 위에서 일부 소개했지만, 다시 불러올 페이지를
입력으로 받아서 page에 저장되어 있는 block sector를 통해서 블록을 읽어와서 실제 프레임을
다시 할당받아 그 프레임에 저장하게 된다. 이 때 함수의 실행 이후 swap out이 된 sector를
사용이 가능하도록 표기하기 위해서 swap_free() 를 통해서 sector를 free 해준다.

### Differences with Design Reports
 디자인 레포트를 작성할 때만 해도, swap table과 관련된 모든 함수는 swap.c 내에서 작성하고자 했다.
즉, swap_free(), swap_allocate() 와 같은 함수를 통해서 실행하는 것이 아닌, 
스왑 테이블의 관리화 실행을 모두 swap.c 내에서 하려고 했지만, 실제 구현되는 과정에서 
struct page를 기반으로 작동되므로 보다 이상적으로는 swap_out(), swap_in() 모두
page.c 에서 구현하여 page를 통해서 swap_in과 out을 실행하도록 구현을 수정하게 되었다.
하지만 실제 작동되는 과정 자체는 디자인 레포트와 동일하다고 볼 수 있다.

### Limitations
 위에서 바뀐 방식이 과연 모든 케이스를 핸들링할 때 유용한지에 대해서는 재고할 필요성이 있다.
즉, 임의의 상황 발생으로 인해 page_swap_in()과 page_swap_out() 각각 실행 중에
다시 실행되게 된다면, 같은 sector로 읽어오거나 쓰는 등이 동시에 반복될 가능성이 존재한다.
따라서 이를 위해서 page_swap_in() 과 page_swap_out() 자체에 lock을 부여하여
보호하는 방법을 적용한다면 현재보단 일부 안정된 구현이라 생각할 수 있지만, 
많은 lock 이 사용될 수록 퍼포먼스에는 심각한 제약이 있을 것이고, 실제 함수의 로직에서는
만약 동시에 같은 block을 읽으려 해도, block_read() 내부에서 보호되며, ASEERT를 통해
PAGE_SWAPPED를 확인하게 되므로 오류는 사전에 차단할 수 있게 구현하였다.

## Task 7 - On Process Termination
 프로젝트 3에서 선언되고 사용되는 구조체는 상당히 많다. frame table 부터 시작해서,
swap table, memory mapped 와 관련된 구조체까지 메모리 영역을 확장하여 사용하기
위해서 다양한 구조체를 선언하여 활용하게 되는데, 그만큼 종료 시에 더 확실하게 확인해야 한다.
즉, 특정 프로세스가 종료될 때 frame table이 잘 할당 해제가 되는지, swap table 에서
다시 불러와서 메모리에 작성하는지 등등 여러 확인할 부분이 남아있게 된다. 따라서 
각 부분에 집중하여 이 부분의 구현을 진행하였다.

### Improvement
```C
/* userprog/process.c - previous version*/
void 
process_exit (int status)
{
  ... 

  for (e = list_begin (&this->children); e != list_end (&this->children); )
    {
      child = list_entry (e, struct process, elem);
      next = list_remove (e);
      child->parent = NULL;

      if (child->status == PROCESS_DEAD)
        destroy_process (child);

      e = next;
    }

  intr_set_level (old_level);
  
  /* Close opened files. */
  for (e = list_begin (&this->opened); e != list_end (&this->opened); )
    {
      next = list_remove (e);
      fp = list_entry (e, struct file, elem);
      file_close (fp);
      e = next;
    }

  ...
}
```
 기존 process_exit() 의 구현을 살펴보자. 여기서는 크게 두 가지에 초점을 맞춰서 할당
해제가 이뤄진다. 첫 째로는 this->children 을 기다리면서 각 children 이 실행이
종료될 때까지 기다리는 것을 수행하게 된다. 다음으로는 opend 된 파일 포인터를 순회하며
파일 포인터를 닫아주게 되고, 이후 종료하게 된다. 따라서 이 구현만으로는 역시
여러 구현된 구조체들과 데이터를 해제할 수 없게 된다. 따라서 다음의 내용을 구현하였다.

### Newly Introduced Functions and Data Structures
#### free pages
```C
/* userprog/process.c */
void
process_exit(int status)
{
  ...

  /* Destroys page record. */
  if (this->pagerec != NULL)
    pagerec_destroy (this->pagerec);

  ...
}

/* vm/page.c */ 
/* Destroys page records, destroying all pages belong to it. */
void
pagerec_destroy (struct pagerec *rec)
{
  hash_destroy (&rec->records, free_page);
  free (rec);
}

/* Destroys a single page associated with E. Used to destroy all pages in a 
   page record. */
static void
free_page (struct hash_elem *e, void *aux UNUSED)
{
  struct page *p = hash_entry (e, struct page, elem);

  if (p->state == PAGE_PRESENT && p->file != NULL)
    page_unload (p);

  page_destroy (p);
}

/* Unloads, or writes back PAGE into the underlying file. It evicts PAGE from 
   the page directory associated with it. PAGE must be in the present state and 
   must not be a null pointer, and must have file associated with it. */
void
page_unload (struct page *page)
{
  ASSERT (page != NULL);
  ASSERT (page->state == PAGE_PRESENT);
  ASSERT (page->file != NULL);

  void *buffer = palloc_get_page (0);
  uint8_t *kpage = pagedir_get_page (page->pagedir, page->uaddr);
  memcpy (buffer, kpage, page->size);
  
  if (page->writable && page_is_dirty (page))
    {
      lock_acquire (&filesys_lock);
      file_write_at (page->file, buffer, page->size, page->offset);
      lock_release (&filesys_lock);
    }

  palloc_free_page (buffer);
  page->state = PAGE_UNLOADED;
  pagedir_clear_page (page->pagedir, page->uaddr);
  frame_free (kpage);
}

/* Destroys PAGE, mark the page as not present in its associated page 
   directory, deleting it from current process's page record. This also evicts
   physical page frame associated with it, if such exists. 
   
   After this, accessing on PAGE will cause page fault, and the kernel won't 
   recover from it. This does NOT close a opened file. */
void
page_destroy (struct page *page)
{
  struct process *this = thread_current ()->process;

  ASSERT (page != NULL);
  ASSERT (this != NULL);

  void *kaddr;
  if ((kaddr = pagedir_get_page (page->pagedir, page->uaddr)) != NULL)
    frame_free (kaddr);

  pagedir_clear_page (page->pagedir, page->uaddr);
  pagerec_clear_page (this->pagerec, page);
  free (page);
}
```
 우선 프로세스가 종료될 때 어떻게 struct page를 해제하는지 확인해보도록 하자.
당연히 프로세스가 종료되는 process_exit() 을 통해 시작되며, pagerec_destory()
를 호출하게 된다. pagerec_destory() 는 process에 기록되어 있는 page들을 해제하도록 
각 page에 대해서 free_page() 를 호출하게 된다.

 free_page() 내에서는 해쉬의 원소들에 대해서 각각 실행된다. free_page 내에서는 두 가지
함수를 실행하게 되는데, 첫 번째로 실행되는 함수는 바로 page_unload() 를 통해서는
만약 dirty bit 인 경우에 file 로 다시 업데이트를 해줘야 한다. 이 경우 제일 중요한 부분이
바로 buffer를 활용하는 부분인데, 만약 buffer를 사용하지 않고, 직접적으로 옮겨주는 경우에
페이지 폴트가 발생하게 된다면 돌이킬 수 없는 문제가 야기된다. 따라서 버퍼를 통해 kpage의 
내용물들을 옮겨오고, 이걸 다시 file 에 써주는 방식으로 구현하게 된다.

 이렇게 swap in 까지 마친 이후 page_desroty() 가 실행되며 pagedir에 저장된 매핑과,
pagerec_clear_page를 통해 현재 process에 저장된 page를 삭제하고, page를 지우게 된다.
이 함수들이 hash 에 원소에 대해 반복되며 모든 원소를 지워나가면서 작동되며 page를 지워주게 된다.

 결국 이 함수의 실행 과정에서 frame table에 저장된 frame 을 지우고, 마찬가지로
pagedir 에서 현재 종료되는 프로세스와 연관이 되어있는 페이지를 지워주게 된다. 이외에도
pagerec_clear_page를 통해서 page를 hash table 에서 지워주는 역할을 수행하게 된다. 따라서
마지막으로 남은 것은 바로 file-memory mapping 과 관련된 부분이다.

#### free file-memory mapping
```C
/* userprog/process.c */
void 
process_exit (int status)
{
  ...
  /* Free file-memory mappings. */
  for (e = list_begin (&this->mappings); e != list_end (&this->mappings); )
    {
      next = list_remove (e);
      mapping = list_entry (e, struct mapping, elem);
      free (mapping);
      e = next;
    }
  ...
}
```
 마지막으로 확인할 부분은 바로 FMM 이다. 각 프로세스에 저장되어 있는 mappings 리스트를
순회하며, 각 리스트에 저장되어 있는 mapping 원소들을 삭제해주게 된다. 이 때 각 mapping에
연결되어 저장되어 있는 file pointer는 이미 opend를 통해서 file_close() 가 실행되었으므로,
다시 반복해서 호출할 필요 없이 할당을 해제하며 프로세스를 종료하게 된다.

### Differences with Design Reports
 디자인 레포트에서는 조금 구체적이지 않게 작성했었다. 실제 해제하는 부분이 오히려
더 복잡한 부분이 많았고, 페이지 폴트 등 예기치 못한 오류가 발생하는 경우가 굉장히 많았다.
특히 page_desroty() 과정 중에서 직접적으로 file_write_at() 을 kpage로 호출하였으나,
이 때 이 과정 중에서 만약 kpage를 차례로 읽어가던 중 page_fault() 가 발생하는 경우가
생긴다면 file_write_at() 자체에서 lock 을 소유한 채로 page_fault() 가 다시 실행되는
참사가 발생하게 된다. 이를 고려하여 문제를 해결하기 위해 buffer를 추가로 선언하여 
중간에 buffer로 옮겨지는 과정에서 모든 이슈들이 먼저 해소되고, 이후에 file_write_at()을
통해서 보다 안정적으로 호출이 마무리되게 된다. 

### Limitations
 구현 상 아쉬운 부분은 page_destory() 와 page_unload() 부분의 모호성이다. 두 가지 함수가
실제 기능은 다른 기능을 담당하고 있으며 각각 모든 페이지를 삭제 / 특정 페이지를 unload 한다는
특성을 갖고는 있지만, page_destory() 내에서 특정 상태인 경우에 대해서만 핸들링한다면
중복해서 구현된 부분을 줄일 수 있을 것이라 생각된다. 이외에도 초기에는 mapping에 대해서도
map() 이 실행되었다는 것은 결국 mumap() 이 실행될 것이라는 것을 내재하고 있다고 생각하여,
따로 process_exit() 시에 지워주지 않았는데, 큰 문제가 발생하지는 않았었다. 이상적으로 
생각해본다면, mapping을 지워주는 것을 확인하는 부분은 필요하지만, mumap() 을 통해서
지워져야 한다고 생각이 들었다.


 