# Project 3 - Design Report (Team 28)
이번 Project 3에서는 다음의 7가지 요구사항을 만족하여야 한다.

## Frame Table
 Frame Table에 대해서 알아보기 전에, pintos에서 어떻게 VM을 구현하고 있는지, 
혹은 virtual address가 어떻게 정의되어 있는지를 확인해야 할 것이다.

#### Pages
 page (혹은 virtual page)는 virtual memory 상에 놓여져 있는 연속적인 공간으로 
pintos 에서는 4 KB 의 용량을 갖고 있다. 또한, 각 page에 접근하기 위해서는 virtual
address를 사용하게 된다. 이 virtual address는 항상 page size에 나눠질 수 있게 
4 KB 단위로 정렬되어야 하며, 현재 pintos 에서는 하단과 같이 virtual 
address를 이루는 32 bit가 구성되어 있다. 앞의 20 bit는 page 를 나타내는 page 
number로 사용되며, 뒤의 12 bit는 offset으로 사용되고 있다. 앞선 20 bit 를 통해 
원하는 page에 접근하고, 위의 12 bit를 통해서 page 내의 특정 위치를 알 수 있다.
실제로 데이터에 접근하기 위해서는 이 virtual address를 physical address로 변환하여 
실제 데이터에 접근해야 한다.

```
31                12 11         0
+-------------------+-----------+
|    Page Number    |   Offset  |
+-------------------+-----------+
```

 각 프로세스가 가지는 virtual memory space는 크게 두 가지 종류로 구분할 수 있다.
각각 user를 위한 user page와 kernel page 로 나뉘고, 이 때 둘을 나누는 기준은 
PHYS_BASE를 기준으로 더 낮은 주소에 위치한 address면 보통 user page를 나타내는 
경우이며, 더 높은 주소를 가지는 경우 kernel page 속의 데이터를 나타낸다고 볼 수 있다. 
이제 알아볼 것은 이러한 page가 실제 메모리 상에서 어떻게 표현되고 있으며, 어떻게 저장되고 
있는지를 이해해야 한다.

#### Frame
 실제 physical memory 에 저장된 데이터는 page와 유사한 frame 단위로 저장된다.
frame 도 마찬가지로 virtual page와 같은 size인 4 KB의 크기를 갖다. 또한,
마찬가지로 page-aligned 되어 있다. virtual page와 마찬가지로 접근하기 위해서 사용할
주소는 physical address라고 불리며 virtual address와 유사한 구조를 갖고 있다.
하단에 있는 것처럼 앞선 20 bit로 frame의 번호를 지칭하며, 나머지 bit로 해당 프레임 
내부의 주소를 지정하게 된다.

```
31               12 11        0
+-------------------+-----------+
|    Frame Number   |   Offset  |
+-------------------+-----------+
```

 현재 pintos에서는 physical address로는 직접 physical memeory에서 데이터를 
읽어오는 방법 자체가 구현되어 있지 않다. pintos 에서는 virtual memory space를 
기준으로 프로세스가 직접적으로 physical memory에 접근하는 것이 아닌, 간접적으로 
접근할 수 있도록 작동하게 된다. 그렇다면 현재는 이러한 과정이 어떤 방식으로 구현되어 
있을까? 실제 코드를 분석하며 알아보도록 하자.

### virtual address
```C
/* threads/vaddr.h */
/* Base address of the 1:1 physical-to-virtual mapping.  Physical
   memory is mapped starting at this virtual address.  Thus,
   physical address 0 is accessible at PHYS_BASE, physical
   address address 0x1234 at (uint8_t *) PHYS_BASE + 0x1234, and
   so on.

   This address also marks the end of user programs' address
   space.  Up to this point in memory, user programs are allowed
   to map whatever they like.  At this point and above, the
   virtual address space belongs to the kernel. */
/* Page offset (bits 0:12). */
#define PGSHIFT 0                      /* Index of first offset bit. */
#define PGBITS  12                       /* Number of offset bits. */
#define PGSIZE  (1 << PGBITS)            /* Bytes in a page. */
#define PGMASK  BITMASK(PGSHIFT, PGBITS) /* Page offset bits (0:12). */
/* Returns true if VADDR is a user virtual address. */
static inline bool is_user_vaddr (const void *vaddr) 
/* Returns true if VADDR is a kernel virtual address. */
static inline bool is_kernel_vaddr (const void *vaddr) 
/* Returns kernel virtual address at which physical address PADDR
   is mapped. */
static inline void * ptov (uintptr_t paddr)
/* Returns physical address at which kernel virtual address VADDR
   is mapped. */
static inline uintptr_t vtop (const void *vaddr)
```
 해당 함수들은 virtual address가 어떻게 사용되고 있는지를 알 수 있다. 위에서 설명한 
것처럼 virtual address가 PHYS_BASE를 기준으로 더 높은 주소라면 kernel vritual 
memory에 해당하며, 이 주소들은 모두 1대 1로 physical memory와 대응되도록 구현되어
있다. 따라서 `ptov()`, `vtop()` 에서 알 수 있는 것처럼 physical address를 가상으로 
변환하려면 PHYS_BASE만 더해주면 되고, 반대로 `vtop()` 인 경우에는 빼주기만 하면 된다. 
이제 Kernel virtual memory 가 어떻게 실제 메모리와 대응되는지는 알았다. 그렇다면
user가 사용할 수 있는 공간인 0 ~ PHYS_BASE (3 GB)의 영역은 어떻게 대응되는 것이며, 
어떻게 구현되어 있을지에 대해서 알아보고자 한다.

 우선 1 대 1로 대응되어 있지 않으므로, 현재 사용자의 가상 메모리 영역이 어떻게 실제 
물리 메모리에 대응되어 있는지를 알기 위해서는 그러한 정보를 누군가는 저장하고 있어야 한다.
그 역할을 수행하는 것이 바로 페이지 테이블이다. 하지만 만약 모든 가상 페이지에 해당하는 
실제 메모리 주소를 저장하기 위해서는 2^20개의 엔트리를 가진 페이지 테이블이 필요하게
되고, 이러한 페이지 테이블은 각 프로세스마다 필요하므로 엄청나게 큰 용량을 차지하게 된다.
따라서 x86에서는 두 개의 계층을 이룬 구조를 선택하였고, 한 개의 페이지 테이블이 저장하는
원소의 숫자를 2^10개로 제한하였다. 대신 상위 구조인 페이지 디렉토리를 따로 선언하여 
사용하게 되는데, 각 페이지 디렉토리가 2^10개의 페이지 테이블을 저장하게 된다.
따라서 하나의 페이지 디렉토리가 하나의 프로세스에서 현재 가상 주소로 총 할당될 수 있는 
4 GB 의 영역을 담당하게 된다. 따라서 페이지 디렉토리에 접근하기 위해서는 가상 주소가
실제로는 하단과 같이 할당되어 있다. 따라서 앞선 10 bit를 통해 페이지 디렉토리 안에서
어떠한 페이지 테이블을 참조할 것인지 알 수 있으며, 그 다음 10 bit를 통해서 해당 페이지
테이블 내의 인덱스를 참조하여 실제 물리 프레임의 주소를 변환할 수 있게 된다. 이제
실제로 핀토스에서 이러한 페이지 테이블, 디렉토리, 그리고 각 엔트리들이 어떻게 구현되어
있는지 간단히 알아보자.
```
 31                  22 21                  12 11                   0
+----------------------+----------------------+----------------------+
| Page Directory Index |   Page Table Index   |    Page Offset       |
+----------------------+----------------------+----------------------+ 

31                                     12 11 9      6 5     2 1 0
+---------------------------------------+----+----+-+-+---+-+-+-+
|           Physical Address            | AVL|    |D|A|   |U|W|P|
+---------------------------------------+----+----+-+-+---+-+-+-+
```
 
#### page allocation
```C
/* Page allocator.  Hands out memory in page-size (or
   page-multiple) chunks.  See malloc.h for an allocator that
   hands out smaller chunks.

   System memory is divided into two "pools" called the kernel
   and user pools.  The user pool is for user (virtual) memory
   pages, the kernel pool for everything else.  The idea here is
   that the kernel needs to have memory for its own operations
   even if user processes are swapping like mad.

   By default, half of system RAM is given to the kernel pool and
   half to the user pool.  That should be huge overkill for the
   kernel pool, but that's just fine for demonstration purposes. */

/* A memory pool. */
struct pool
  {
    struct lock lock;                   /* Mutual exclusion. */
    struct bitmap *used_map;            /* Bitmap of free pages. */
    uint8_t *base;                      /* Base of pool. */
  };

/* Two pools: one for kernel data, one for user pages. */
static struct pool kernel_pool, user_pool;

/* Initializes the page allocator.  At most USER_PAGE_LIMIT
   pages are put into the user pool. */
void palloc_init (size_t user_page_limit)

/* Obtains and returns a group of PAGE_CNT contiguous free pages.
   If PAL_USER is set, the pages are obtained from the user pool,
   otherwise from the kernel pool.  If PAL_ZERO is set in FLAGS,
   then the pages are filled with zeros.  If too few pages are
   available, returns a null pointer, unless PAL_ASSERT is set in
   FLAGS, in which case the kernel panics. */
void * palloc_get_multiple (enum palloc_flags flags, size_t page_cnt)
void * palloc_get_page (enum palloc_flags flags) 

/* Frees the PAGE_CNT pages starting at PAGES. */
void palloc_free_multiple (void *pages, size_t page_cnt) 
void palloc_free_page (void *page) 

/* Initializes pool P as starting at START and ending at END,
   naming it NAME for debugging purposes. */
static void init_pool (struct pool *p, void *base, size_t page_cnt, const char *name) 
/* Returns true if PAGE was allocated from POOL,
   false otherwise. */
static bool page_from_pool (const struct pool *pool, void *page) 
```
 핀토스 상에서 각 페이지를 실제 메모리인 램에 어떻게 데이터를 저장하고, 관리하는지에 대해
저장하고 있는 page allocator 부분을 확인해보도록 하자. 시스템 메모리는 크게 두 개의
풀로 구별되는데, 크게 유저 풀과 커널 풀로 구별된다. 각 풀은 램을 정확히 반으로 나눠서 
4 KB인 페이지의 용량에 맞게 나눠가지게 된다. 각 풀은 연산이 문제 없이 실행되기 위한
락과 사용 여부를 나타내기 위한 비트맵, 그리고 시작 주소로 이뤄져 있다. 이렇게 두 개로 
구별된 이유는 사용자 프로세스가 계속해서 swap이 발생하더라도, 커널 메모리에서는 영향을 받지
않고 계속해서 안정적으로 수행할 수 있도록 하기 위해 두 개의 풀로 나눠서 구성되어 있다. 
이렇게 나뉘어진 풀을 기반으로 `palloc_init()` 에서는 입력받은 제한만큼 유저 풀의 개수를
제한을 두지만 기본적으로는 절반으로 나뉘어져서 유저 풀과 커널 풀을 초기화하게 된다.
그 다음으로 사용되는 것은 바로 `palloc_get_multiple()` 이다. 해당 함수에서는 입력받은
flag를 기준으로 PAL_USER가 설정된 경우 유저 풀(아닌 경우 커널 풀)에서 page_cnt 
만큼의 페이지를 풀에서 할당하게 된다. PAL_ZERO 플래그가 설정된 경우 페이지를 0으로 초기화
하며, 마지막으로 PAL_ASSERT가 설정된 경우 연속된 페이지를 할당할 메모리가 부족하면
커널 패닉을 일으키게 되지만, 설정되지 않은 경우에는 null 포인터를 반환하게 된다. 다음으로
`palloc_free_multiple()` 에서는 입력받은 pages 부터 page_cnt 개수만큼의 페이지의
할당을 해제하는데, 각 페이지별로 어떤 풀에서 할당된 것인지 확인하고 각 풀 내부의 비트맵에
사용 여부를 false로 변환하고, 페이지의 사용 여부를 확인한 뒤에 사용을 해제하게 된다.
마지막 `page_from_pool()` 의 경우에는 입력받은 풀에서 입력받은 페이지가 풀 내에 
존재하는지 여부를 반환한다. 여기서 중요하개 확인할 수 있는 부분은, 유저 메모리가 단순히 
별개로 할당되는 것이 아닌 커널 메모리가 매핑되어 있는 물리 메모리를 1 대 1 로 나눠서 
풀을 할당하는 방식으로 이뤄져 있다는 것이다. 즉, PHYS_BASE 이상의 영역은 유저와 커널이
함께 공유하여 사용하게 될 영역이 될 것이다.

#### Page Table Entry
```C
/* From threads/pte.h */
/* Page table index (bits 12:21). */
#define	PTSHIFT PGBITS		         /* First page table bit. */
#define PTBITS  10                    /* Number of page table bits. */
#define PTSPAN  (1 << PTBITS << PGBITS)//Bytes covered by a page table 
#define PTMASK  BITMASK(PTSHIFT, PTBITS) /* Page table bits (12:21). */

/* Page directory index (bits 22:31). */
#define PDSHIFT (PTSHIFT + PTBITS)   /* First page directory bit. */
#define PDBITS  10                   /* Number of page dir bits. */
#define PDMASK  BITMASK(PDSHIFT, PDBITS)// Page directory bits (22:31)

/* Obtains page table index from a virtual address. */
static inline unsigned pt_no (const void *va)
/* Obtains page directory index from a virtual address. */
static inline uintptr_t pd_no (const void *va)

#define PTE_FLAGS 0x00000fff    /* Flag bits. */
#define PTE_ADDR  0xfffff000    /* Address bits. */
#define PTE_AVL   0x00000e00    /* Bits available for OS use. */
#define PTE_P 0x1               /* 1=present, 0=not present. */
#define PTE_W 0x2               /* 1=read/write, 0=read-only. */
#define PTE_U 0x4               /* 1=user/kernel, 0=kernel only. */
#define PTE_A 0x20              /* 1=accessed, 0=not acccessed. */
#define PTE_D 0x40              /* 1=dirty, 0=not dirty (PTEs only). */

/* Returns a PDE that points to page table PT. */
static inline uint32_t pde_create (uint32_t *pt)
/* Returns a pointer to the page table that page directory entry
   PDE, which must "present", points to. */
static inline uint32_t *pde_get_pt (uint32_t pde)

/* Returns a PTE that points to PAGE.
   The PTE's page is readable.
   If WRITABLE is true then it will be writable as well.
   The page will be usable only by ring 0 code (the kernel). */
static inline uint32_t pte_create_kernel (void *page, bool writable)

/* Returns a PTE that points to PAGE.
   The PTE's page is readable.
   If WRITABLE is true then it will be writable as well.
   The page will be usable by both user and kernel code. */
static inline uint32_t pte_create_user (void *page, bool writable)

/* Returns a pointer to the page that page table entry PTE points
   to. */
static inline void *pte_get_page (uint32_t pte)
```
 이제 알아볼 것은 핀토스 상에서 각 페이지 테이블(디렉토리)의 엔트리가 어떻게 이뤄져 있는지 
여부이다. 우선 처음 정의된 매크로들은 각각 페이지 테이블과 페이지 디렉토리가 가상 주소 
상에서 어떤 비트로 매핑되어있는지 표현하고 있다. 위에서 설명한 것처럼 10 / 10 / 10 으로 
나뉘어진 부분을 정의하고 있다. `pt_no()`는 가상 주소 상에서 페이지 테이블 인덱스를 
반환하며, `pd_no()` 는 페이지 디렉토리 인덱스를 반환하고 있다. 현재 코드에서 구현되어 있는 
방식은 페이지 테이블에 해당하는 입력을 받으면, 그 테이블의 주소를 저장하기 위한 페이지 
디렉토리 엔트리를 생성하는 `pde_create()` 와 현재 입력받은 페이지 디렉토리 엔트리를 
바탕으로 페이지 테이블의 주소를 반환하는 `pde_get_pt()` 로 이뤄져 있다. 또한, 페이지 
디렉토리 엔트리에서는 위에서 정의된 메크로를 바탕으로 각 페이지 테이블에 접근 시 확인할 
플래그와 함께 저장되어 있다. 따라서 하단에 제시된 그림과 같이 상위 20개 bit가 페이지 
테이블의 주소를 이루고 있으며, 하단의 12 비트가 플래그를 저장하게 된다. 이 구조는 
마찬가지로 페이지 테이블 엔트리에서도 동일하게 사용하고 있다. 다음으로 구현된 부분은 
`pte_create_kernel()`과 `pte_create_user()`로 함수의 이름에서 알 수 있는 것처럼 
page와 writable을 입력받아 페이지에 해당하는 페이지 테이블 엔트리를 생성하고 각각 
커널인지 여부와 유저인지 여부에 맞춰 생성하게 된다. 또한, 입력받은 페이지 테이블 엔트리를 
바탕으로 다시 페이지의 가상 주소를 반환하는 `pte_get_page()`가 구현되어 있다. 또한, 
생성 시에 각각 풀에서 lock을 취득하여 원자성을 보장하게 된다. 이렇게 페이지 테이블 
엔트리가 구현된 것을 확인할 수 있지만, 하지만 이 코드 상에서는 페이지 테이블이나 
디렉토리를 직접 생성하여 관리하지 않고 있다. 따라서 마지막으로 알아보고자 하는 코드는 
pagedir.c 에서 실제 어떻게 페이지 디렉토리가 운영되고 있는지를 확인해보도록 하자.

```
31                                 12 11                     0
+------------------------------------+------------------------+
|         Physical Address           |         Flags          |
+------------------------------------+------------------------+
```

 추가적으로 플래그는 AVL(운영체제에서 사용할 수 있는 비트), P(존재 여부), W(쓸 수 
있는지 여부), U(유저 접근 여부), A(최근 에 사용되었는지 여부), D(데이터가 수정되었는지 
여부) 를 각각 저장하게 된다.

#### Page Directory Table
```C
/* From userprog/pagedir.c */
/* Creates a new page directory that has mappings for kernel
   virtual addresses, but none for user virtual addresses.
   Returns the new page directory, or a null pointer if memory
   allocation fails. */
uint32_t * pagedir_create (void) 

/* Destroys page directory PD, freeing all the pages it
   references. */
void pagedir_destroy (uint32_t *pd) 

/* Returns the address of the page table entry for virtual
   address VADDR in page directory PD.
   If PD does not have a page table for VADDR, behavior depends
   on CREATE.  If CREATE is true, then a new page table is
   created and a pointer into it is returned.  Otherwise, a null
   pointer is returned. */
static uint32_t *
lookup_page (uint32_t *pd, const void *vaddr, bool create)

/* Adds a mapping in page directory PD from user virtual page
   UPAGE to the physical frame identified by kernel virtual
   address KPAGE.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   If WRITABLE is true, the new page is read/write;
   otherwise it is read-only.
   Returns true if successful, false if memory allocation
   failed. */
bool pagedir_set_page 
(uint32_t *pd, void *upage, void *kpage, bool writable)

/* Looks up the physical address that corresponds to user virtual
   address UADDR in PD.  Returns the kernel virtual address
   corresponding to that physical address, or a null pointer if
   UADDR is unmapped. */
void * pagedir_get_page (uint32_t *pd, const void *uaddr) 

/* Marks user virtual page UPAGE "not present" in page
   directory PD.  Later accesses to the page will fault.  Other
   bits in the page table entry are preserved.
   UPAGE need not be mapped. */
void pagedir_clear_page (uint32_t *pd, void *upage) 

/* Returns true if the PTE for virtual page VPAGE in PD is dirty,
   that is, if the page has been modified since the PTE was
   installed.
   Returns false if PD contains no PTE for VPAGE. */
bool pagedir_is_dirty (uint32_t *pd, const void *vpage) 
void pagedir_set_dirty (uint32_t *pd, const void *vpage, bool dirty) 

/* Returns true if the PTE for virtual page VPAGE in PD has been
   accessed recently, that is, between the time the PTE was
   installed and the last time it was cleared.  Returns false if
   PD contains no PTE for VPAGE. */
bool pagedir_is_accessed (uint32_t *pd, const void *vpage) 
void pagedir_set_accessed (uint32_t *pd, const void *vpage, bool accessed) 

/* Loads page directory PD into the CPU's page directory base
   register. */
void pagedir_activate (uint32_t *pd) 

/* Returns the currently active page directory. */
static uint32_t * active_pd (void) 

/* Some page table changes can cause the CPU's translation
   lookaside buffer (TLB) to become out-of-sync with the page
   table.  When this happens, we have to "invalidate" the TLB by
   re-activating it.

   This function invalidates the TLB if PD is the active page
   directory.  (If PD is not active then its entries are not in
   the TLB, so there is no need to invalidate anything.) */
static void invalidate_pagedir (uint32_t *pd) 
```
 페이지 디렉토리를 생각해보면 페이지 디렉토리 엔트리는 각각 32 bit (4 B)로 구성되어 
있고, 약 2^10개의 엔트리가 필요하므로, 총 4 KB 의 용량이 필요하다. 즉, 핀토스에서 
사용하는 페이지 크기와 동일하므로 페이지 디렉토리를 만드는 `pagedir_create()` 를 통해  
프로세스에서 사용할 페이지 디렉토리를 생성하게 된다. 따라서 palloc_get_page(0) 을 통해
한 개의 페이지를 RAM 에서 할당하며 이 영역에 페이지 디렉토리가 저장되며, 커널이 가진 
정보로 설정하기 위해 init_page_dir의 내용을 디렉토리로 초기에 설정하게 된다. 
그러한 이유는 커널 가상 공간이 PHYS_BASE 이상의 영역으로 대응되어 있으므로 모든 
프로세스에서 해당 부분에 대해서는 동일한 공간을 공유하게 된다. 따라서 동일하게 설정해준다.

 특정 가상 주소의 데이터를 읽기 위해서는 우선 페이지 디렉토리에서 대응되는 페이지 테이블을 
찾아서 페이지 테이블에서 실제 물리 메모리의 프레임 주소를 찾아 거기서 데이터를 읽는다.
따라서 페이지 디렉토리에서 입력받은 vaddr에 대응되는 페이지 테이블 엔트리를 반환하는 함수인
`lookup_page()` 가 구현되어 있다. 이 함수에서는 우선 페이지 디렉토리 엔트리를 계산하여
페이지 테이블이 없는 경우에는 새로운 페이지 테이블을 생성(플래그가 설정된 경우)하여 테이블 
내에서 페이지 테이블 엔트리(가상 주소)를 반환하게 된다. 이렇게 물리 메모리에 대응되는
페이지를 찾을 수 있고, 그러한 페이지를 관리하기 위한 함수들로, `pagedir_is_dirty()`,
`pagedir_set_dirty()`, `pagedir_is_accessed()`, `pagedir_set_accessed()`가 
있다. 각각 수정 여부를 반환/설정 하고, 최근에 접근되었는지 여부를 확인/설정 한다. 반대로 
페이지 디렉토리 자체를 삭제하는 함수인 `pagedir_destroy()` 에서는 각 엔트리를 모조리
확인하여 모든 페이지 테이블을 해제하고, 그 다음에 종료하게 된다. `pagedir_clear_page()`
에서는 유저 페이지를 비활성화 하는 함수로 A 플래그를 해제하고, `invalidate_pagedir()`
통해서 변경된 내용을 가진 페이지 디렉토리를 다시 TLB에 업데이트하게 된다. 이외에도 
`pagedir_set_page()` 에서는 새로운 매핑을 추가해주는 함수로 유저 가상 페이지를 
실제 물리 페이지로 매핑을 페이지 디렉토리에 추가해주고, 업데이트 해주는 과정을 수행한다.

 이 때 추가적으로 살펴볼 곳은 바로 CPU의 CR3 레지스터에 할당되는 과정을 살펴봐야 한다.
우선 CPU에서 가상 주소를 물리 주소로 바꿀 때 사용하는 MMU에서는 TLB를 갖고 있다. TLB는
최근에 사용된 가상 주소와 물리 주소의 관계를 저장하는 캐시로, TLB를 우선 확인하여 가상
주소를 변환하게 된다. 따라서 우리가 특정 페이지 디렉토리 엔트리를 비활성화하게 되면,
현재 CPU의 TLB에 저장된 페이지 디렉토리와 차이가 발생하게 된다. 따라서 이를 방지하기 위해
다시 업데이트 해주는 과정이 필요하게 되므로 `pagedir_set_accessed()` 에서는 
`invalidate_pagedir()` 를 통해 TLB에 최신 정보로 업데이트 해준다고 이해할 수 있다.
`pagedir_activate()` 함수에서 수행하는 어셈블리 루틴에서 확인할 수 있는 것처럼 CPU의
CR3 레지스터가 저장하고 있는 페이지 디렉토리 정보를 해당 함수에서 다시 업데이트하게 된다.

### Basic Descriptions and Limitations
 위의 분석을 통해 알아볼 수 있었던 것처럼 현재 메모리 (RAM) 을 사용하기 위해서는 
page allocate 부분을 활용하여 페이지 단위로 영역을 할당해서 사용하게 된다. 페이지
얼로케이터 내에서는 풀을 바탕으로 각 풀에서 할당할 수 있는 연속된 영역에 맞춰서 할당하는
방식으로만 구현이 되어있는데, 이 때 남아있는 물리 메모리만으로는 여러 프로세스가 실행하는
큰 메모리가 사용되는 프로그램을 실행할 수 없다. 따라서 사용하지 않는 데이터를 디스크로 옮겨
데이터를 저장하는 방식으로 더 큰 용량을 지원해야 한다. 하지만 현재 구현에서는 
`palloc_get_page()` 를 통해 새로운 페이지를 할당하지만, 만약 용량이 부족하다면, 
새로운 프레임을 할당하지 못하고 문제를 발생하게 된다. 따라서 이번 구현에서는 
새로운 프레임 테이블을 선언하여 실제 물리 메모리 상에 할당된 프레임들을 관리해야 하고,
자주 사용하지 않는 프레임이 있다면 디스크로 swap out 해야 하고, 사용해야 할 프레임이 
있지만 메모리에 없다면 `page_fault()` 가 발생하여 swap in 을 통해 메모리로 해당
프레임을 불러와야 한다. 이 때 새로운 프레임을 할당하지 못하는 경우에는 기존에 있던 
프레임 중에 방출(eviction) 될 프레임을 선정하는 알고리즘을 선언하고, 해당 알고리즘에 
맞게 프레임을 선택하여 방출하는 구현이 필요하게 된다.

### Necessity and Design Proposal
 우선 Frame Table은 RAM의 상태를 관리해야 한다. 따라서 RAM의 용량을 Frame 단위로 
나누고 각 프레임에 대해서 현재 할당 여부, 최근에 접근되었는지 여부, 수정되었는지 여부 등을 
함께 저장하여 관리해야 한다. 프레임 테이블의 핵심은 바로 사용 가능한 프레임을 찾고, 
만약에 가능한 게 없다면 스왑할 프레임을 골라서 프레임을 할당해주는 것을 효율적으로 수행해야 
한다. 따라서 그러한 프레임 테이블을 저장하는 엔트리를 위한 구조체를 정의하였다.

 또한, 프레임 테이블은 우선 모든 프로세스에서 공유하는 전역 변수로 선언해야 한다. 
그 이유는 당연하게도 RAM은 모든 프로세스가 공유하는 자원이므로, 프레임 테이블에 접근해서
사용 가능한 프레임을 찾을 수 있어야 한다. 따라서 프레임 테이블을 사용하기 전에는 항상
lock을 얻어서 안전성을 보장해야 한다. 

```C
/* From vm/frame.h */
struct fte
  {
    void *upage; /* user virtual memory address */
    void *kpage; /* kernel frame memory address */
    struct process *process; /* which process current frame in ? */
    bool pinning; /* pinned or not ? */
    struct list_elem elem;
  }

/* From vm/frame.c */
struct list ft;
struct lock ft_lock;
```

 선언할 구조체를 보면 우선 저장할 유저 가상 페이지의 주소를 담고 있는 upage와
커널이 저장하고 있을 kpage를 선언하였다. kpage는 palloc_get_page(PAL_USER)로
할당되는 방식으로 구현할 예정이며, 공식 문저에 나와있는 것처럼 유저 풀에서만 프레임을
할당해야 한다. 또한, 어떤 유저 프로세스에서 해당 프레임이 할당되었는지 확인해야, 
이후 페이지 폴트 혹은 다양한 이슈 발생 시 확인할 수 있으므로 프로세스를 저장하고자 한다.
추가적으로 고정된 페이지가 있다면 해당 페이지는 swap out 되지 않도록 할당해야 하므로,
고정 여부를 저장하고, list 에 넣을 수 있도록 elem을 선언하였다.


 이 구조체를 기반으로 전체 프레임 테이블을 저장할 전역 변수는 ft이고, 해당 ft에 
접근하기 위해서는 ft_lock을 통해 접근이 이뤄지게 된다. 우선 ft는 init.c 에서 
`main()` 이 실행되며 핀토스가 구성될 때 함께 초기화가 이뤄져서 준비되어야 한다.
이후 유저 프로세스가 실행되었을 때 유저 프로세스를 위해서 새로운 프레임을 할당하는 과정에서
우선 락을 획득하고, 기존에 구현된 palloc_get_page(PAL_USER) 를 통해서 
유저 풀 내에서 새로운 프레임을 할당해야 한다. 하지만, 이 때 이 과정 중에서 비어있는 
프레임이 없는 경우 방출을 통해서 새로운 공간을 할당해야 한다. 이렇게 비어있는 공간을
만들었다면, fte를 선언하고 프레임 테이블에 추가해준다. 이 과정은 모두 lock 으로 
보호되어야 한다. 또한, 해제하는 과정도 마찬가지로 유사하다. lock 을 얻은 뒤에
`palloc_free_page()` 를 통해 kpage를 할당을 해제하고, `pagedir_clear_page()`를
통해서 현재 프로세스의 페이지 디렉토리에서 기록되어 있는 upage의 연결을 마찬가지로
지워줘야 한다.


 방출 과정을 살펴보도록 하자. 방출 관련 부분은 유저 프로세스에 할당할 새로운 프레임이
없을 경우에 실행되며, 공식 문서에 따라 LRU를 근사하는 clock 알고리즘에 따라서 
방출될 프레임을 선정하게 된다. 기존 page allocator에 설정된 accessed 비트를 활용하여
접근 된 경우 1로 설정되어 있다. 따라서 ft를 순회하면서 accessed가 1로 설정되어
있다면 0으로 바꿔주는 과정을 리스트를 순회하며 수행하다가, 0인 프레임을 만나게 되면
해당 프레임을 `swap_out()` 해주고, 해당하는 프레임 테이블 엔트리 또한 해제하는 방식으로
구현할 수 있을 것으로 생각된다. `swap_out()` 의 보다 자세한 과정은 하단에서 서술하고자
한다. 이 과정 중에 pinning 을 함께 확인하여 설정된 경우 accessed가 0 이더라도 
방출하지 않아야 한다. 이외에도 유저 프로세스가 구성될 때 `setup_stack()`에서 kpage를 
`palloc_get_page() `를 통해서 직접적으로 프레임을 할당해줬는데, 이러한 방식을 페이지
얼로케이터를 직접 사용하는 대신, 프레임 테이블을 거쳐가는 방식으로 구현하여 스택 또한 
프레임 테이블을 통해서 관리해줘야 한다.


 방출 과정에서 또 유념해야 할 부분이 있다. 하단에서 자세히 기술할 예정이지만, 
spt를 이용할 수 있도록 해야 하므로 방출될 vpage가 선정되었다면, 그에 해당하는
spte를 찾아, `swap_out()` 후에 해당하는 값에 맞게 업데이트해주는 부분이 필요하다.
spte는 각 프로세스에 해시맵 형태로 할당되어 있으므로, 현재 프로세스->spt
순으로 접근하여 대응되는 spte를 찾고, `swap_out()` 이후 swapped를 설정하고,
스왑된 슬롯의 id를 받아와서 기록해야 한다.

## Supplemental Page Table
페이지 테이블은 CPU로 하여금 가상 주소를 실제 물리적 주소로 번역하는 데 중요한
역할을 하지만, 페이지 테이블의 각각의 원소는 현재 존재하는 페이지에 대해서만
유효하며 운영체제가 페이지 폴트를 처리하며 현재 존재하지 않는 페이지를 가져올 
때는 페이지 테이블에 있는 정보 이외에 다른 정보를 필요로 하는 경우가 존재한다는
한계점이 존재한다. 이러한 이유 때문에, Pintos 문서에서는 페이지 테이블과 독립된 
자료구조이지만 페이지 테이블을 보조(supplement)하는 자료구조로 보조 페이지 
테이블(Supplemental Page Table; 이후 SPT라 칭함)이라는 자료구조를 제시한다. 

### Basic Descriptions
SPT는 어떠한 가상 주소를 그와 연관된 정보에 대응시킨다는 점에서는 페이지 테이블
과 비슷한 자료구조이다. 하지만, 페이지 테이블이 페이지 번호를 그에 대응되는 
물리적 프레임의 번호와 몇 가지 메타 정보에 대응시키는 것과 달리, SPT는 페이지 
번호를 다른 메타 정보에 대응시킨다는 차이점이 있다. 즉, SPT는 프로세서의 
사양(specification)에 정의된 페이지 테이블의 원소에는 저장할 수 없는 종류의 
정보를 저장하기 위한 자료구조이다.

그렇다면, 각 SPT의 원소(entry, 이후 SPTE라 칭함)에는 어떠한 정보가 들어가야 
할까? Pintos 공식 문서의 'Managing the Supplemental Page Table' 단락을 보면, 
페이지 폴트의 상황에서 운영체제는 SPT를 이용해 해당 메모리 참조가 유효하지만 
물리적 메모리 공간의 한계로 인해 쫓겨나거나(evicted) 아직 디스크로부터 
적재(load)되지 않아 페이지 폴트가 일어났는지, 아니면 애초에 해당 메모리 참조가 
유효하지 않아 페이지 폴트가 일어났는지를 알아낸다. 즉, 각 SPTE는 각 페이지의 
존재성 혹은 참조 가능성에 대한 정보를 담고 있어야 한다. 이는 어떤 가상 주소에 
대한 SPTE가 존재한다면 해당 페이지가 해당 프로세스의 맥락(context) 내에서 
유효함을 의미한다고 해석함으로써 달성 가능하다.

하지만, SPT을 이용해 해당 페이지의 참조 가능성을 알아낸다고 해서
운영체제가 페이지 폴트를 처리하기 위한 모든 작업이 끝나는 것은 아니다. 만약
해당 페이지가 참조 가능하다고 해도, 어떻게 디스크에서 해당 페이지를 가져올 
것인지에 대한 정보를 SPTE는 가지고 있어야 한다. 이를 위해 `devices/block.c`의
`block_read()` 함수의 정의를 살펴보자.

```C
/* From devices/block.c. */
/* Reads sector SECTOR from BLOCK into BUFFER, which must
   have room for BLOCK_SECTOR_SIZE bytes.
   Internally synchronizes accesses to block devices, so external
   per-block device locking is unneeded. */
void
block_read (struct block *block, block_sector_t sector, void *buffer)
{
  check_sector (block, sector);
  block->ops->read (block->aux, sector, buffer);
  block->read_cnt++;
}
```
해당 함수의 정의를 살펴보면, 블록에서 섹터를 읽기 위해 블록과 읽은 내용을 
저장하기 위한 버퍼의 포인터 이외에도 해당 블록의 섹터 번호를 요구함을 알 수
있다. 페이지 폴트 상황에서 디스크에서 가져와야 할 데이터의 크기는 페이지 하나의
크기인 4 KiB로 정해져 있고, 각각의 페이지가 스왑 블록 혹은 파일 시스템 블록 
내에서 연속된 공간을 차지한다고 가정하면 하나의 페이지를 읽기 위해서는 해당
페이지가 실제 블록 장치 내에 존재하는 시작 섹터의 번호만을 알면 전체 페이지를
다 읽어낼 수 있을 것이다. 또한, `block_read()`는 어떠한 블록을 읽을지를 인자로 
요구하는데, 하나의 페이지는 만약 해당 페이지가 물리적 프레임에서 쫓겨난 경우 
스왑 블록에 존재할 수도 있으며, 또는 해당 페이지가 실행 파일의 일부이거나 메모리 
대응 파일의 일부인 경우 파일 시스템 블록에 존재할 수도 있으므로 해당 페이지가 
실제로 어떤 장치에 존재하는지를 나타내는 일종의 플래그 혹은 열거형 또한 SPTE의 
한 멤버로 필요할 것이다.

### Limitations and Necessity
```C
/* From userprog/exception.c. */
/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  ...

  /* If it is caused by invalid address passed to the kernel, kill user process
     while making no harm to the kernel. */
  if (is_user_vaddr (fault_addr) && !user)
    process_exit (-1);

  /* To implement virtual memory, delete the rest of the function
     body, and replace it with code that brings in the page to
     which fault_addr refers. */
  printf ("Page fault at %p: %s error %s page in %s context.\n",
          fault_addr,
          not_present ? "not present" : "rights violation",
          write ? "writing" : "reading",
          user ? "user" : "kernel");
  kill (f);
}
```
`userprog/exception.c`의 페이지 폴트 핸들러 함수를 살펴보면, 현재의 Pintos 
구현은 SPT나 SPTE에 해당하는 개념이 없어, 사용자 프로세스에서 페이지 폴트가 
발생할 시 항상 해당 사용자 프로세스의 버그로 간주하고 해당 프로세스를 종료시키고 
있다. 즉, 현재의 페이지 폴트 핸들러는 사용자 프로세스가 항상 자신의 페이지 
디렉터리에 존재하는 페이지만을 참조할 것을 가정하고, 해당 가정에서 어긋나는 
사용자 프로세스를 종료시키는 방식으로 구현되어 있다. 하지만, 이후 설명할 게으른 
적재(lazy loading)나 메모리 대응 파일(memory mapped file), 페이지 교체 등이
구현되면 이러한 가정은 더 이상 성립하지 않는다. 이제 사용자 프로세스는 파일에 
대응된 메모리 영역에 접근하는 등의 유효한 메모리 참조에 대해서도 페이지 폴트를
발생시킬 수 있다.

SPT를 새로 구현하고, 이러한 SPT를 페이지 폴트 핸들러가 참조하여 사용자 
프로세스의 메모리 접근을 검증하고, 유효한 메모리 접근이라면 해당 페이지를 스왑
블록 혹은 파일 시스템 블록에서 가져오도록 한다면, 사용자 프로세스는 프로세스
실행 시 실행 파일의 모든 부분을 적재할 필요 없이 프로세스의 시작 부분에 해당하는
부분만을 적재하거나 메모리 영역을 파일에 대응시켜 사용할 수 있을 것이다. 또한,
물리적 메모리의 크기를 벗어나는 가상 주소 공간을 페이지의 효율적인 교체 
알고리즘을 통해 지원할 수 있을 것이다.

한 가지 문제로 이러한 자료구조가 전역적으로, 스레드 혹은 프로세스 간에 공유되며
사용되어야 하는지 혹은 각 프로세스에 한정된 자료구조여야되는지에 대한 문제가 
있다. 당연하게도, SPT는 각 프로세스에 한정된 자료구조여야 하는데, 왜냐하면
각 프로세스는 주소 공간을 공유하지 않기 때문에 각각 따로 메모리 대응 파일이나
교체된 페이지, 혹은 아직 적재되지 않은 페이지를 가지고 있기 때문이다.

### Design Proposal
<!-- TODO: Memory-mapping or lazy loading will be discussed on the relevant 
           section. -->
그렇다면, 이러한 SPT를 구현하기 위한 적절한 자료구조는 무엇일까? SPT는 
기본적으로 페이지 번호에서 위에서 서술한 메타 정보들에 대한 대응으로 생각할 수
있다. 이러한 대응을 구현하는 대표적인 자료구조로는 해당 대응에 가능한 모든 
원소를 미리 저장하는 배열이 있지만, SPT 하나당 가능한 SPTE의 개수는 2^20개인데, 
(오프셋을 제외한 페이지 번호의 길이가 20 bit이므로) 해당 SPTE 중 실제로는
대부분이 사용되지 않으므로 배열로 이를 구현한다면 메모리 공간이 낭비될 것이다.
연결 리스트는 어떤 페이지 번호에서 그 페이지의 메타 정보를 가져오는 데 O(n)의
시간이 걸리므로 이러한 구현 또한 실용적이지 못하다.

위의 두 구현, 배열과 연결 리스트의 한계점을 살펴보면 다음과 같은 요구사항을 알
수 있다. 첫째로, SPT를 구현하는 자료구조는 어떤 페이지 번호에서 그 페이지의 
메타 정보를 가져오는데 상수 시간, 혹은 상수 시간에 가까운 극히 짧은 시간이
걸려야 한다. 해당 시간이 길어지면 길어질수록 페이지 폴트 핸들러의 실행 시간이
길어져 전체 커널 구현이 비효율적이 될 것이다. 둘째로, SPT를 구현하는 자료구조는
희박하게(sparsely) 떨어진 SPTE들을 효율적으로 저장해야 한다. 즉, 쓸모없는 SPTE를
저장하느라 불필요한 공간을 낭비하지 않아야 한다.

이러한 특성을 가지는 자료구조로는 대응을 저장하기 위한 다른 방법으로는 해시 
테이블이 있다. 해시 테이블은 어떤 키(key)에서 값(value)으로의 대응을 저장하기
위한 대표적인 자료구조로, 현재 상황에서는 페이지 번호가 키가 되고 메타 정보를
담고 있는 구조체가 값이 될 것이다. 해시 테이블은 특정 키에 대해 해시 함수를 
적용한 해시 값을 먼저 구하고, 그 해시 값을 이용해 배열을 참조하여 값을 얻어낸다. 
이때 해시 값을 이용해 참조되는 배열의 원소의 개수는 실제 가능한 키의 개수보다 
작아 낭비되는 공간이 적다. 하지만, 해시 테이블의 원소의 개수가 실제 가능한 키의 
개수에 비해 작다는 특징은 비둘기집 원리에 의해 필연적으로 해시 
충돌(hash collision)의 가능성을 만든다. 따라서, 일반적인, 그리고 Pintos에서 
제공하는 해시 테이블 구현은 각 배열 원소마다 하나의 리스트를 저장해 해당 
리스트를 순회하여 값을 찾도록 구현하고 있다. 물론 이는 실제 배열 참조보다는
긴 시간이 걸리겠지만, 리스트 순회보다는 상수 시간에 가까운 극히 짧은 시간이므로
효율적인 구현이 가능할 것이다.

#### `struct spte`
```C
/* Maybe into vm/spt.h. */
struct spte 
  {
    int size;
    bool swapped;
    void *uaddr;
    mapid_t mapid;
    block_sector_t index;
    struct hash_elem elem;
  }
```
`spte` 구조체는 SPTE를 나타내기 위한 구조체이다. `struct spte`는 먼저, 해당
페이지가 4 KiB 공간 내에서 실제로 데이터를 가지고 있는 공간이 얼마인지를 
나타내기 위한 `size` 멤버를 가지고 있다. 이는 해당 크기를 넘어가는 바이트들을
불필요하게 블록에서 읽어들이지 않고, 해당 공간을 그냥 0으로 채우기 위해 
존재한다. 또한, 해당 SPTE에 대응되는 페이지가 현재 스왑 블록에 존재하는지 혹은 
파일 시스템 블록에 존재하는지를 나타내기 위한 `swapped` 멤버를 가지고 있다. 
`uaddr`은 해당 SPTE에 연관된 페이지의 사용자 영역 가상 주소이다.


또한, 해당 SPTE가 만약 메모리 파일 대응으로 가상 주소 공간에 대응된 파일이라면,
해당 SPTE와 연관된 대응의 식별자(map identifier)를 저장하기 위한 `mapid` 멤버를
가지고 있다. 만약 해당 SPTE와 연관된 페이지가 메모리 파일 대응으로 적재되는
페이지가 아니라면 `mapid`는 `MAPID_ERROR`로 초기화되어 해당 페이지가 메모리
대응 파일로 대응된 페이지가 아님을 나타낸다. 또, 해당 블록에서 어떤 섹터에 해당 
페이지가 존재하는지를 나타내기 위한 멤버로 `index`를 가지고 있으며, 해시 
테이블에 해당 구조체를 추가하기 위한 원소로 `elem`을 가지고 있다.

#### `struct process`
```C
/* From userprog/process.h. */
/* An user process. */
struct process
  {
    ...
    /* Owned by vm/spt.c. */
    struct hash spt;
  };
```
원래의 `process` 구조체는 각 프로세스의 페이지 디렉터리만을 저장했지만, 새로운 
구현에서는 위에서 논의된 SPT를 어디엔가 저장해야 하고, SPT는 각 사용자 
프로세스마다 존재하는 자료구조이므로 각 `struct process`가 이러한 SPT를 저장해야
할 것이다. 또한, 프로세스의 초기화 과정에서 SPT 또한 `hash_init()` 함수를 통해 
초기화되어야 한다.

#### `page_fault()`
```C
/* From userprog/exception.c. */
static void
page_fault (struct intr_frame *f) 
{
  ...

  /* If it is caused by invalid address passed to the kernel, kill user process
     while making no harm to the kernel. */
  if (is_user_vaddr (fault_addr) && !user)
    process_exit (-1);

  /*
    1. Check if current process's SPT has an entry about the faulting page.
    2. If the SPT does not have such SPTE, terminate the malicious user process.
    3. Else, according to the information in the entry, allocate a physical 
       frame which the page would be loaded into. This is done with the frame
       table.
      - If the physical page frame is full, evict a frame by LRU-approximating
        page replacement algorithm.
      - Also, find the swap slot by the swap table.
      - Then, modify the evicted page's SPTE so that it now holds the block and
        sector where the page is evicted.
    4. Load the faulting page into the frame using block_read().
    5. Go back to the normal execution flow. 
  */
  ...
}
```
페이지 폴트 핸들러의 원래 구현은 상술한 바와 같이 페이지 폴트를 발생시킨 
페이지가 실제 유효하게 접근 가능한 페이지인지를 검증하지 않고, 무조건 페이지
폴트를 발생시킨 사용자 프로세스를 종료시키도록 구현되어 있었다. 새로운 
구현에서는, 위에 서술한 의사 코드에 따라 해당 페이지가 실제로 유효하게 접근
가능한 페이지인지를 검증하고, 만약 접근 가능한 페이지라면 이를 디스크에서 
적재하는 과정을 구현하여야 한다.

## Lazy Loading and Paging
현재 Pintos의 실행 파일 적재 과정은 해당 실행 파일 전체를 메모리에 적재하도록 
되어 있다. 이러한 구현은 간단하지만, 실행 파일의 특정 부분이 불필요한 경우에도
해당 부분이 메모리에 적재되어 물리적 메모리 공간이 낭비되고 실행 파일이 큰
경우 프로세스 실행 시 디스크 입출력에 소요되는 시간이 길어지는 단점이 있었다.
따라서, 이번 프로젝트에서는 실행 파일을 프로세스 실행 시 전부 적재하지 않고
필요한 부분만을 적재하는 게으른 적재(lazy loading)을 구현하여야 한다.

또한, 페이지를 프레임에 적재할 때, 물리적 프레임 중 비어 있는 공간이 없다면
현재 프레임을 점유하고 있는 페이지 중 적절한 것을 선택하여 새로운 것으로 
교체하여야 할 것이다. 만약 특정 페이지가 프레임에서 교체되어 디스크로 
쫓겨난다면, 나중에 해당 페이지가 필요할 것에 대비해 해당 페이지를 어떻게 다시 
프레임에 적재할 지에 대한 정보를 SPT에 저장해야 할 것이다. 또한, 페이지가 
교체되어 디스크로 쫓겨날 때, 항상 해당 페이지의 내용을 디스크에 쓰는 것이 아닌,
해당 페이지가 수정되었을(dirty) 때에만 해당 페이지의 내용을 디스크에 쓰는 것이
효율적일 것이다. 마지막으로, 상술한 바와 같이 페이지 폴트 핸들러는 페이지 폴트
발생시 SPT를 탐색하여 해당 페이지가 디스크에 존재하는지를 확인하고 만약 
존재한다면 페이지를 다시 메모리에 적재해야 한다. 이러한 일련의 과정을 페이징이라
칭한다.

### basic descriptions
실행 파일로부터 새로운 프로세스를 실행할 때, 운영체제에서는 디스크에서 해당 실행
파일을 메모리로 적재하는 과정을 거치게 된다. 이러한 실행 파일 적재 과정은 
필수적이지만, 모든 실행 파일을 한 번에 메모리로 적재할 필요는 없다. 즉, 프로세스
실행 시에는 실행에 필요한 최소한의 물리적 메모리, 즉 프로세스 실행 과정 중
쓰일 초기 스택만을 할당하고, 이후 필요한 코드는 실행 파일에서 필요할 때마다
적재하여 사용한다면 물리적 메모리 공간을 절약하고 프로세스의 초기 실행 과정을
단축할 수 있을 것이다.

이러한 게으른 적재 과정을 위해서는, 만약 실제로 해당 페이지가 필요할 시 이를
어떤 블록의 몇 번째 섹터에서 가져와야 할 지를 커널이 기억하고 해당 페이지를
가져올 수 있도록 하는 과정이 필요할 것이다. 이러한 정보는 상술한 SPT에 
저장되어야 할 것이다. 즉, 게으른 적재를 구현하기 위해서는 프로세스의 초기 실행
과정에서 해당 실행 파일에 대한 정보를 SPT에 저장하는 과정이 필요하다. 이후
페이지 폴트가 발생하면 상술한 페이지 폴트 핸들러 의사 코드에 따라 실제 블럭에서
해당 실행 파일을 적재하는 과정을 거치게 된다.

또한, 페이징을 위해서 상술한 바와 같이 새로운 페이지를 프레임으로 적재할 때
현재 프레임 테이블을 순회하여 빈 공간을 찾고, 빈 공간이 없다면 적절한 페이지를
찾아 해당 페이지를 새로운 페이지로 교체하며, 만약 교체 대상이 된 페이지가
수정되었다면 그 페이지의 내용을 다시 스왑 블록 혹은 파일 시스템 블록에 쓰는
과정을 거쳐야 한다. 

### limitations and necessity
게으른 적재와 페이징을 구현하기 전에, 현재 Pintos의 실행 파일 적재 과정을
살펴보자. 

```C
/* From userprog/process.c. */
/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;
      ...
      switch (phdr.p_type) 
        {
          ...
        case PT_LOAD:
        ...
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
        ...
        }
    }
    ...
}
```
`userprog/process.c`의 `load()` 함수를 살펴보면, ELF 헤더의 각 원소를 순회하며
만약 해당 원소가 적재 가능하다면 `load_segment()` 함수를 호출하여 해당 원소를
메모리에 적재하는 것을 볼 수 있다. 그렇다면, `load_segment()` 함수에서는 어떻게
해당 원소를 메모리에 적재할까?

```C
/* From userprog/process.c. */
/* Loads a segment starting at offset ofs in file at address
   upage.  In total, read_bytes + zero_bytes bytes of virtual
   memory are initialized, as follows:

        - Read_bytes bytes at upage must be read from file
          starting at offset ofs.

        - Zero_bytes bytes at upage + read_bytes must be zeroed.

   The pages initialized by this function must be writable by the
   user process if writable is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ...
  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         we will read page_read_bytes bytes from file
         and zero the final page_zero_bytes bytes. */
      size_t page_read_bytes = read_bytes < pgsize ? read_bytes : pgsize;
      size_t page_zero_bytes = pgsize - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (pal_user);
      if (kpage == null)
        return false;

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

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += pgsize;
    }
  return true;
}
```
`load_segment()` 함수에서는 새로운 페이지를 할당받아 인자 `file`의 내용을 
`ofs`부터 시작하여 `read_bytes`만큼 읽어들이고, 이후 해당 페이지에 읽어들인
내용을 쓰는 작업을 반복하는 것을 볼 수 있다. 이때, `install_page()` 호출을 
이용해 인자로 전달받은 사용자 가상 주소 `upage`에서 새롭게 할당받은 프레임, 혹은
물리적 주소 `kpage`로의 대응을 현재 프로세스의 페이지 디렉터리에 추가하는 것을 
볼 수 있다. 이때 중요한 점은 `load_segment()`의 현재 구현에서는 이러한 작업을 
인자로 넘어온 `read_bytes`와 `zero_bytes`가 0 이상일 때 계속 반복하여 결국 해당 
실행 파일의 모든 페이지를 읽어들인다는 점이다. 즉, Pintos의 현재 구현은 상술한
게으른 적재가 구현되어 있지 않고, 실행 파일의 전체 내용을 메모리로 읽어들이고
있다.

또한, 위에서 `userprog/exception.c`의 페이지 폴트 핸들러를 살펴본 바와 같이, 
현재 Pintos에서는 페이지 폴트 발생 시 비어 있는 프레임에 새로운 페이지를 
할당하거나 이미 있는 프레임에 새로운 페이지를 교체하는 작업 없이, 그저 페이지
폴트를 발생시킨 유저 프로세스를 종료시키는 것을 볼 수 있다. 즉, 현재
Pintos에서는 페이지 교체 알고리즘이나 페이지 되쓰기(write-back) 작업을 포함하는
페이징이 구현되지 않았다.

### Design Proposal
이러한 게으른 적재와 페이징을 구현하기 위해서는, 위에서 서술한 페이지 폴트 
핸들러의 수정과 함께 프로세스 초기 실행시 호출되는 `load_segment()`의 정의또한
수정해야 한다. 즉, 현재 `load_segment()`는 프로세스 실행과 동시에 실행 파일을
전부 적재하도록 되어 있는데, 새로운 `load_segment()`에서는 이들에 대한 정보를
SPT에만 저장하고, 실제 해당 페이지가 적재되는 것은 해당 실행 파일에서 아직 
메모리에 적재되지 않은 부분을 참조하는 도중 발생한 페이지 폴트의 핸들러에서
이루어져야 한다. 이러한 아이디어를 반영한 `load_segment()`의 수정된 버전의
의사 코드는 다음과 같을 것이다.

```C
/* From userprog/process.c. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ...
  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         we will read page_read_bytes bytes from file
         and zero the final page_zero_bytes bytes. */
      size_t page_read_bytes = read_bytes < pgsize ? read_bytes : pgsize;
      size_t page_zero_bytes = pgsize - page_read_bytes;

      /*
        After calculating the number of bytes to be read and the number of bytes
        to be filled with zeros, we should do the following.

        1. Gets the sector number of the underlying block of the file.
        2. Create new SPTE which holds the sector number from the first step.
        3. Insert the SPTE with the hash key of upage into the SPT of this newly
           executed process.
        4. After this step, the executable file will be demand-paged whenever
           the process trys to executed yet unloaded portion of the executable,
           by the modified page fault handler discussed above.
      */

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += pgsize;
    }
  return true;
}
```
즉, 새로운 버전의 `load_segment()`에서는 실행 파일을 전부 메모리에 적재하지 
않고, 해당 실행 파일의 사용자 가상 주소에서 해당 실행 파일이 존재하는 섹터 
번호만을 SPT에 추가한다.

## Stack Growth
### Basic Descriptions & Limitations
 기존 pintos에서는 stack 이 single page(4 KB)로 제한되어 있었다. 
즉, single page를 넘어가는 stack space가 필요한 경우에는 page fault가 
발생하여 이를 handling은 하지만, stack 자체를 확장하는 것이 아니라 
단순히 오류를 처리하는 것에 가까웠다. 결국 이는 큰 용량을 가지는 프로그램이 
실행할 수 없게 되는 문제를 야기하게 된다. 따라서 이를 해결하고자 한다.

### Necessity
 기존 구현을 간단히 살펴보면 ```userprog/exception.c``` 에 위치한 
```static void page_fault (struct intr_frame *f) ``` 
을 통해서 구현되어 있다. 해당 handler의 실행 과정 중에서 fault가 발생한
address에 따라서 stack growth 가 필요한 경우 실행할 수 있도록 구현을 수정해야 한다.
 우선 필요한 것은 입력받은 인터럽트 프레임 내에서 해당 주소가 어떤 상황인지 확인해야 한다.
`fault_addr`을 확인하여 stack growth 가 필요한지 여부를 알아야 한다. 공식 문서에서 
확인할 수 있는 것처럼 x86 에서 기본적으로 esp 아래의 주소를 참조하는 경우는 PUSH 혹은
PUSHA 인스트럭션이 존재하는데 각각 4 btye, 32 byte 아래의 주소를 참조하게 된다. 
따라서 최대 32 byte 이내의 주소를 참조했을 때 stack growth를 통해 지원해야 하며, 
나머지 경우에 대해서는 해당 명령을 오류로 처리하여, 프로세스를 종료해야 한다. 또한, 
마찬가지로 공식 문서에서 추천하는 것처럼 8 MB (PHYS_BASE로 부터)를 최대 stack size로 
제한하여, 이 영역을 초과하게 된다면 heap 영역까지 초과할 수 있으므로 마찬가지로 8 MB를 
초과한 경우에도 프로세스를 종료해야 한다. 마지막으로 확인할 부분은 당연하게도 할당되었는지 
여부를 확인해야 한다. 즉, Supplemental Page Table을 확인하여 `fault_addr`에 
해당하는 page가 이미 로드가 되어있는지 여부를 확인하여, 해당 page가 존재할 경우에는 
확장을 따로 수행하지 않는 대신에, write이 가능한 경우에는 해당 page를 다시 
memeory로 불러와야 한다. 또한, 마지막으로 필요한 것은 user mode가 아닌 
kernel mode 에서 실행되었을 경우에는 `f->esp`가 kernel 의 stack 이므로 stack
growth를 구현하기 위해서는 user thread의 esp를 따로 저장하고, 그 stack을 
키워주는 부분을 고려해야 한다.

### Design Proposal
 위에서 언급한 바와 같이 exception.c 의 `page_fault()` 를 수정하는 방식으로 구현한다.
기존 코드에 존재하는 not_present, write, user를 바탕으로 not_present 시에는 
`fault_addr`을 확인하여 Supplemental Page Table을 직접 확인하여 load 되었는지 
여부를 확인하고 아니라면 해당 주소가 32 bytes 이내인지 여부, `PHYS_BASE` 로부터 8MB 
이내인지 여부를 확인해야 한다. 만약 전부 해당한다면, user mode 인 경우에는 `f->esp`를 
키워주고, kernel mode인 경우에는 기존에 실행되고 있던 user thread에 저장된 stack
pointer를 저장하여 해당 esp를 growth 해주는 방식으로 구현하고자 한다. 이 때 사용되는
pointer는 syscall.c에서 `syscall_handler()`가 실행될 때 thread에 `f->esp`를 
저장하여 놓고, 이후 `page_fault()` 시에 저장한 그 pointer 값을 사용하는 방식으로 
구현하고자 한다. 마지막으로 stack growth 시에는 `pg_round_down (fault_addr)` 
에 해당하는 주소에 새로운 page를 할당하는 방식으로 구현하고자 한다.

## File Memory Mapping
사용자 프로세스가 파일을 읽어들일 때, `read()` 시스템 호출을 사용해 해당 파일의
내용을 미리 확보된 메모리 공간에 한 번에 읽어들일 수도 있지만, 다른 방법으로
사용자 프로세스의 가상 주소 공간의 일부를 해당 파일에 대응시켜 해당 파일을 
필요할 때만 물리적 페이지에 읽어들일 수도 있다. 이러한 대응을 이용한 파일 입출력
방식을 메모리 대응 파일(memory mapped file)이라 칭한다. 메모리 대응 파일은
일반적인 `read()` 시스템 호출을 이용한 방식과 비교하였을 때, 파일을 읽어들이기
위한 디스크 입출력이 필요할 때만 이루어진다는 특징이 있다. 만약 읽어들일 파일의
크기가 작다면 파일을 미리 메모리에 적재하는 `read()`를 이용한 방식이 효과적일
수 있지만, 용량이 큰 파일을 읽어들일 때는 필요한 부분만을 메모리에 적재하는
메모리 대응 방식이 효과적일 것이다.

### Basic Descriptions
이러한 파일 메모리 대응을 사용자 프로세스에 제공하는 시스템 호출 중 대표적인
것으로 Unix의 `mmap()`이 존재한다. Pintos 문서에 서술된 `mmap()`의 함수 
원형(prototype)과 Unix `mmap()`의 함수 원형은 조금 다르지만, 파일과 해당 파일에
대응될 주소를 인자로 받아 해당 파일에 대한 메모리 주소 공간 상의 대응을 
생성한다는 면에서는 같다. `mmap()`으로 대응을 생성했다면 사용이 끝난 후에 이러한
대응을 해제해야 한다. `munmap()`은 Unix에서 이러한 기능을 제공하는 시스템 
호출로, Pintos의 `munmap()`은 핸들을 이용하여 한 번에 해당 핸들이 가리키는
대응 전체를 해제할 수밖에 없다는 기능상 한계점을 제외하면 크게 다른 점은 없다.

`mmap()`의 함수 원형을 자세히 살펴보면, 파일 설명자(file descriptor) `fd`와
해당 파일을 대응시킬 시작 주소 `addr`를 받아 해당 대응에 대한 `mappid_t` 형의
핸들을 반환하는 것을 알 수 있다. 즉, Unix의 `mmap()`이 대응될 파일의 길이를
지정할 수 있고 페이지 단위로 정렬된 주소를 반환한다는 점과 달리, Pintos의 
`mmap()`은 반드시 파일 전체를 대응시켜야 하고 대응의 시작 주소는 항상 인자로
전달된 주소로 고정되며, 대응된 가상 주소 공간의 시작 주소를 반환하기보다는
해당 대응 전체에 대한 핸들을 반환하는 것을 알 수 있다.

`munmap()`의 함수 원형은 해당 대응을 나타내는 핸들을 받아 해당 대응 전체를 할당
해제하는 것을 볼 수 있다. `mmap()`이 해당 대응 전체에 대한 핸들을 반환하고
`munmap()`이 해당 대응 전체를 할당 해제한다는 것은 커널에서 메모리와 파일의
대응, 그리고 핸들과 실제 페이지에 대한 대응을 어디엔가 저장해야 한다는 뜻이다. 
따라서, `mmap()`과 `munmap()`을 구현하기 위해서는 이러한 대응을 관리하는 
자료구조를 구현하거나, SPT에 이러한 핸들을 저장하고 `munmap()` 호출마다 SPT를
순회하여 핸들에 대응되는 페이지의 목록을 찾아야 한다. 본 구현에서는 후자의 
접근을 택하였다.

### Limitations and Necessity
```C
/* From userprog/syscall.c. */
static void
syscall_handler (struct intr_frame *f) 
{
  int syscall_number = (int) dereference (f->esp, 0, WORD_SIZE);

  switch (syscall_number) {
    case SYS_HALT: halt (); break; 
    case SYS_EXIT: exit (f->esp); break;   
    case SYS_EXEC: f->eax = exec (f->esp); break;
    case SYS_WAIT: f->eax = wait (f->esp); break;
    case SYS_CREATE: f->eax = create (f->esp); break;
    case SYS_REMOVE: f->eax = remove (f->esp); break;
    case SYS_OPEN: f->eax = open (f->esp); break;
    case SYS_FILESIZE: f->eax = filesize (f->esp); break;
    case SYS_READ: f->eax = read (f->esp); break;
    case SYS_WRITE: f->eax = write (f->esp); break;  
    case SYS_SEEK: seek (f->esp); break;   
    case SYS_TELL: f->eax = tell (f->esp); break;   
    case SYS_CLOSE: close (f->esp); break;
    /* There's no handling routine for mmap() and munmap()! */
  }
}
```
현재의 Pintos 구현을 살펴보면, `mmap()`과 `munmap()` 시스템 호출을 처리하기 위한
시스템 호출 핸들러가 구현되어 있지 않으며, 해당 시스템 호출이 발생하였을 시 
핸들러에 실행 흐름을 넘겨주는 절차도 구현되어 있지 않은 것을 볼 수 있다. 

### Design Proposal

#### `syscall_handler()`
```C
/* From userprog/syscall.c. */
static void
syscall_handler (struct intr_frame *f) 
{
  int syscall_number = (int) dereference (f->esp, 0, WORD_SIZE);

  switch (syscall_number) {
    ...
    case SYS_MMAP: f->eax = mmap (f->esp); break;
    case SYS_MUNMAP: munmap (f->esp); break;
  }
}
```
먼저, 해당 시스템 호출을 정상적으로 처리하기 위해서는 해당 시스템 호출에 대한
핸들러로 실행 흐름을 넘겨주는 루틴을 구현해야 한다.

#### `mmap()` 
```C
/* Maybe into userprog/syscall.c. */
static uint32_t
mmap (void *esp)
{
  int fd = (int) dereference (esp, 1, WORD_SIZE);
  void *addr = (void *) dereference (esp, 2, WORD_SIZE);
  struct file *fp = retrieve_fp (fd);

  if (fp == NULL)
    return MAPID_ERROR;

  /* 
    To implement mmap() system call, this function should do followings;

    1. Allocate a mapid by allocate_mapid() call.
    2. Get the size of file, divide it by the size of a page (4 KiB). Let the
       quotient be N.
    3. Add (N + 1) SPTEs into the SPT of current process. 
      - The SPTE should have the underlying block, sector number, and the
        size within the page if the size of the file is not aligned with the
        size of a page. 
      - The SPTE should also have the mapid for this mapping. 
    4. Return the mapid allocated in the first step.
  */
}
```
`mmap()` 시스템 호출에서는 새로운 식별자를 할당받고, 파일의 크기에 따라 필요한
만큼의 SPTE를 현재 프로세스의 SPT에 추가한다. 만약 파일의 크기가 15 KiB라면,
총 4개(= 15 / 4 + 1 개)의 SPTE를 SPT에 추가하고, 마지막 SPTE의 `size` 멤버는
3으로 초기화되어야 할 것이다. 또한, 각각 SPTE는 인자 `fd`가 가리키는 파일 내부의
블럭에 대한 정보와 해당 블럭 내에서 섹터 번호에 대한 정보를 가지고 있어야 한다.
이후 만약 대응된 페이지에 접근하는 도중 페이지 폴트가 발생하면, 페이지 폴트 
핸들러는 `mmap()`에서 추가된 SPTE를 참조해 필요한 페이지를 디스크에서 가져온다. 

`allocate_mapid()`는 각 대응마다 고유한 식별자를 할당하는 함수로, 
`threads/thread.c`의 `allocate_tid()`나 `filesys/file.c`의 `allocate_fd()`와
거의 비슷한 기능과 역할을 한다.

#### `munmap()`
```C
/* Maybe into userprog/syscall.c. */
static void
munmap (void *esp)
{
  mapid_t mapid = (mapid) dereference (esp, 1, WORD_SIZE);

  /*
    To implement munmap() system call, this function should do followings;

    1. Iterate through the SPT of current process, mark all the pages as not
       present if the page's mapid is same with the mapid to unmap.
    2. Also, for each pages to be unmapped, write them back to the disk if
       the page is dirty.
    3. Remove all of SPTEs whose mapid is to be unmapped, from current process's 
       SPT. 
    4. Deallocate all physical frames occupied by the mappings.
  */
}
```
`munmap()` 시스템 호출을 구현하기 위해서, 커널에서는 먼저 인자로 넘어온 
`mapid`와 동일한 `mapid` 멤버를 가진 SPTE를 현재 프로세스의 SPT에서 찾는다.
이후, 이러한 대응을 해제할 SPTE들과 연관된 모든 페이지들의 페이지 테이블 원소의 
`present` 비트를 0으로 설정해 해당 페이지가 더 이상 유효하지 않음을 나타내고,
만약 해당 페이지의 `dirty` 비트가 1이라면 해당 페이지를 다시 디스크에 쓴다.
이후에는 모든 대응이 해제된 페이지들의 SPTE를 SPT에서 삭제하고, 이들에 연관된
물리적 페이지 프레임 또한 삭제한다.

## Swap Table
### Basic Descriptions and Limitations
 당연하게도 대부분의 HW는 제한된 physical memory space를 갖고 있다. 이 경우 
제한된 메모리 사이즈를 갖고 있기 때문에 해당 메모리가 이미 전부 사용되고 있다면, 
기존에 사용하던 혹은 잘 사용하지 않은 메모리 영역을 비우고, 새롭게 데이터를 읽어와야 한다.
하지만, pintos에서는 memory mapping이 존재하지 않는 상황이라 읽어와야 하는 데이터가 
필요한 경우에도 읽지 못하고 정지되게 된다.

 이 때 현재 메모리가 부족하여 메모리를 비워야 할 경우에 사용하는 영역이 바로 swap disk
이다. disk 상에 존재하는 영역으로, 위의 상황처럼 physical memory가 부족한 경우 
이용할 수 있는 공간이다. 이 때 메모리에서 swap disk로 넘어가는 것을 swap out, 반대로 
메모리로 불러오는 것을 swap in 이라고 한다. 현재 pintos 에서는 이러한 swap disk 는
`struct block`을 통해 구현되어 있다. 각 block은 sector(512 B) 단위로 구성되어 
있으며, swap-in, out은 page 단위로 수행되기 때문에 pintos 내에서는 8개의 sector가
한 개의 swap slot으로 구성된다고 이해할 수 있다. 또한, 각 역할에 따라 block은 한 개씩
정의되어 있고, `block_get_role()` 을 통해 해당하는 역할(`BLOCK_SWAP`)에 맞는 block을
가져와서 사용할 수 있게 된다. 공식 문서에 있는 바와 같이 swap disk는 pintos-mkdisk
swap.dsk 명령어와 --swap-size 옵션을 통해 원하는 용량을 가진 swap disk를 
생성할 수 있다. 이러한 swap disk의 한 단위인 swap slot은 page의 크기와 동일한 
영역으로, 연속된 디스크 영역이라고 생각하면 된다. swap disk의 목적 자체가 physical 
memory에 저장된 page를 옮기거나, 다시 저장하는 역할로 수행되므로 이에 맞게 단위를 page 
size(4 KB)인 단위를 swap slot이라고 한다. 당연히 swap slot 또한, page-aligned 
되어야 하며, 이러한 slot을 관리할 때 사용하는 데이터를 바로 swap table이라 정의할 수 
있다. 따라서 swap table은 각 slot이 사용되고 있는지 여부를 저장하고 있어야 하고, 이후 
교체를 위해서 필요한 정보(최근 사용 여부)들을 함께 저장하여 표기해야 한다. 현재 pintos 
상에서는 struct block 만 구현되어 있는 상태로, 다른 연산들은 전혀 정의되어 있지 않은 
상황이다. 따라서 처음에 서술한 바와 같이 physical memory가 부족하게 된다면 더 이상 
실행할 수 없으므로 이를 해결해야 한다.

### Necessity
 swap table은 위에서 설명한 것처럼 swap slot을 관리하기 위한 table이다. 따라서 각 
slot 이 현재 사용되고 있는지, 혹은 비어있는지 여부 등을 기록하고 있어야 한다. 또한, 
physical memory 상의 page가 비워지는 경우라면, 해당 page를 저장하기 위한 비어있는
swap slot을 선택하여 기록할 수 있어야 하며, 반대로 저장된 것을 불러오는 경우거나 혹은
해당 swap slot을 사용하는 process가 종료된 경우에도 영역을 지워야 하는 것을 유념해야
한다. 또한, swap disk 라고 해서 전체 executable data를 미리 저장해놓고 있거나, 
혹은 할당할 slot을 사전에 정의해서는 안된다. 따라서 lazy allocation을 바탕으로 하여
eviction 시에(physical memory가 비워질 때)만 비어있는 slot을 찾아서 할당해야 한다.

 이러한 swap table을 관리하기 위해서는 크게 swap out, swap in, swap free가 
필요하다. 우선 swap out은 memory에서 page가 비워지며 swap slot에 저장되어야 할 때 
swap table을 통해서 비어있는 swap slot을 선택하고, 해당 slot에 `block_write()`을
통해서 page의 데이터를 입력해야 하고, 상태를 비어있지 않도록 업데이트를 해야 한다. 이 때
사용되는 것이 위에서 소개한 eviction policy 이며, 현재 physical memory 상에 
저장된 page 중에서 어떤 page를 선정하여 eviction 할 지를 정해야 한다. 이렇게 정해진 
page가 eviction 되어 swap disk 에 저장된다고 이해할 수 있다. 반대로 swap in 
시에는 대표적인 원인으로 page fault 발생 시에는 swap disk 에 저장되어 있는 page를 
physical memory로 다시 옮겨야 하므로, 이 때 새로운 page를 할당하고, 해당 page에 
`block_read()` 를 통해 읽어온 swap slot의 데이터를 옮겨줘야 한다. 또한, 기존 
데이터가 저장되어 있는 slot은 할당을 해제해줘야 한다. 마지막으로 swap free의 경우에는 
swap in처럼 데이터가 읽혀서 physical memory에 있는 경우거나 혹은 해당 slot을 
이용하는 process가 종료된 경우에도 사용하고 있던 모든 slot을 free 해줘야 한다. 

 또한, swap disk 는 실행되는 여러 process 사이에서 함께 공유되는 영역이다. 따라서 
swap table이 만약 각 쓰레드에 한정되어 지역적으로 선언될 경우 오히려 관리하는데 
더 많은 overhead가 발생할 수 있다. 따라서 전역적으로 vm/swap.c 에 선언하는 방식으로
관리한다면, 한 개의 bitmap을 통해 전체 disk 를 효율적으로 관리할 수 있을 것이다. 
대신에 이 경우 lock 을 통해서 해당 bitmap 에 접근 및 연산 수행 시 atomic 을 
보장해주는 것이 필요하게 될 것이다.

### Design Proposal
```C
/* From vm/swap.c */
struct hash st;
struct lock st_lock;
```

 swap table은 공식 문서에서 제안한 것처럼 비트맵을 기반으로 구현하고자 한다. 우선 
st는 처음 init.c 의 `main()` 에서 초기화가 수행되어야 할 것이다. 해시맵의 크기는
BLOCK_SWAP 역할을 가진 블록의 크기를 바탕으로 슬롯의 개수만큼 할당하고, lock을 
설정해주는 과정이 필요하게 된다. `swap_in()` 에서는 lock을 취득하고 실행하게 된다.
디스크에서 불러오고자 하는 spte를 입력받아야 하며, 입력받은 spte를 기반으로 
`block_read()`를 통해서 스왑 디스크에서 해당하는 슬롯을 읽어오게 된다. 이후 
사용된 스왑 슬롯을 st에서 해제하고, spte에 대응되는 vpage를 업데이트하는 것으로
실행을 마무리하게 될 것이다. 반대로 `swap_out()` 에서는 kpage만 입력받게 되며,
비어있는 스왑 슬롯을 st에서 할당받아서 block_write() 을 통해 해당 슬롯에
그 프레임을 입력하게 된다. `swap_out()` 의 경우 위의 프레임 테이블에서 방출 과정에서
선언되므로 반환된 스왑 슬롯의 id를 spte에 기록하게 되어 추후에 해당 spte를
바탕으로 다시 `swap_in()` 하게 된다. 마찬가지로 `swap_out()` 도 lock을 취득한
상황에서만 수행되어야 한다.

## On Process Termination
### Basic Descriptions and Limitations
 모든 실행이 끝나거나 혹은 문제가 발생하여 process 가 종료될 때에는 위에서 선언해준 모든
데이터를 할당 해제한 뒤에 종료해야 한다. 따라서 이를 위해서 위에서 선언한 frame table, 
supplementary page table, file memory mapping, swap table 모든 것들에 
대해서 할당을 해제해야 하며, lock 과 관련된 부분이나 혹은 열려있는 파일 등과 같이 유념하여 
할당을 해제해야 할 부분들이 존재하지만, 현재 구현에서는 userprog/process.c 에서 정의된 
`process_exit()`와 `destory_process()` 을 통해 수행되고 있으며, 현재 구현은 process에서
열려 있는 file pointer 들을 close 해주며, directory 들을 free 해주는 방식으로 구현되어 
있다. 하지만 이러한 방식으로는 앞서서 구현된 다양한 데이터들을 해제하지 못하며, swap slot 
들에 사용된 여러 데이터 또한 남아있는 채로 종료되게 된다. 따라서 해당 문제를 해결하고자 한다.

### Necessity and Design Proposal
 우선 on process termination 에 해당하는 구현은 userprog/process.c에 정의된,
`process_exit()` 함수 내에서 구현을 하고자 한다. 처음으로 Supplementary Page Table의 
경우에도 위와 유사하다. 이번에는 모든 할당된 Page를 해제해줘야 하는데, 이 경우 dirty 를 
확인하여 dirty 인 경우 디스크에 write 하고 그 이후애 해제해주는 방식으로 구현해야 한다.
Swap Table에서는 현재 process가 사용하고 있는 (혹은 대응되는) swap slot들을 모두 
찾아서 swap table에서 이를 해제하도록 변경해야 한다. File Memory Mapping 의 경우 
기존에는 `close_file()` 만 수행했으나, load 과정 중에 열렸던 파일을 포함하여 S-page 
table 에서 저장되어 있는 file 에 대한 정보도 함께 확인하여 모든 file 을 close 해줘야
하는 방식으로 구현하고자 한다. 또한, dirty 를 추가적으로 함께 확인하여 수정되었다면 
다시 write 해주는 과정이 필요하고, 그 이후 해당 memory mapping을 해제해주는 방식으로
현재 process가 가진 전체 file memory mapping 을 해제해주면 된다. 마지막으로 
해제해야 할 것은 frame table 인데, 이 경우 현재 종료되려고 하는 process가 
사용하고 있는 frame 자체를 해제하는 것이 필요하다. 따라서 현재 process 에 대응되는 
frame을 frame table에서 찾아서 이를 해제하고, 이 때 만약 해당 frame이 dirty 인 
상황이라면 (수정되었다면) 해당 frame 의 데이터를 디스크에 write 하는 방식으로 
저장해줘야 한다. 이후 frame을 해제하며 process 를 종료할 수 있게 된다. 그 과정 중에서 
추가로 확인해야 할 부분은 해당 프로세스가 Lock을 acquire 한 상태로 종료 시에 문제가 
발생할 수 있으므로, 이 경우에도 현재 종료하는 프로세스가 갖고 있는 lock을 저장하고 있다가,
마지막에 프로세스 종료 시 lock 또한 release 해줘야 한다.