# Project 3 - Final Report (Team 28)

## Task 1 - Frame Table
<!-- TODO: To be filled by Taeho. -->
### Improvement
<!-- TODO: To be filled by Taeho. -->
### Newly Introduced Functions and Data Structures
<!-- TODO: To be filled by Taeho. -->
### Differences with Design Reports
<!-- TODO: To be filled by Taeho. -->
### Things We Have Learned
<!-- TODO: To be filled by Taeho. -->
### Limitations
<!-- TODO: To be filled by Taeho. -->

## Task 2 - Lazy Loading

### Improvement
기존 Pintos의 구현에서는 프로세스를 시작할 때, 실행 파일 전체를 메모리에 적재한
후 해당 내용을 참조하여 프로세스를 시작하도록 되어 있었다. Project 3에서는 
이러한 구현을 수정하여, 프로세스를 시작할 때 디스크에 존재하는 실행 파일의 내용
전체를 메모리에 적재하는 것이 아닌, 실행 파일의 참조만을 보조 페이지 테이블
(supplemental page table)에 적재하여 실행 파일의 해당 부분이 실제 필요한 
경우에만 메모리에 적재하여야 하며, 이러한 프로세스 실행 방식을 게으른 적재(lazy 
loading)라 칭한다.

### Newly Introduced Functions and Data Structures
앞서 설명한 바와 같이, 기존 구현에서는 실행 파일 전체를 메모리에 적재하도록
구현되어 있었다. 이를 더 자세히 살펴보기 위해, `userprog/process.c`의 
`load_segment()`를 살펴보자.

```C
/* From previous version of userprog/process.c. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ...
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      ...
      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }
      ...
    }
  return true;
}
```
`load_segment()`는 `load()`에서 호출되어, 디스크에 존재하는 실행 파일에서 하나의
세그먼트를 적재하는 함수이다. 기존에 구현되었던 `load_segment()` 함수에서는, 
`file_read()` 함수 호출을 통해 실제로 실행 파일을 할당된 페이지에 적재한다.
게으른 적재를 구현하기 위해 수정한 `load_segment()` 함수의 정의는 다음과 같다.

```C
/* From userprog/process.c. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *uaddr,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ...
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Make a page to be inserted to the page record. */
      struct page *upage = 
        page_from_file (uaddr, writable, file, ofs, page_read_bytes);

      if (upage == NULL)
        return false;

      /* Register this page onto current process's page record. */
      pagerec_set_page (this->pagerec, upage);

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      uaddr += PGSIZE;
      ofs += page_read_bytes;
    }
  ...
}
```
새로이 변경된 `load_segment()` 함수의 구현에서는, 실제로 `file_read()` 함수 
호출을 통해 해당 세그먼트를 물리적 메모리에 적재하지 않고, 해당 세그먼트에
속해있는 페이지만을 `pagerec_set_page()` 함수 호출을 통해 새롭게 만들어진 
프로세스의 보조 페이지 테이블, 혹은 페이지 레코드에 저장한다. 하지만, 해당 
페이지 레코드 원소(page record entry; or, supplemental page table entry)는 실제 
메모리에 적재된 것이 아니며, 페이지 디렉터리에 등록되지도 않았기 때문에 실제
사용자 프로세스에서 실행 영역의 해당 부분에 접근하려 하면 페이지 폴트가 
발생한다. 

`page_from_file()`은 현재 파일에 존재하는 페이지를 나타내기 위한 `struct page`의
생성자이다. `struct page`와 그와 관련된 함수들에 관해서는 이후 단락에서 
설명한다.

페이지 폴트 핸들러에서는 현재 프로세스의 페이지 레코드 원소를 찾아 실행 파일의
해당 페이지를 실제 물리적 메모리에 적재한다. 페이지 레코드 자료구조와 페이지
레코드에 저장되는 `struct page`에 대해서는 이후 Supplemental Page Table 단락에서 
자세히 설명할 것이다. 이 단락에서는 먼저 실제로 해당 페이지를 메모리에 적재하는 
페이지 폴트 핸들러의 관련된 부분에 대해 살펴보자.

```C
/* From userprog/exception.c. */
static void
page_fault (struct intr_frame *f) 
{
  ...
  struct page *upage = pagerec_get_page (this->pagerec, uaddr);
  ... 
  /* Do sanity checking, load page, exit the user process if it's faulty. */
  if (not_present)
    {
      if (upage == NULL || upage->state == PAGE_PRESENT)
        process_exit (-1);

      if (upage->state == PAGE_UNLOADED)
        page_load (upage);
      else if (upage->state == PAGE_SWAPPED)
        page_swap_in (upage);
        
      return;
    }
  ...
}
```
페이지 폴트 핸들러에서는 만약 사용자 프로세스 혹은 커널이 페이지 디렉터리에 
존재하지 않는 페이지에 접근하여 페이지 폴트가 발생한 경우, 먼저 현재 프로세스의
페이지 레코드에서 해당하는 페이지를 찾는다. 만약 현재 프로세스의 맥락 내에서
해당하는 페이지가 존재하지 않거나, 이미 존재하는 페이지에 대해 페이지 폴트가
발생한 경우 사용자 프로세스를 종료시킨다.

`pagerec_get_page()`는 인자로 넘어온 페이지 레코드에서 사용자 가상 주소에 
해당하는 페이지를 찾는 함수이다. 해당 함수에 대해서는 이후 단락에서 설명한다.

만약 해당하는 페이지를 찾은 경우, `page_load()` 혹은 `page_swap_in()` 함수를 
호출하여 해당 페이지를 물리적 메모리에 적재한다. 이 중 게으른 적재의 상황에서
주목하여야 할 부분은 현재 페이지의 상태가 `PAGE_UNLOADED`여서 `page_load()` 
함수가 호출되는 부분인데, 게으른 적재를 구현하기 위해 페이지 레코드에 등록된
실행 파일들의 상태는 `page_from_file()`에서 기본적으로 `PAGE_UNLOADED`로 
설정되기 때문이다. 

`page_load()`에서 실제로 인자로 넘겨진 `struct page`를 메모리에 적재하는 부분은
다음과 같다.

```C
/* From vm/page.c. */
/* Loads PAGE into a physical frame. PAGE must be in the unloaded state and
   must not be a null pointer. Returns newly allocated kernel address if it 
   succeed, a null pointer if failed. */
void *
page_load (struct page *page)
{
  ASSERT (page != NULL);
  ASSERT (page->state == PAGE_UNLOADED);

  uint8_t *kpage = frame_allocate (page);

  if (kpage == NULL)
    return NULL;

  lock_acquire (&filesys_lock);
  file_read_at (page->file, kpage, page->size, page->offset);
  lock_release (&filesys_lock);

  page->state = PAGE_PRESENT;
  pagedir_set_page (page->pagedir, page->uaddr, kpage, page->writable);

  return kpage;
}
```
`page_load()`는 해당 페이지가 실제로 파일에 존재할 때, 해당 페이지의 상응하는
파일의 일부분을 실제 물리적 프레임으로 적재하는 함수이다. 이를 위해서 
`page_load()`는 `frame_allocate()`를 호출하여 해당 페이지가 적재될 물리적 
프레임을 할당받는다. 이후, `file_read_at()` 호출을 통해 페이지에 상응하는 파일의
일부분을 할당받은 프레임에 적재한다. 마지막으로, 페이지가 속하는 프로세스의 
페이지 디렉터리에 해당 페이지를 등록하여 이후 해당 페이지에 대한 접근이 
정상적으로 이루어질 수 있게 하고, 페이지의 상태를 `PAGE_PRESENT`로 변경한다.

### Differences with Design Reports
이전의 설계 보고서에서 서술한 `load_segment()` 함수의 의사 코드에서는 Pintos에서
제공하는 `struct file`의 인터페이스 함수를 통해 해당 실행 파일을 읽어들이는 것이
아닌, 실제 파일 시스템 블럭 장치의 섹터 번호를 통해 해당 실행 파일을 
읽어들이도록 서술되어 있었다. 

이러한 구현의 문제점으로는 먼저 Pintos에서 이미 제공하는 `struct file`과 
`struct inode`의 인터페이스 함수들의 구현을 변경하거나 새로운 함수를 추가하지 
않는 이상 파일을 읽어들이기 위한 블럭 장치의 섹터 번호를 알아낼 방법이 없다는 
점이 존재한다. 만약 `struct inode`에 관련된 함수를 수정하여 해당 파일에 관련된 
섹터 번호를 알아낸다고 하더라도, 이러한 섹터 번호를 통해 `block_read()`를 직접
호출하여 해당 파일을 읽는 행위는 Pintos에서 `struct file`을 통해 제공하는 
추상화와 캡슐화를 무시하는 위험한 행위이다. 또한, 파일을 읽어들일 시작 바이트 
번호, 또는 오프셋은 페이지 단위로 정렬(page-aligned)되어 있지 않으므로 버퍼에
각 섹터를 읽어들여 이를 페이지 단위로 정렬하여 실제 물리적 프레임에 쓰는 작업을
`page_load()`에서 구현하여야 하는데, 이러한 작업은 `file_read_at()` 혹은 
`inode_read_at()`에 이미 구현되어 있으므로 굳이 그러할 필요가 없다.

따라서, 새로운 구현에서는 스왑 장치와 파일 시스템 장치 모두에서 섹터 번호를 통해
읽어들인다는 기존의 구현을 버리고, 스왑 장치에 존재하는 페이지에 대해서는 섹터
번호로, 파일 시스템 장치에 존재하는 페이지에 대해서는 파일을 통해 읽어들이도록
구현을 변경하였다. 처음에 위에서 설명한 구현을 채택한 이유는 스왑 장치와 파일
시스템 장치 모두 섹터 번호로 읽어들인다면 `struct page`의 원소의 개수, 또는 
크기가 줄어들고 구현의 일관성을 달성할 수 있다는 이유였지만, 이러한 구현의 
일관성이나 몇 바이트의 용량보다 위에서 설명한 단점이 더 크다고 판단하여 구현
방식을 변경하였다.

### Limitations
현재 구현에서는 두 프로세스가 같은 실행 파일을 참조하더라도 해당 실행 파일에
대한 실제 물리적 프레임에의 사본을 두 개 만들고 있다. 허나, 실행 파일은 
기본적으로 쓰기가 금지되어 있으므로 두 프로세스가 같은 실행 파일을 참조한다면
새로운 사본 혹은 페이지를 만드는 것이 아닌 같은 페이지를 공유하도록 할 수 있을
것이다. 

따라서, 프로세스의 초기 실행 과정에서 이러한 공유 가능 여부를 체크하고
만약 현재 해당 실행 파일을 실행하고 있는 다른 프로세스가 존재한다면 새로운
페이지를 만들지 않고 해당 실행 파일 페이지를 참조하도록 한다면 더 좋은 성능을
달성할 수 있을 것이다.

## Task 3 - Supplemental Page Table

### Improvement
기존의 Pintos에서는 사용자 프로세스에 필요한 데이터를 모두 실제 물리적 메모리에
적재하였기 때문에, 페이지 폴트는 사용자 프로세스 혹은 커널의 버그에 의해서만 
발생하였다. 하지만, 게으른 적재와 파일 메모리 대응, 그리고 페이징을 구현한
이후에는 페이지 폴트가 더 이상 버그에 의해서만 발생하지 않고, 실제 유효한 메모리
접근에 의해서도 발생할 수 있으며, 페이지 폴트 핸들러는 이러한 유효한 메모리
접근에 의한 페이지 폴트를 적절하게 처리하여야 한다.

이때, 페이지 폴트 핸들러가 해당 메모리 접근이 유효한지, 그리고 만약 메모리 
접근이 유효하다면 해당 페이지를 어디어 가져와야 하는지 등의 정보를 알아내기 위해
Pintos 문서에서는 보조 페이지 테이블이라는 자료구조를 제시하고 있다. 즉, 페이지
폴트 핸들러는 페이지 폴트 발생시 보조 페이지 테이블을 참조하여 페이지 폴트를
처리할 방식을 결정하게 된다.

아래의 단락들을 읽기 전에, 한 가지 용어에 대한 정의를 하자면 이후 단락에서 
'페이지 레코드' 혹은 `struct pagerec`은 Pintos에서 제시하는 보조 페이지 테이블을 
가리킨다. 즉, 이후 단락들에서 페이지 레코드라 칭하는 대상들은 모두 어떤 
프로세스에 속한 보조 페이지 테이블을 가리키는 용어라 생각하면 된다. 이러한 
이름은 보조 페이지 테이블이 x86 아키텍처의 페이지 디렉터리, 혹은 더 일반적인 
용어로는 페이지 테이블과 비슷한 역할을 하기 때문에, 영단어 'directory'의 유의어 
중 하나로 'record'를 택하여 지었다.

### Newly Introduced Functions and Data Structures
먼저, 페이지 레코드에 속하는 하나의 페이지를 나타내는 자료구조인 `struct page`의
정의부터 살펴보자.

```C
/* From vm/page.h. */
/* A virtual page. */
struct page 
  {
    enum page_state state;     /* State of this page. */
    void *uaddr;               /* User virtual address of this page. */
    uint32_t *pagedir;         /* Page directory where this page lies in. */
    bool writable;             /* Is this page writable? */
    block_sector_t sector;     /* The sector number of underlying swap block. */
    struct file *file;         /* Underlying file. */
    off_t offset;              /* Offset from underlying file. */
    off_t size;                /* Bytes to be loaded from underlying file. */
    struct hash_elem elem;     /* Hash element. */
  };
```
`struct page`는 하나의 사용자 가상 페이지를 나타내기 위한 구조체이다. 
`struct page`는 먼저 해당 페이지의 상태를 나타내는 `state`를 멤버로 가지고 있다.
`enum page_state`는 `PAGE_PRESENT`, `PAGE_SWAPPED`, `PAGE_UNLOADED`의 세 가지
값을 가질 수 있으며, 각각 현재 메모리에 존재하는 상태, 스왑 블럭 장치에 쫓겨난
(evicted) 상태, 파일 시스템 블럭 장치에 존재하나 아직 물리적 프레임에는 적재되지
않은 상태를 나타낸다.

`uaddr`는 해당 페이지가 시작하는 가상 주소를 나타내는 멤버이다. `pagedir`는 해당
페이지가 속해야 할 페이지 디렉터리를 가리키는 포인터이다. 가상 주소는 가상 주소
혼자서는 의미가 없고 반드시 상응하는 페이지 디렉터리가 존재하여야 의미가 생기며,
실제로 어떤 페이지가 스왑 장치로 쫓겨날 때에는 페이지 디렉터리에서 해당 페이지에
상응하는 페이지 디렉터리 원소를 찾아 이의 `not_present` 비트를 0으로 설정하는
과정이 필요하므로 이렇게 구현하였다.

`writable`은 해당 페이지가 쓰기 가능한지를 나타내는 플래그이다. `sector`는 스왑
장치로 쫓겨난 페이지가 스왑 블럭 장치 내에서 존재하는 섹터 번호를 나타내는
멤버이다. `file`은 아직 파일에서 적재되지 않은 페이지가 존재하는 파일을 가리키는
포인터이며, `offset`는 `file` 내에서의 시작 오프셋, `size`는 `offset`부터 
시작하여 읽어들어야 할 바이트의 수를 나타낸다.

마지막으로, 해시 테이블로 구현되어 있는 페이지 레코드의 원소로 `struct page`를
추가하기 위해 쓰이는 `elem`을 원소로 가지고 있다.

```C
/* From vm/page.c. */
/* A page record. */
struct pagerec 
  {
    struct hash records;
  };
```
이러한 페이지들은 페이지 레코드라 칭하는 자료구조에 저장되며, 페이지 레코드는
내부적으로 해시 테이블로 구현되어 있다. `struct pagerec`은 `vm/page.c`에 
정의되고 `vm/page.h`에는 선언만이 노출되어 있어 `vm/page.c` 외부에서는 
`struct pagerec`을 인자로 받는 인터페이스 함수만을 이용하게 된다.

```C
/* From userprog/process.h. */
/* An user process. */
struct process
  {
    ...
    struct pagerec *pagerec;      /* Page records. */
    uint8_t *esp                  /* Saved user stack pointer. */
    ...
  };
```
새로운 `struct process`는 이제 페이지 폴트 핸들러에서 사용할 페이지 레코드를
멤버로 가지고 있다. 이때, 프로세스는 독립된 주소 공간을 가지고 있으므로 페이지
레코드에 대한 포인터는 `struct process`내에 저장되고, 페이지 레코드는 기본적으로 
해당 페이지 레코드가 속한 프로세스의 맥락 내에서 수정된다.

또한, 페이지 폴트 핸들러에서 스택 확장의 상황을 확인하기 위해 현재 프로세스의
ESP 레지스터를 저장하기 위한 `esp` 멤버를 추가하였다.

```C
/* From vm/page.c. */
/* Creates page records. Returns a null pointer if it failed to allocate 
   memory.*/
struct pagerec *
pagerec_create (void)
{
  struct pagerec *rec = (struct pagerec *) malloc (sizeof (struct pagerec));

  if (rec == NULL)
    return NULL;

  hash_init (&rec->records, page_hash, page_less_func, NULL);
  return rec;
}
```
`pagerec_create()`는 새로운 페이지 레코드를 생성하는 함수로, 프로세스의 초기화
과정, 더 자세히는 `make_process()` 과정 중 호출되어 새로운 프로세스에 대한
페이지 레코드를 만드는 데 쓰인다. 이때, 페이지 레코드 내부 해시 테이블에 대한
해시 함수와 해시 비교 함수는 `page_hash()`와 `page_less_func()`가 쓰이는데, 
둘 모두 `struct page`의 `uaddr`를 참조하여 이에 대한 해시 값을 계산하거나 비교를
수행한다.

```C
/* From vm/page.c. */
/* Inserts PAGE into REC. If such PAGE already exists on REC, destroy it and
   insert new one. */
void
pagerec_set_page (struct pagerec *rec, struct page *page)
{
  ASSERT (rec != NULL);
  ASSERT (page != NULL);

  struct hash_elem *e;

  if ((e = hash_replace (&rec->records, &page->elem)) != NULL)
    {
      struct page *old = hash_entry (e, struct page, elem);
      page_destroy (old);
    }
}
```
`pagerec_set_page()`는 인자 `rec`에 `page`를 등록하는 함수이다. 만약 같은 사용자
가상 주소에 대한 페이지가 이미 `rec`에 존재한다면, 이미 존재하는 원소를 버리고
새로운 페이지를 `rec`에 등록한다.

```C
/* From vm/page.c. */
/* Retrieves page to which UADDR belongs in REC. Returns a null pointer
   if there's no page with UADDR in the given REC. */
struct page *
pagerec_get_page (struct pagerec *rec, const void *uaddr)
{
  ASSERT (rec != NULL);

  struct page p = {.uaddr = pg_round_down (uaddr)};
  struct hash_elem *e = hash_find (&rec->records, &p.elem);

  return e == NULL ? NULL : hash_entry (e, struct page, elem);
}
```
`pagerec_get_page()`는 인자 `rec`에서 사용자 가상 주소 `uaddr`를 가지는 페이지를
찾는 함수이다. 만약 `uaddr`을 원소로 가지는 페이지가 `rec`에 존재하지 않으면
널 포인터를 반환하며, 그러한 페이지가 존재한다면 해당 페이지에 대한 포인터를
반환한다.

```C
/* Clears mapping associated with PAGE in REC. Does nothing when there's no 
   such mapping. */
void
pagerec_clear_page (struct pagerec *rec, struct page *page)
{
  ASSERT (rec != NULL);

  struct hash_elem *e = hash_find (&rec->records, &page->elem);
  
  if (e != NULL)
    hash_delete (&rec->records, e);
}
```
`pagerec_clear_page()`는 `rec`에서 `page`를 지우는 함수이다. 

```C
/* From vm/page.c. */
/* Makes a new page that alreay lies in the memory, with address of UADDR and
   WRITABLE flag. Since this assumes the new page is already in the physical
   memory, the caller must allocate a frame for this page right after calling 
   it.
   
   The page directory of new page will be automatically set to the page 
   directory of current process.    

   Returns a null pointer if it failed to allocate memory. UADDR must be page
   aligned. */
struct page *
page_from_memory (void *uaddr, bool writable)
{
  struct page *page = (struct page *) malloc (sizeof (struct page));
  struct process *this = thread_current ()->process;

  ASSERT (this != NULL);
  ASSERT (pg_ofs (uaddr) == 0);

  if (page == NULL)
    return NULL;

  page->state = PAGE_PRESENT;
  page->uaddr = uaddr;
  page->pagedir = this->pagedir;
  page->writable = writable;
  page->sector = 0;
  page->file = NULL;
  page->offset = 0;
  page->size = 0;

  return page;
}
```
```C
/* From vm/page.c. */
/* Makes a new page from a file, which will demand paged from FILE, with 
   WRITABLE flag and SIZE, OFFSET within the file.

   The page directory of the page will be automatically set to current
   process's page directory.    

   Returns a null pointer if it failed to allocate memory. UADDR must be page
   aligned. FILE must not be a null pointer and OFFSET + SIZE must be less than 
   the length of FILE. SIZE must be less than or equal to PGSIZE. */ 
struct page *
page_from_file (void *uaddr, bool writable, struct file *file, 
                off_t offset, size_t size)
{
  struct page *page = (struct page *) malloc (sizeof (struct page));
  struct process *this = thread_current ()->process;

  ASSERT (this != NULL);
  ASSERT (pg_ofs (uaddr) == 0);
  ASSERT (file != NULL);
  ASSERT (file_length (file) >= offset + (off_t) size);
  ASSERT (size <= PGSIZE);

  if (page == NULL)
    return NULL;

  page->state = PAGE_UNLOADED;
  page->uaddr = uaddr;
  page->pagedir = this->pagedir;
  page->writable = writable;
  page->sector = 0;
  page->file = file;
  page->offset = offset;
  page->size = size;

  return page;
}
```
`page_from_memory()`와 `page_from_file()`은 새로운 페이지를 만드는 두 가지 
상황에 대응하기 위한 `struct page`의 생성자이다.

`page_from_memory()`는 사용자 가상 주소 `uaddr`과 해당 페이지가 쓰기 가능한지를
나타내는 플래그 `writable`을 받아 새로운 페이지를 생성하는 `struct page`의
생성자이다. 이때, 해당 페이지는 실제 물리적 프레임에 이미 적재되어 있을 것을 
가정하므로 페이지의 상태는 `PAGE_PRESENT`로 설정되며, `pagedir`는 현재 
프로세스의 페이지 디렉터리로 설정된다. `sector`, `file`, `offset`, `size` 등 
나머지 멤버 변수들은 해당하는 스왑 장치 내의 공간이 존재하지 않으므로 0 혹은 
그에 상응하는 초기값으로 초기화된다.

`page_from_file()`은 파일 시스템 장치 내에 존재하는 파일에 해당하는 페이지를
만들기 위한 `struct page`의 생성자이다. 사용자 가상 주소 `uaddr`과 `writable`
플래그, 파일 포인터 `file`, 파일 내의 오프셋 `offset`과 파일에서 읽어들일 크기인
`size`를 받아 새로운 `struct page`를 생성한다. 새로운 페이지의 상태는 아직
메모리에 적재되지 않은 것을 가정하므로 `PAGE_UNLOADED`로 설정된다.

```C
/* From vm/page.c. */
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
`struct page`의 소멸자이다. `page`를 받아 상응하는 물리적 프레임을 할당 
해제하고, 해당 페이지의 페이지 디렉터리와 현재 프로세스의 페이지 레코드에서
해당 페이지를 삭제한다. 마지막으로 `page` 자신을 할당 해제한다.

```C
/* From vm/page.c. */
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
스왑 장치로 쫓겨난 `page`를 실제 물리적 프레임에 다시 읽어들이는 함수이다. 
먼저 새로운 물리적 프레임을 `frame_allocate()` 함수를 호출하여 할당받고, 
할당받은 물리적 프레임에 스왑 블럭 장치의 `page->sector`번째 섹터부터 읽어들여
쓴다. 함수 호출 이후에는 스왑 슬롯을 할당 해제하고, 페이지의 상태를 
`PAGE_PRESENT`로 변경하며, 해당 페이지가 속해야 하는 페이지 디렉터리에 페이지의
사용자 가상 주소로부터 물리적 프레임으로의 대응을 등록한다.

```C
/* From vm/page.c. */
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
`page`를 스왑 장치로 다시 쫓아내는 함수이다. `swap_allocate()` 호출을 통해 
새로운 스왑 슬롯을 할당받고, 할당받은 스왑 슬롯에 해당 페이지의 내용을 쓴다.
이후 페이지의 상태를 현재 스왑 블럭 장치에 존재함을 나타내는 `PAGE_SWAPPED`로
변경하고, 이후 페이지를 다시 읽어들일 섹터 번호를 새로이 할당받은 스왑 슬롯의
섹터 번호로 설정하며, 페이지 디렉터리에서 해당 페이지에 대한 대응을 지운다.
마지막으로, 해당 페이지가 존재하였던 물리적 프레임을 할당 해제한다.

```C
/* From vm/page.c. */
/* Loads PAGE into a physical frame. PAGE must be in the unloaded state and
   must not be a null pointer. Returns newly allocated kernel address if it 
   succeed, a null pointer if failed. */
void *
page_load (struct page *page)
{
  ASSERT (page != NULL);
  ASSERT (page->state == PAGE_UNLOADED);

  uint8_t *kpage = frame_allocate (page);

  if (kpage == NULL)
    return NULL;

  lock_acquire (&filesys_lock);
  file_read_at (page->file, kpage, page->size, page->offset);
  lock_release (&filesys_lock);

  page->state = PAGE_PRESENT;
  pagedir_set_page (page->pagedir, page->uaddr, kpage, page->writable);

  return kpage;
}
```
현재 파일 시스템 블럭 장치에 존재하는 `page`를 물리적 프레임으로 읽어들이는 
함수이다. `page_swap_in()`과 거의 비슷한 동작을 하나, 읽어들이는 위치가 스왑
블럭 장치가 아닌 파일이라는 차이점이 있다.

```C
/* From vm/page.c. */
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
```
`page`를 물리적 프레임에서 할당 해제하고 다시 파일로 내쫓는 함수이다. 만약, 
`page`가 쓰기 가능한 페이지이고 수정되었다면 해당 페이지의 내용을 다시 파일에
되쓴다. 내부적으로 커널 영역에 새로운 버퍼를 생성하여 해당 버퍼에 페이지를
복사한 후 이를 파일에 쓰는데, 이는 파일 쓰기 작업 중 발생하는 페이지 폴트를
방지하기 위함이다.

```C
/* From vm/page.c. */
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
```
페이지 레코드의 한 원소를 할당 해제하는 함수이다. 만약 해당 페이지가 현재 
메모리 상에 존재하고, 해당 페이지에 대응되는 파일이 존재하는 경우에는 
`page_unload()`를 호출하여 해당 페이지를 파일에 되쓰기 위해 시도한다. 이후에는
`page_destroy()`를 호출하여 해당 페이지와 관련된 프레임을 메모리에서 할당 
해제한다.

```C
static void
page_fault (struct intr_frame *f) 
{
  ...
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
  
  /* Do sanity checking, load page, exit the user process if it's faulty. */
  if (not_present)
    {
      if (upage == NULL || upage->state == PAGE_PRESENT)
        process_exit (-1);

      if (upage->state == PAGE_UNLOADED)
        page_load (upage);
      else if (upage->state == PAGE_SWAPPED)
        page_swap_in (upage);
        
      return;
    }

  if (is_user_vaddr (fault_addr))
    process_exit (-1);

  
  printf ("Page fault at %p: %s error %s page in %s context.\n",
          fault_addr,
          not_present ? "not present" : "rights violation",
          write ? "writing" : "reading",
          user ? "user" : "kernel");
  kill (f);
}
```
이러한 페이지 레코드의 구현에 맞추어 새롭게 구현한 페이지 폴트 핸들러는 다음과
같다. 새로운 페이지 폴트 핸들러에서는 먼저 프로세스의 페이지 레코드에서 페이지
폴트가 발생한 주소가 속한 페이지를 찾는다. 이후, 현재 프로세스에 사용자 스택 
포인터를 저장하며, 스택 확장 상황인지를 판단해 만약 필요한 경우 스택을 
확장해준다. 스택 확장에 대한 설명은 이후 단락에서 더 자세히 한다.

이후로는, 만약 페이지 레코드에 해당 페이지가 존재한다면 페이지 레코드의 내용과
앞에서 설명한 `struct page`에 대한 함수를 이용해 페이지를 파일에서 읽어들이거나
스왑 블럭 장치에서 읽어들인다. 만약 페이지 레코드에 해당 페이지가 존재하지 
않거나, 만약 페이지 폴트가 커널 영역에서 발생한 사용자 가상 주소에 대한 페이지에
대한 페이지 폴트라면 현재 실행되고 있는 프로세스를 종료한다.

### Differences with Design Reports
앞서 설명한 바와 같이, 기존의 보조 페이지 테이블 원소는 내부 블럭 장치의 섹터
번호만을 원소로 가지고, 이를 통해 블럭을 읽거나 쓰는 작업을 하도록 구현하기로
계획되었다. 하지만, 앞서 설명한 문제점으로 인해 새로운 `struct page`는
섹터 번호는 스왑 블럭 장치에 쓰거나 읽을때만 사용하고, 파일 시스템 블럭 장치에
대해서는 Pintos에서 제공하는 파일 인터페이스 함수를 이용해 읽거나 쓰는 방식으로
구현하였다.

또한, 기존의 보조 페이지 테이블 원소는 해당 페이지가 만약 파일 메모리 대응에
속한 페이지일 경우 파일 메모리 대응을 삭제할 때 참조하기 위한 용도로 `mapid`
멤버를 가지고 있었지만, 새로운 `struct page`와 파일 메모리 대응의 구현은 파일 
메모리 대응을 다른 구조체와 리스트를 이용해 관리하므로 `mapid` 멤버를 가지고 
있지 않다. 파일 메모리 대응의 자세한 구현에 대해서는 관련된 단락에서 다룬다.

마지막으로, 새로운 `struct page`는 페이지의 상태를 `PAGE_PRESENT`, 
`PAGE_SWAPPED`, `PAGE_UNLOADED`의 세 가지로 한정하고 이를 저장하기 위한 멤버를
가지고 있다.

### Limitations
위에서 언급한 프로세스 간 실행 파일 페이지 공유에 대해, 현재의 구현은 페이지 
레코드가 프로세스에 국한된 자료구조여서 실행 파일을 공유하기 힘들며, 프로세스
종료 시 실행 파일을 닫아버리기 때문에 코드 세그먼트에 대한 페이지 공유를 
구현하기 힘들다는 단점이 있다.

이를 위해서, 실행 파일에 대한 포인터를 기존의 `opened` 리스트와는 다른 멤버로
저장하고, 새로운 프로세스 시작 시 실행하고자 하는 파일이 현재 열려있는지를 
확인한 후, 해당 파일에 대한 페이지를 찾아 새로운 프로세스의 페이지 레코드에 
추가하도록 할 수 있을 것이다.

## Task 4 - Stack Growth
<!-- TODO: To be filled by Taeho. -->
### Improvement
<!-- TODO: To be filled by Taeho. -->
### Newly Introduced Functions and Data Structures
<!-- TODO: To be filled by Taeho. -->
### Differences with Design Reports
<!-- TODO: To be filled by Taeho. -->
### Things We Have Learned
<!-- TODO: To be filled by Taeho. -->
### Limitations
<!-- TODO: To be filled by Taeho. -->

## Task 5 - File Memory Mapping

### Improvement
현대 운영체제에서는 파일을 조작하기 위한 시스템 호출을 이용하여 파일을 
명시적으로 메모리에 적재하는 방식 이외에도, 파일을 가상 메모리 주소에 대응시켜
한 번에 파일을 읽어들이지 않고 페이지 단위로 게으르게(lazily) 읽어들이는 방법을
대응한다. 이는 큰 파일을 읽을 때 한 번에 파일을 읽어들이지 않으므로 특히 
효율적이며, 프로세스 간에 자원을 공유할 수 있는 방법을 제공한다. 이러한 파일과
메모리 영역 상의 대응을 파일 메모리 대응이라 칭하며, Unix-like 운영체제에서는
일반적으로 `mmap()`과 `munmap()` 시스템 호출을 이용해 이러한 파일 메모리 대응을
생성하고 삭제하는 방법을 제공한다.

이전까지의 Pintos 구현은 `mmap()`과 `munmap()`이 구현되지 않아 사용자가 파일을
읽어들이고 수정하기 위해서는 명시적으로 `read()`, `write()` 시스템 호출을 
이용해야 했으나, 새로운 구현에서는 `mmap()`, `munmap()` 시스템 호출에 대한 
구현을 추가해 위에서 설명한 파일 메모리 대응을 만들고 제거할 방법을 제공한다.

### Newly Introduced Functions and Data Structures
```C
/* From userprog/syscall.h. */
/* A file-memory mapping. */
struct mapping
  {
    uint8_t *uaddr;          /* Starting user address. */
    int pages;               /* The number of pages belong to this mapping. */
    mapid_t mapid;           /* Mapping identifier. */
    struct file *file;       /* File for this mapping. */
    struct list_elem elem;   /* List element. */
  };
```
`struct mapping`은 하나의 파일 메모리 대응을 나타내기 위한 구조체이다. 해당
구조체는 파일 메모리 대응에 속하는 페이지를 나타내는 것이 아닌, 하나의 파일
메모리 대응 전체를 나타낸다는 것에 주목하자.

이러한 대응을 나타내기 위해, `struct mapping`은 먼저 대응의 시작 주소를 나타내는
`uaddr`, 대응에 속한 페이지의 개수를 나타내는 `pages`, 대응의 식별자(map 
identifier)를 나타내는 `mapid`, 대응되는 파일을 나타내는 `file`, 현재 프로세스의
파일 메모리 대응 리스트에 추가되기 위한 `elem`을 멤버로 가지고 있다. 

```C
/* From userprog/syscall.c. */
/* System call handler for mmap(). */
static uint32_t
mmap (void *esp)
{
  int fd = (int) dereference (esp, 1, WORD_SIZE);
  uint8_t *addr = (uint8_t *) dereference (esp, 2, WORD_SIZE);

  struct file *file = retrieve_fp (fd);
  struct process *this = thread_current ()->process;

  ASSERT (this != NULL);

  if (!is_mapping_valid (addr, file))
    return MAP_FAILED;

  file = file_reopen (file);

  struct mapping *mapping = (struct mapping *) malloc (sizeof (struct mapping));

  mapping->uaddr = addr;
  mapping->file = file;
  mapping->mapid = allocate_mapid ();
  mapping->pages = file_length (file) / PGSIZE + 1;

  for (off_t offset = 0; offset < file_length (file); offset += PGSIZE)
    {
      size_t size = file_length (file) - offset >= PGSIZE 
                    ? PGSIZE 
                    : file_length (file) - offset;

      struct page *page = 
        page_from_file (addr + offset, true, file, offset, size);
      pagerec_set_page (this->pagerec, page);
    }
  
  list_push_back (&this->mappings, &mapping->elem);
  return mapping->mapid;
}
```
`userprog/syscall.c`의 `mmap()`은 시스템 호출 `mmap()`을 실제로 처리하는 시스템
호출 핸들러이다. 해당 함수에서는 먼저 인자로 넘어온 `fd`가 실제로 유효한 파일
설명자인지, `addr`가 페이지 단위로 정렬되어있는지, 파일의 길이가 0인지, `addr`이
널 포인터가 아닌지, 만약 해당 파일 메모리 대응이 다른 대응이나 이미 존재하는
스택 영역 등과 겹치지 않는지를 확인하는 함수인 `is_mapping_valid()`를 호출하여
해당 `mmap()` 호출이 유효한지를 검증한다. 또한, `allocate_mapid()` 함수를 
호출하여 새로운 대응에 대한 고유한 대응 식별자를 생성한다.

이후에는, `file_reopen()`을 호출하여 해당 파일에 대한 새로운 참조를 생성하며,
`struct mapping` 객체를 생성하여 새로운 대응을 만들고, 이러한 대응에 속하는
페이지를 새로 만들어 현재 프로세스의 페이지 레코드에 등록한다. 마지막으로, 
새로운 대응을 현재 프로세스의 대응 목록의 끝에 추가하여 이후 대응을 삭제할 때
참조할 수 있도록 한다.

```C
/* From userprog/syscall.c. */
/* System call handler for munmap(). */
static void
munmap (void *esp)
{
  mapid_t mapid = (mapid_t) dereference (esp, 1, WORD_SIZE);

  struct mapping *mapping = NULL;
  struct process *this = thread_current ()->process;

  for (struct list_elem *e = list_begin (&this->mappings); 
       e != list_end (&this->mappings);
       e = list_next (e))
    {
      mapping = list_entry (e, struct mapping, elem);

      if (mapping->mapid == mapid)
        break;
    }

  if (mapping == NULL || mapping->mapid != mapid)
    return;

  for (int offset = 0; offset < mapping->pages * PGSIZE; offset += PGSIZE)
    {
      struct page *page = 
        pagerec_get_page (this->pagerec, mapping->uaddr + offset);
      
      if (page->state == PAGE_SWAPPED)
        page_swap_in (page);

      if (page->state == PAGE_PRESENT)
        page_unload (page);

      pagerec_clear_page (this->pagerec, page);
    }
  
  file_close (mapping->file);
  list_remove (&mapping->elem);
  free (mapping);
}
```
`userprog/syscall.c`의 `munmap()`은 시스템 호출 `munmap()`에 대한 시스템 호출
핸들러이다. `munmap()`은 먼저, 현재 프로세스의 대응 리스트를 순회하면서 
`mapid`에 상응하는 현재 프로세스의 대응 리스트의 원소를 찾는다. 만약 그러한
대응이 존재하지 않는다면, 아무것도 하지 않고 함수를 종료한다.

만약 그러한 대응을 찾았다면, 대응에 속한 모든 페이지에 대해 해당하는 페이지의
상태를 검사하고, 만약 상태가 현재 스왑 블럭 장치로 쫓겨난 상태라면 
`page_swap_in()`을 호출해 먼저 해당 페이지를 실제 물리적 프레임 상에 적재한다.
이후에는, 만약 페이지가 원래 `PAGE_PRESENT` 상태였거나 방금 전의 
`page_swap_in()` 호출에서 `PAGE_PRESENT`로 바뀌었다면 해당 페이지를 다시 
메모리로 되쓰려 시도한다. 또한, 해당 페이지를 현재 프로세스의 페이지 레코드에서
제거하여 이후에 대응되었던 메모리 영역에 대한 접근이 불가능하게 한다. 이때, 
페이지 디렉터리에서 해당 페이지에 대한 대응을 제거하는 작업은 
`page_unload()`에서 담당한다.

마지막으로, 대응을 위해 열었던 파일을 닫고, 대응 목록에서 대응을 삭제하며, 
대응 자체를 나타내는 `struct mapping`을 할당 해제한다.

```C
/* From userprog/syscall.c. */
static void
syscall_handler (struct intr_frame *f) 
{
  int syscall_number = (int) dereference (f->esp, 0, WORD_SIZE);

  switch (syscall_number) 
    {
      ...
    case SYS_MMAP: f->eax = mmap (f->esp); break;
    case SYS_MUNMAP: munmap (f->esp); break;
    }
}
```
이러한 새롭게 구현된 시스템 호출 핸들러는 `syscall_handler()`에서 정상적으로
해당 함수를 호출할 수 있도록 추가되었다.

### Differences with Design Reports
기존에는 대응 전체를 저장하는, 파일 메모리 대응 테이블에 해당하는 자료구조를 
만들지 않고 `munmap()` 호출시 현재 프로세스의 페이지 레코드 전체를 순회하여 
대응에 속하는 페이지를 찾는 방식으로 구현하려 하였다. 하지만, 이러한 구현은 
`munmap()` 시스템 호출마다 프로세스의 모든 페이지를 순회하며 `mapid`가 같은
페이지가 존재하는지를 확인해야 하며, 페이지의 개수는 무조건 대응의 개수보다 많아
비효율적이라는 단점이 있었다.

따라서, 새로운 구현에서는 대응을 저장하는 대응 리스트를 각 프로세스마다 가지고 
있고, `mmap()` 시스템 호출에서는 해당 리스트에 새로운 원소를 추가하고 `munmap()`
시스템 호출에서는 해당 리스트에서 `mapid`에 상응하는 원소를 제거하는 식으로
구현하였다.

### Limitations
현재 파일 메모리 대응을 저장하는 자료구조는 리스트 형태로, 구현은 간단하지만
`mapid`에 대응되는 실제 `struct mapping`을 찾기까지 대응의 개수에 비례하는 
시간이 걸린다는 단점이 있다. 또한, 현재의 `allocate_mapid()` 구현은 다음에
할당할 대응 식별자를 하나씩 올리는 식으로 구현되어 있어 만약 대응의 개수가 
많아진다면 대응 식별자를 할당하기 힘들어진다는 단점이 있다. 따라서, 대응 
테이블을 해시 테이블이나 배열 등 다른 자료구조로 변경하여 무작위 접근을
효율적으로 하고, `munmap()`이 호출된 대응의 대응 식별자는 할당 해제하여 해당
대응 식별자를 다음 대응에서 사용할 수 있도록 하는 것이 효율적일 것이다.

## Task 6 - Swap Table
<!-- TODO: To be filled by Taeho. -->
### Improvement
<!-- TODO: To be filled by Taeho. -->
### Newly Introduced Functions and Data Structures
<!-- TODO: To be filled by Taeho. -->
### Differences with Design Reports
<!-- TODO: To be filled by Taeho. -->
### Things We Have Learned
<!-- TODO: To be filled by Taeho. -->
### Limitations
<!-- TODO: To be filled by Taeho. -->

## Task 7 - On Process Termination
<!-- TODO: To be filled by Taeho. -->
### Improvement
<!-- TODO: To be filled by Taeho. -->
### Newly Introduced Functions and Data Structures
<!-- TODO: To be filled by Taeho. -->
### Differences with Design Reports
<!-- TODO: To be filled by Taeho. -->
### Things We Have Learned
<!-- TODO: To be filled by Taeho. -->
### Limitations
<!-- TODO: To be filled by Taeho. -->

## Conclusion