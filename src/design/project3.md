# Project 3 - Design Report (Team 28)
이번 Project 3에서는 다음의 7가지 요구사항을 만족하여야 한다.

## Frame Table
<!-- 
  Requirement 문서에서 각 테이블이 각 스레드에 한정되어 지역적으로 선언되어야
  하는지 혹은 전역적으로, 모든 스레드가 공유하도록 선언해야 하는지를 결정해야
  한다고 합니다. 프레임 테이블이나 스왑 테이블 부분 서술하실 때 이 부분
  염두해 주세요.
-->
<!-- TODO: To be filled by Taeho. -->
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
<!-- TODO: To be filled by Taeho. -->
### Basic Descriptions
<!-- TODO: To be filled by Taeho. -->
### Limitations and Necessity
<!-- TODO: To be filled by Taeho. -->
### Design Proposal
<!-- TODO: To be filled by Taeho. -->

## File Memory Mapping
<!-- TODO: To be filled by Doyoung. -->
### Basic Descriptions
<!-- TODO: To be filled by Doyoung. -->
### Limitations and Necessity
<!-- TODO: To be filled by Doyoung. -->
### Design Proposal
<!-- TODO: To be filled by Doyoung. -->

## Swap Table
<!-- 
  Requirement 문서에서 각 테이블이 각 스레드에 한정되어 지역적으로 선언되어야
  하는지 혹은 전역적으로, 모든 스레드가 공유하도록 선언해야 하는지를 결정해야
  한다고 합니다. 프레임 테이블이나 스왑 테이블 부분 서술하실 때 이 부분
  염두해 주세요.
-->
<!-- TODO: To be filled by Taeho. -->
### Basic Descriptions
<!-- TODO: To be filled by Taeho. -->
### Limitations and Necessity
<!-- TODO: To be filled by Taeho. -->
### Design Proposal
<!-- TODO: To be filled by Taeho. -->

## On Process Termination
<!-- TODO: To be filled by Taeho. -->
### Basic Descriptions
<!-- TODO: To be filled by Taeho. -->
### Limitations and Necessity
<!-- TODO: To be filled by Taeho. -->
### Design Proposal
<!-- TODO: To be filled by Taeho. -->
