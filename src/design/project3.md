# Project 3 - Design Report (Team 28)
이번 Project 3에서는 다음의 7가지 요구사항을 만족하여야 한다.

## Frame Table
<!-- 
  Requirement 문서에서 각 테이블이 각 스레드에 한정되어 지역적으로 선언되어야
  하는지 혹은 전역적으로, 모든 스레드가 공유하도록 선언해야 하는지를 결정해야
  한다고 합니다. 프레임 테이블이나 스왑 테이블 부분 서술하실 때 이 부분
  염두해 주세요.
-->
<!-- 추가로 여기서 page pinned 도 설명해야 한다. -->
### Basic Descriptions
<!-- TODO: To be filled by Taeho. -->
### Limitations and Necessity
<!-- TODO: To be filled by Taeho. -->
### Design Proposal
<!-- TODO: To be filled by Taeho. -->

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
<!-- TODO: Page fault handler modification, modifications on SPTs. -->
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
    bool is_swapped;
    block_sector_t index;
    struct hash_elem elem;
  }
```
`spte` 구조체는 SPTE를 나타내기 위한 구조체이다. `struct spte`는 먼저, 해당 
SPTE에 대응되는 페이지가 현재 스왑 블록에 존재하는지 혹은 파일 시스템 블록에 
존재하는지를 나타내기 위한 `is_swapped` 멤버를 가지고 있다. 또, 해당 블록에서
어떤 섹터에 해당 페이지가 존재하는지를 나타내기 위한 멤버로 `index`를 가지고 
있으며, 해시 테이블에 해당 구조체를 추가하기 위한 원소로 `elem`을 가지고 있다.

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
       frame which the page would be loaded into.
      - If the physical page frame is full, evict a frame by LRU-approximating
        page replacement algorithm.
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

## Lazy Loading
<!-- TODO: To be filled by Doyoung. -->
### Basic Descriptions
<!-- TODO: To be filled by Doyoung. -->
### Limitations and Necessity
<!-- TODO: To be filled by Doyoung. -->
### Design Proposal
<!-- TODO: To be filled by Doyoung. -->

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
<!-- TODO: To be filled by Doyoung. -->
### Basic Descriptions
<!-- TODO: To be filled by Doyoung. -->
### Limitations and Necessity
<!-- TODO: To be filled by Doyoung. -->
### Design Proposal
<!-- TODO: To be filled by Doyoung. -->

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
 swap table은 공식 문서에서 제안한 것처럼 struct bitmap을 기반으로 구현하고자 한다.
vm/swap.c 에 새롭게 파일을 생성하고 해당 파일 내에 새로운 변수를 저장하고자 한다.
<!-- 구조를 선언해줘야 한다. 아직 구조를 자세히 설명을 안해놨다.-->

## On Process Termination
### Basic Descriptions and Limitations
 모든 실행이 끝나거나 혹은 문제가 발생하여 process 가 종료될 때에는 위에서 선언해준 모든
데이터를 할당 해제한 뒤에 종료해야 한다. 따라서 이를 위해서 위에서 선언한 frame table, 
supplementary page table, file memory mapping, swap table 모든 것들에 
대해서 할당을 해제해야 하며, lock 과 관련된 부분이나 혹은 열려있는 파일 등과 같이 유념하여 
할당을 해제해야 할 부분들이 존재하지만, 현재 구현에서는 userprog/process.c 에서 정의된 
process_exit()와 destory_process() 을 통해 수행되고 있으며, 현재 구현은 process에서
열려 있는 file pointer 들을 close 해주며, directory 들을 free 해주는 방식으로 구현되어 
있다. 하지만 이러한 방식으로는 앞서서 구현된 다양한 데이터들을 해제하지 못하며, swap slot 
들에 사용된 여러 데이터 또한 남아있는 채로 종료되게 된다. 따라서 해당 문제를 해결하고자 한다.

### Necessity and Design Proposal
 우선 on process termination 에 해당하는 구현은 userprog/process.c에 정의된,
process_exit() 함수 내에서 구현을 하고자 한다. 처음으로 Supplementary Page Table의 
경우에도 위와 유사하다. 이번에는 모든 할당된 Page를 해제해줘야 하는데, 이 경우 dirty 를 
확인하여 dirty 인 경우 디스크에 write 하고 그 이후애 해제해주는 방식으로 구현해야 한다.
Swap Table에서는 현재 process가 사용하고 있는 (혹은 대응되는) swap slot들을 모두 
찾아서 swap table에서 이를 해제하도록 변경해야 한다. File Memory Mapping 의 경우 
기존에는 close_file() 만 수행했으나, load 과정 중에 열렸던 파일을 포함하여 S-page 
table 에서 저장되어 있는 file 에 대한 정보도 함께 확인하여 모든 file 을 close 해줘야
하는 방식으로 구현하고자 한다. 또한, dirty 를 추가적으로 함께 확인하여 수정되었다면 
다시 write 해주는 과정이 필요하고, 그 이후 해당 memory mapping을 해제해주는 방식으로
현재 process가 가진 전체 file memory mapping 을 해제해주면 된다. 마지막으로 
해제해야 할 것은 frame table 인데, 이 경우 현재 종료되려고 하는 process가 
사용하고 있는 frame 자체를 해제하는 것이 필요하다. 따라서 현재 process 에 대응되는 
frame을 frame table에서 찾아서 이를 해제하고, 이 때 만약 해당 frame이 dirty 인 
상황이라면 (수정되었다면) 해당 frame 의 데이터를 디스크에 write 하는 방식으로 
저장해줘야 한다. 이후 frame을 해제하며 process 를 종료할 수 있게 된다.