# Project 2 - Design Report (Team 28)

## Project Description
Project 2는 크게 프로세스 종료 메세지 출력, 프로그램에 인자 전달, 시스템 호출, 
실행 파일에 대한 쓰기 거절의 네 가지 기능을 구현해야 한다.

### Process Termination Messages
일반적인 운영체제에서는 사용자 프로그램이 종료되면 그에 따른 종료 메세지를
프로세스의 반환값과 함께 출력한다. 이번 과제의 첫 번째 Task인 프로세스 종료
메세지 출력은 그와 같이, 현재 실행중인 프로세스가 종료되면 종료된 프로세스의
이름과 종료 코드(exit code)를 출력하도록 요구하고 있다.

### Argument Passing
프로그램을 실행할 때, 프로그램의 이름과 함께 명령행 인수(command line argument)
가 같이 전달되는 경우가 있다. 예를 들어, `cc -o hello hello.c`라는 명령어는
`cc`라는 프로그램을 실행하며 `cc`, `-o`, `hello`, `hello.c`라는 명령행 인수를
새로 만들어진, `cc`를 실행하는 프로세스에 전달한다. 이렇게 전달된 명령행 인수는
일반적인 C 프로그램의 경우 `main()`의 인수 `argv`에 전달되고 인수의 개수는 
`argc`에 전달되어, 프로그래머는 이러한 명령행 인수를 확인하여 프로그램의 동작을
적절하게 바꾸도록 프로그래밍하는 것이 일반적이다.

이러한 인수 전달 기능은 공짜로 제공되는 것이 아닌 운영체제의 추상화로 달성된다.
자세히 말하자면, 명령행 인수가 새로이 실행된 프로세스에 전달되기 위해서는 
명령행 인수를 whitespace 단위로 쪼개고(tokenize), 이들의 개수와 각 인수를 새로 
만들어진 프로세스의 스택에 넣어주어야 한다. Pintos 문서의 3.5.1 단락 Program 
Startup Details에서는 이러한 과정에 대해 자세히 서술하고 있다. 

### System Calls
많은 운영체제에서 사용자 프로그램은 기본적으로 사용자 모드에서 실행되며, 커널
권한이 필요한 명령어들은 사용자 모드에서 실행될 수 없다. 따라서, 만약 사용자
프로세스가 특정 커널 기능을 사용하고자 한다면 시스템 호출이라는 특별한 종류의 
예외(exception)를 발생시켜 커널에 해당 기능을 수행하도록 요청해야 한다. 즉,
시스템 호출은 사용자에게 공개된 커널에 대한 인터페이스 역할을 하여 사용자
프로세스가 커널 권한이 필요한 기능을 통제된 환경에서 수행할 수 있도록 한다.

시스템 호출은 사용자 프로그램에게는 하나의 함수 호출처럼 보여야 한다. 즉, 
사용자 프로세스는 시스템 호출과 함께 인자를 전달하고, 반환값이 있다면 반환값을
사용자 프로세스에서 사용할 수 있어야 하고, 시스템 호출이 끝나면 (시스템 호출이 
프로세스나 시스템을 종료시키지 않는다는 가정 하에) 시스템 호출의 다음 명령어에서 
프로세스 실행을 재개해야 한다. 이러한 추상화는 운영체제에 의해 제공된다. 즉, 
운영체제는 시스템 호출이 발생하면 인자를 시스템 호출 핸들러에 전달하고, 시스템 
호출 핸들러의 반환값을 다시 사용자 프로세스에 전달해야 한다.

이러한 시스템 호출에는 시스템의 실행을 중지하는 `halt()`, 프로세스의 실행을 자식
프로세스의 종료까지 잠시 멈추는 `wait()`, 실행 파일을 실행하는 `exec()` 등이 
있다. 

### Denying Write to Executables
만약 어떤 프로그램이 실행되는 도중에, 해당 실행 파일이 변경된다면 어떻게 될까? 
만약 실행 파일의 변경된 부분이 메모리에 이미 올라가 있고 이후 해당 페이지가 
메모리에서 다시 내보내지지(swap-out) 않는다면 프로그램은 변경되기 이전과 같이 
실행을 마칠 것이다. 만약 해당 변경된 부분이 메모리에 적재되어있지 않고, 새로
변경된 부분이 운좋게 현재 프로그램의 맥락과 일관적이라면 변경된 프로그램의
내용대로 실행될 수도 있을 것이다. 가장 최악의 경우, 실행 파일의 수정으로 인해
프로그램이 제대로 작동하지 않을 수도 있다. 즉, 프로그램 실행 도중 해당 파일의
변경은 예측할 수 없는 결과를 낳는다.

따라서, 많은 운영체제에서는 실행 파일이 실행되는 도중 해당 파일에 대한 쓰기를
제한한다. 예를 들어, Linux를 포함한 Unix 계열 운영체제에서는 파일이 실행되는 
도중 해당 파일에 쓰고자 하면 (일반적으로) ETXTBSY 오류를 발생시킨다. 이번 
프로젝트에서는 Pintos에서도 이러한 기능을 도입하여, 현재 실행중인 파일에 대한 
쓰기를 운영체제가 거부하도록 하는 것을 목표로 한다.

## Code Analysis

### System Call Handling Procedure in Pintos
```C
/* From lib/user/syscall.c */
pid_t
exec (const char *file)
{
  return (pid_t) syscall1 (SYS_EXEC, file);
}
```
이번 프로젝트에서는 `halt()` 부터 `close()`까지의 시스템 호출을 구현해야 한다.
이를 위해 먼저 Pintos에서 제공된 시스템 호출 함수들의 정의를 살펴보자. 시스템 
호출 함수들의 정의는 각기 크게 다르지 않으므로, Pintos의 시스템 호출 과정을 
보여주는 예시로 `exec()` 함수를 발췌하였다.

`exec()`의 정의를 보면, `syscall1()`이라는 매크로의 인자로 `SYS_EXEC`과 원래 
인자인 `file`을 넘기며 이를 호출하는 것을 볼 수 있다. 그리고 이의 반환값을
`pid_t` 형으로 형변환(casting)하여 다시 이를 반환하는 것을 볼 수 있다.

```C
/* From lib/syscall-nr.h */
/* System call numbers. */
enum 
  {
    /* Projects 2 and later. */
    SYS_HALT,                   /* Halt the operating system. */
    SYS_EXIT,                   /* Terminate this process. */
    SYS_EXEC,                   /* Start another process. */

    ...

    SYS_INUMBER                 /* Returns the inode number for a fd. */
  };
```
그렇다면, 이 매크로의 인자인 `SYS_EXEC`은 무엇일까? Pintos에서는 시스템 호출마다 
번호를 붙여 시스템 호출 핸들러가 시스템 호출의 종류를 알 수 있도록 한다. 이
상수, 혹은 열거형의 정의는 `lib/syscall-nr.h`에서 찾아볼 수 있으며, `SYS_EXEC`은 
`exec()` 시스템 호출에 해당하는 번호임을 알 수 있다.

```C
/* From lib/user/syscall.c */
/* Invokes syscall NUMBER, passing argument ARG0, and returns the
   return value as an `int'. */
#define syscall1(NUMBER, ARG0)                                           \
        ({                                                               \
          int retval;                                                    \
          asm volatile                                                   \
            ("pushl %[arg0]; pushl %[number]; int $0x30; addl $8, %%esp" \
               : "=a" (retval)                                           \
               : [number] "i" (NUMBER),                                  \
                 [arg0] "g" (ARG0)                                       \
               : "memory");                                              \
          retval;                                                        \
        })
```
`syscall1()` 매크로의 인자로 넘어간 `file`은 `exec()`으로 실행할 파일의 경로를 
나타내는 문자열을 가리키는 포인터일 것이다. 그렇다면, `syscall1()` 매크로는 과연 
무슨 역할을 할까? `lib/user/syscall.c`에는 `syscall1()` 말고도 `syscall0()`, 
`syscall2()`, `syscall3()` 또한 정의되어 있지만, 이들은 인자의 개수를 제외하고는 
`syscall1()`과 동일하므로 생략하였다.

`syscall1()`은 인자 `ARG0`와 `NUMBER`를 현재 스택에 push하고, 인터럽트 번호 
0x30에 해당하는 인터럽트를 발생시킨 후, esp 레지스터에 8을 더하는 동작을 함을
알 수 있다. 이후 `retval`을 반환하는데, 인라인 어셈블리의 출력 피연산자 목록(
`"=a" (retval)`)을 보면 `retval`에는 eax 레지스터의 값이 저장됨을 알 수 있다. 

여기까지 알아낸 바에 의해 `exec()` 시스템 호출이 발생하였을 때 Pintos에서 
일어나는 일을 정리해 보자면 다음과 같다. 사용자 프로세스에서 `exec()` 호출이
발생하면, `exec()`에서는 다시 `syscall1()` 매크로에 `SYS_EXEC`과 `file` 인자를
전달하며 이를 호출한다. `syscall1()`은 스택에 `SYS_EXEC`과 `file`을 push하고,
0x30 인터럽트를 발생시키며, 인터럽트 핸들러 실행이 종료되면 두 인자를 버리고 이 
시점의 eax 레지스터의 값을 반환한다.

```C
/* From userprog/syscall.c */
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}
```
0x30 인터럽트의 핸들러는, Pintos 초기화 과정 중 호출되는 `syscall_init()`에 
따르면, `syscall_handler()`이다.

```C
/* From userprog/syscall.c */
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit ();
}
```
`syscall_handler()`는 상술한 바와 같이 시스템 호출이 발생되었을 때 최종적으로
실행되는 함수로, 0x30 인터럽트의 핸들러 역할을 한다. `syscall_handler()`의 
정의를 살펴보면, 현재 구현은 단지 `system call!`이라는 메세지만을 출력하고
스레드를 종료시킨다는 것을 알 수 있다.

아직 분석은 끝나지 않았다. 그렇다면, `syscall_handler()`의 인자인 `f`에는 어떤
정보가 담겨있을까?

```C
/* From threads/interrupt.h */
/* Interrupt stack frame. */
struct intr_frame
  {
    ...

    /* Pushed by the CPU.
       These are the interrupted task's saved registers. */
    void (*eip) (void);         /* Next instruction to execute. */
    uint16_t cs, :16;           /* Code segment for eip. */
    uint32_t eflags;            /* Saved CPU flags. */
    void *esp;                  /* Saved stack pointer. */
    uint16_t ss, :16;           /* Data segment for esp. */
  };
```
<!-- TODO: intr-stubs.S 및 interrupt.c 발췌하여 서술 -->
Project 1 design report에서 서술한 바와 같이, IA32 아키텍처에서 `int` 명령어로
인터럽트가 발생하면, 프로세서는 IDTR이 가리키는 IDT를 참조해 해당하는 ISR을 
호출한다. 또한, 프로세서는 IDT 게이트의 GDT selector를 cs 레지스터에 load한다.
cs 레지스터의 최하위 2비트는 프로세서의 특권 수준(privilege level)을 나타내므로, 
이 과정에서 프로세서의 실행 모드가 사용자 모드에서 OS/hypervisor 모드로 바뀌게 
된다.

Pintos에서는 `threads/interrupt.c`의 `intr_init()`에서 IDTR을 IDT를 
가리키도록 초기화한다. 각 IDT에는 `intr_stubs.S`에 정의된 `intrNN_stub`(`NN`은 
`00`부터 `FF`까지의 16진수)가 등록되어 있어, 인터럽트 발생시 해당 루틴으로 실행 
흐름을 바꾼다. 이후 `intrNN_stub`은 `intr_entry`로 점프하고, `intr_entry`는 
`intr_handler()`를 호출하며, `intr_handler()`는 `intr_register_int()`에 의해
등록된 인터럽트 핸들러를 호출한다. 이 경우에는 `syscall_handler()`가 인터럽트
번호 0x30에 대한 인터럽트 핸들러가 된다.

`f`의 타입인 `struct intr_frame`을 보면, CPU에 의해 인터럽트가 발생한 프로세스의
스택 포인터, 즉 esp 레지스터가 커널 스택 프레임에 저장됨을 알 수 있다. 즉, 
앞서 설명한 `syscall1()`의 인자 `NUMBER`와 `ARG0`는 인터럽트가 발생한 프로세스의
스택의 맨 위에 저장되어 있으므로, 커널에서는 저장된 스택 포인터로 `NUMBER`와 
`ARG0`를 참조할 수 있을 것이다. 좀 더 자세히 설명하자면, `syscall_handler()`에서
`((int *) f->esp)[0]`은 `NUMBER`의 값이 될 것이고, 이를 통해 시스템 호출의
종류를 결정한 후 시스템 호출 인자의 타입에 따라 `((t *) f->esp)[1]`과 같이 
`ARG0`의 값도 참조할 수 있을 것이다.

### Exception Handling Procedure in Pintos
프로그램이 실행되면서 프로그램은 정적으로 예측할 수 없는 다양한 예외적 상황에
놓이게 된다. 예를 들어, 가장 쉽게 발생시킬 수 있는 종류의 예외로는 '0으로 
나누기' 예외가 있으며, 유효하지 않은 명령어 코드(opcode)를 실행하려 할 때에도 
예외가 발생한다. 또한, 페이지 디렉터리 혹은 페이지 테이블에 해당 VPN(Virtual
Page Number)에 해당하는 PDE(Page Directory Entry) 혹은 PTE(Page Table Entry)가
존재하지 않는 경우에도 CPU는 페이지 폴트 예외를 발생시켜 운영체제로 하여금 이를
처리하도록 요구한다.

```C
/* From userprog/exception.c */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  ...
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}
```
이러한 예외는 CPU에 의해 직접 발생하는 경우가 있다는 점을 제외하고는 상술한
인터럽트의 처리 과정과 거의 동일하게 처리되므로 어떻게 예외 발생 시 예외 처리
핸들러가 호출되는지에 대한 설명은 생략한다. `int 0x30`으로 인해 0x30 인터럽트가
발생하여 시스템 호출 핸들러 `syscall_handler()`가 호출되는 것과 같이, 0으로 
나누기 예외가 발생하였을 때에도 CPU는 `intr00_stub`과 `intr_entry`를 통해
결국, 위 코드에 따르면, `kill()`을 호출하게 된다.

한 가지 눈여겨볼 점은 예외에 따라 `intr_register_int()`의 두 번째 인자인 `dpl`이
달라진다는 점이다. `dpl`은 해당 예외를 `int` 명령어로 직접 발생시키기 위해 
필요한 최소한의 특권 수준(privilege level)을 의미한다. IA32 아키텍처에는 4개의 
특권 수준이 있지만 보통은 OS/hypervisor 수준을 의미하는 0과 사용자 수준을 
의미하는 3의 두 가지만 쓰인다. 만약 `intr_register_int()`가 `dpl`을 3으로 하여
호출되었다면 해당 예외는 `int`, `int3`, `int0` 혹은 `bound` 명령어를 통해 사용자
프로그램 스스로 발생시킬 수 있다. 하지만, 만약 `dpl`이 0이라면, 해당 예외는
사용자가 직접 발생시킬 수 없으며 오직 OS/hypervisor 수준에서 발생시키거나 해당
예외 조건이 만족되어야 발생한다.

Pintos에서는 `userprog/exception.c`의 `exception_init()`를 초기화 과정 중 
호출하여 이러한 예외 처리 핸들러를 등록한다. 현재 Pintos의 구현을 살펴보면,
거의 모든 종류의 예외에 대해 `kill()`을 예외 처리 핸들러로 등록하여 예외 발생
시 해당 프로세스를 종료하도록 구현되어 있는 모습을 볼 수 있다.

```C
/* From userprog/exception.c */
/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit (); 

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}
```
`kill()`의 동작을 보면, 인터럽트 프레임에 저장된 cs 레지스터의 값을 확인하여 
예외가 발생한 프로세스의 코드 세그먼트가 사용자 코드 세그먼트(`SEL_UCSEG`)인지,
혹은 커널 코드 세그먼트(`SEL_KCSEG`)인지 확인 후 만약 사용자 코드 세그먼트에서
해당 예외가 발생하였다면 해당 프로세스를 종료한다. 만약 커널 코드 세그먼트에서
해당 예외가 발생하였다면 커널의 버그이므로 커널 패닉을 일으킨다.

페이지 폴트 단 하나의 예외를 제외하면 Pintos의 현재 구현은 모든 예외에 대해
`kill()`이 이를 처리하도록 하고 있다. 페이지 폴트 예외와 페이지 폴트 예외 처리
핸들러에 대해서는 다음 단락에서 더 자세히 살펴볼 것이다.

### IA32 Paging and Address Translation
사용자 메모리 접근과 페이지 폴트 처리에 대해 살펴보기 전에, IA32 아키텍처에서
가상 주소(virtual address)가 실제 물리적 주소로 어떻게 번역되며, Pintos에서는
이러한 번역 과정이 어떻게 처리되는지 알아보자.

IA32 아키텍처에서는 페이지 디렉터리(page directory)와 페이지 
테이블(page table)의 두 계층으로 이루어진 자료구조를 통해 가상 주소를 번역한다.
먼저, 32비트 가상 주소는 다음의 세 비트 필드로 나누어진다.

|       31~22        |      21~12     |    11~0   |
|:------------------:|:--------------:|:---------:|
|Page Directory Index|Page Table Index|Page Offset|

이 중 페이지 디렉터리 인덱스는 페이지 디렉터리를 참조하는데 사용되며, 페이지
디렉터리에 저장된 PDE에는 각각의 페이지 테이블의 주소가 저장된다. 이렇게 PDE를
통해 페이지 테이블을 찾으면, 페이지 테이블 인덱스를 통해 다시 한번 해당 PTE를
참조하며, 이때 PTE에 있는 물리적 주소와 페이지 오프셋을 더한 것이 실제 물리적
주소가 된다. PTE는 하나당 32비트이며, 따라서 20비트의 물리적 주소를 제외한 PTE의
나머지 비트 필드에는 수정 여부(dirty), 접근 여부(accessed), 존재 여부(present),
접근 권한(user/supervisor), 쓰기 가능(writable) 등의 정보가 저장된다. 

그렇다면, 이러한 페이지 디렉터리는 어떻게 프로세서에 의해 참조될까? IA32 
아키텍처 프로세서는 cr0부터 cr8의 컨트롤 레지스터를 가지고 있다. 이 중, cr3
레지스터는 현재 가상 주소 번역에 사용되는 페이지 디렉터리의 주소를 담고 있다.
즉, 가상 주소를 통한 메모리 접근 요청이 들어오면 프로세서는 cr3 레지스터가 
가리키는 페이지 디렉터리에서 (PDE에 담긴) 페이지 테이블의 주소를 찾고, 페이지
테이블에서 다시 해당하는 물리적 주소를 찾아 이를 오프셋과 더해 물리적 주소를
생성한다. 만약 이 과정에서 해당 페이지 테이블 혹은 페이지가 존재하지 않는다면,
즉, 존재 여부를 나타내는 비트(present)가 0이라면 프로세서는 페이지 폴트를 
발생시켜 운영체제로 하여금 이를 처리하도록 할 것이다.

IA32 아키텍처에서는 운영체제에 직접 물리적 주소로 메모리에 접근할 방법을
제공하지 않는다. 따라서, Pintos에서는 `PHYS_BASE` 위의 가상 주소 공간을 
선형적으로 물리적 주소에 대응시키는 방법을 사용한다. 정확히는, 물리적 주소 
`paddr`에 대해 대응되는 커널 영역 가상 주소는 `paddr + PHYS_BASE`가 된다. 
Pintos는 `threads/init.c`의 `paging_init()`을 통해, 해당 영역의 가상 주소가 
물리적 주소에 바로 대응될 수 있도록 페이지 디렉터리와 페이지 테이블을 
초기화한다.

```C
/* From init.c */
static void
paging_init (void)
{
  uint32_t *pd, *pt;
  size_t page;
  extern char _start, _end_kernel_text;

  pd = init_page_dir = palloc_get_page (PAL_ASSERT | PAL_ZERO);
  pt = NULL;
  for (page = 0; page < init_ram_pages; page++)
    {
      uintptr_t paddr = page * PGSIZE;
      char *vaddr = ptov (paddr);
      size_t pde_idx = pd_no (vaddr);
      size_t pte_idx = pt_no (vaddr);
      bool in_kernel_text = &_start <= vaddr && vaddr < &_end_kernel_text;

      if (pd[pde_idx] == 0)
        {
          pt = palloc_get_page (PAL_ASSERT | PAL_ZERO);
          pd[pde_idx] = pde_create (pt);
        }

      pt[pte_idx] = pte_create_kernel (vaddr, !in_kernel_text);
    }

  /* Store the physical address of the page directory into CR3
     aka PDBR (page directory base register).  This activates our
     new page tables immediately.  See [IA32-v2a] "MOV--Move
     to/from Control Registers" and [IA32-v3a] 3.7.5 "Base Address
     of the Page Directory". */
  asm volatile ("movl %0, %%cr3" : : "r" (vtop (init_page_dir)));
}
```
`paging_init()`의 구현을 보면, 먼저 페이지 디렉터리로 사용될 페이지를 
할당받는다. 이후, 물리적 메모리의 각 페이지마다 해당하는 물리 주소를 구하고,
이를 해당하는 가상 주소로 변환한다. 이후, 만약 해당 페이지에 해당하는 PTE가 
저장될 페이지 테이블이 없다면 이를 할당받고, 페이지 테이블을 가리키는 PDE를 
페이지 디렉터리에 등록한다. 이를 커널 영역의 각 메모리 페이지마다 반복하고,
이후 초기화된 페이지 디렉터리를 가리키는 `init_page_dir`을 cr3 레지스터에 
저장하여 이후의 가상 주소 번역이 해당 페이지 디렉터리를 통해 이루어지도록 한다.

```C
/* From threads/vaddr.h */
/* Offset within a page. */
static inline unsigned pg_ofs (const void *va) {
  return (uintptr_t) va & PGMASK;
}

/* Virtual page number. */
static inline uintptr_t pg_no (const void *va) {
  return (uintptr_t) va >> PGBITS;
}

/* Round up to nearest page boundary. */
static inline void *pg_round_up (const void *va) {
  return (void *) (((uintptr_t) va + PGSIZE - 1) & ~PGMASK);
}

/* Round down to nearest page boundary. */
static inline void *pg_round_down (const void *va) {
  return (void *) ((uintptr_t) va & ~PGMASK);
}
```
`threads/vaddr.h`에서는 가상 주소에 대해 페이지 오프셋, 페이지 번호, 페이지 
시작 주소로의 반올림/내림 등 다양한 연산을 할 수 있는 보조 함수를 제공한다. 
또한, 인자 `vaddr`을 `PHYS_BASE`와 비교하여 `vaddr`이 사용자 가상 주소 영역에 
포함되는지 혹은 커널 가상 주소 영역에 포함되는지를 반환하는 `is_user_vaddr()`, 
`is_kernel_vaddr()` (Pintos에서는 `PHYS_BASE` 이상의 가상 주소는 커널 영역,
`PHYS_BASE` 아래의 가상 주소는 사용자 영역으로 간주된다는 점을 기억하자), 물리 
주소 `paddr`을 커널 가상 주소로 변환하는 `ptov()`, 반대로 가상 주소 `vaddr`을
물리 주소로 변환하는 `vtop()` 함수를 제공하며, 이들은 동작이 크게 복잡하지 않은 
보조 함수이기 때문에 보고서에는 포함하지 않았다.

```C
/* From threads/pte.h */
/* Obtains page table index from a virtual address. */
static inline unsigned pt_no (const void *va) {
  return ((uintptr_t) va & PTMASK) >> PTSHIFT;
}

/* Obtains page directory index from a virtual address. */
static inline uintptr_t pd_no (const void *va) {
  return (uintptr_t) va >> PDSHIFT;
}

...

/* Returns a PDE that points to page table PT. */
static inline uint32_t pde_create (uint32_t *pt) {
  ASSERT (pg_ofs (pt) == 0);
  return vtop (pt) | PTE_U | PTE_P | PTE_W;
}

/* Returns a pointer to the page table that page directory entry
   PDE, which must "present", points to. */
static inline uint32_t *pde_get_pt (uint32_t pde) {
  ASSERT (pde & PTE_P);
  return ptov (pde & PTE_ADDR);
}

/* Returns a PTE that points to PAGE.
   The PTE's page is readable.
   If WRITABLE is true then it will be writable as well.
   The page will be usable only by ring 0 code (the kernel). */
static inline uint32_t pte_create_kernel (void *page, bool writable) {
  ASSERT (pg_ofs (page) == 0);
  return vtop (page) | PTE_P | (writable ? PTE_W : 0);
}

/* Returns a PTE that points to PAGE.
   The PTE's page is readable.
   If WRITABLE is true then it will be writable as well.
   The page will be usable by both user and kernel code. */
static inline uint32_t pte_create_user (void *page, bool writable) {
  return pte_create_kernel (page, writable) | PTE_U;
}

/* Returns a pointer to the page that page table entry PTE points
   to. */
static inline void *pte_get_page (uint32_t pte) {
  return ptov (pte & PTE_ADDR);
}
```

|              31 ~ 12             |11 ~ 9| 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|:--------------------------------:|:----:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
|Page Table Address or Page Address|  AVL | G |PAT| D | A |PCD|PWT|U/S|R/W| P |

위는 전술한 PDE와 PTE의 비트 필드이다. 

`threads/pte.h`에서는 가상 주소에서 페이지 디렉터리 인덱스, 페이지 테이블 
인덱스를 추출하는 `pd_no()`, `pt_no()`, PDE와 PTE를 위의 비트 필드 형식에
맞춰 생성하는 `pde_create()`, `pte_create_user()`, `pte_create_kernel()` 
함수를 제공한다. 이때, 사용자 영역 페이지를 가리키는 PTE의 접근 권한 비트는
1로 설정되어야 하기 때문에 `pte_create_user()`와 `pte_create_kernel()`이 따로
정의되었다. 마지막으로 PDE에서 페이지 테이블 주소를 추출하는 `pde_get_pt()`와
PTE에서 페이지 주소를 추출하는 `pte_get_page()`가 정의되어 있다.

### Page Directory Abstraction in Pintos
Pintos에서는 하드웨어 수준의 페이지 테이블 조작을 추상화하기 위해 
`userprog/pagedir.c`에 정의된 인터페이스 함수를 제공한다. `pagedir.c`의 
인터페이스 외부에서 프로그래머는 더이상 PTE 혹은 PDE와 직접 상호작용하지 않으며, 
단지 가상 주소에서 물리 주소로의 대응을 담는 '페이지 디렉터리'라는 객체에 대한 
추상적 상호작용만을 이용하게 된다.

```C
/* From userprog/pagedir.c */
/* Creates a new page directory that has mappings for kernel
   virtual addresses, but none for user virtual addresses.
   Returns the new page directory, or a null pointer if memory
   allocation fails. */
uint32_t *
pagedir_create (void) 
{
  uint32_t *pd = palloc_get_page (0);
  if (pd != NULL)
    memcpy (pd, init_page_dir, PGSIZE);
  return pd;
}
```
`pagedir_create()`는 새로운 페이지 디렉터리를 생성하여 그 페이지 디렉터리의
주소를 반환하는 함수이다. 이때, 새롭게 생성된 페이지 디렉터리 또한 `PHYS_BASE`
위의 커널 영역 가상 주소에 대한 대응은 `init_page_dir`과 같아야 한다. 따라서,
`pagedir_create()`는 먼저 페이지를 할당받은 후 커널 영역에 대해서는 
`init_page_dir`을 복사하는 식으로 동작한다.

```C
/* From userprog/pagedir.c */
/* Destroys page directory PD, freeing all the pages it
   references. */
void
pagedir_destroy (uint32_t *pd) 
{
  uint32_t *pde;

  if (pd == NULL)
    return;

  ASSERT (pd != init_page_dir);
  for (pde = pd; pde < pd + pd_no (PHYS_BASE); pde++)
    if (*pde & PTE_P) 
      {
        uint32_t *pt = pde_get_pt (*pde);
        uint32_t *pte;
        
        for (pte = pt; pte < pt + PGSIZE / sizeof *pte; pte++)
          if (*pte & PTE_P) 
            palloc_free_page (pte_get_page (*pte));
        palloc_free_page (pt);
      }
  palloc_free_page (pd);
}
```
위 `pagedir_create()`가 페이지 디렉터리 객체의 생성자라면, `pagedir_destroy()`는
페이지 디렉터리 객체의 소멸자로 기능하는 함수이다. 인자 `pd`의 PDE를 하나씩 
순회하며, 만약 PDE가 현재 존재한다면, 해당 PDE가 가리키는 페이지 테이블에 속한
페이지를 하나씩 free시킨다. 이때 또한, 커널 가상 주소 영역의 페이지는 그대로
유지되어야 하므로 `pd`에서 `pd + pd_no (PHYS_BASE)`, 즉 해당 페이지 디렉터리 중
사용자 영역에 해당하는 페이지만을 free시킨다. 이후 마지막으로 페이지 
디렉터리까지 free시키며 함수 실행은 끝난다.

```C
/* From userprog/pagedir.c */
/* Returns the address of the page table entry for virtual
   address VADDR in page directory PD.
   If PD does not have a page table for VADDR, behavior depends
   on CREATE.  If CREATE is true, then a new page table is
   created and a pointer into it is returned.  Otherwise, a null
   pointer is returned. */
static uint32_t *
lookup_page (uint32_t *pd, const void *vaddr, bool create)
{
  ...

  /* Check for a page table for VADDR.
     If one is missing, create one if requested. */
  pde = pd + pd_no (vaddr);
  if (*pde == 0) 
    {
      if (create)
        {
          pt = palloc_get_page (PAL_ZERO);
          if (pt == NULL) 
            return NULL; 
      
          *pde = pde_create (pt);
        }
      else
        return NULL;
    }

  /* Return the page table entry. */
  pt = pde_get_pt (*pde);
  return &pt[pt_no (vaddr)];
}
```
`lookup_page()`는 인자 `vaddr`에 해당하는 PTE를 페이지 디렉터리에서 찾아 이를 
반환하는 함수이다. `lookup_page()`는 먼저 `pd`에서 `vaddr`에 해당하는 PDE를 
찾는다. 이때, 만약 해당 PDE가 `pd`에 존재하지 않는다면, `create` 플래그에 따라
동작이 달라진다. 만약 `create` 플래그가 참이라면 해당 `vaddr`에 대응되는 새로운
페이지 테이블을 만들어 그를 `pd`에 등록하고 해당 PTE를 반환한다. 만약 `create`
플래그가 거짓이라면 새로운 페이지 테이블을 만들지 않고 NULL 포인터를 반환한다.

```C
/* From userprog/pagedir.c */
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
bool
pagedir_set_page (uint32_t *pd, void *upage, void *kpage, bool writable)
{

  ...

  pte = lookup_page (pd, upage, true);

  if (pte != NULL) 
    {
      ASSERT ((*pte & PTE_P) == 0);
      *pte = pte_create_user (kpage, writable);
      return true;
    }
  else
    return false;
}
```
`pagedir_set_page()`는 `pd`에 `upage`에서 `kpage`로의 대응을 추가하는 함수이다.
만약 `pd`에 `upage`에 해당하는 페이지 테이블이 존재하지 않는 상태에서 새로운 
페이지 테이블을 위한 페이지를 할당받는데 실패하면 거짓을, 해당 대응을 추가하는
데 성공하면 참을 반환한다. 

```C
/* From userprog/pagedir.c */
/* Looks up the physical address that corresponds to user virtual
   address UADDR in PD.  Returns the kernel virtual address
   corresponding to that physical address, or a null pointer if
   UADDR is unmapped. */
void *
pagedir_get_page (uint32_t *pd, const void *uaddr) 
{
  uint32_t *pte;

  ASSERT (is_user_vaddr (uaddr));
  
  pte = lookup_page (pd, uaddr, false);
  if (pte != NULL && (*pte & PTE_P) != 0)
    return pte_get_page (*pte) + pg_ofs (uaddr);
  else
    return NULL;
}
```
`pagedir_get_page()`는 `pd`에서 `uaddr`에 해당하는 물리 주소를 찾아 반환하는
함수이다. 만약 `uaddr`로부터 어떤 물리 주소에 해당하는 대응이 `pd`에 존재한다면
`uaddr`을 물리 주소로 번역한 결과를 반환하며, 그러한 대응이 존재하지 않는다면
NULL 포인터를 반환한다.

```C
/* From userprog/pagedir.c */
/* Marks user virtual page UPAGE "not present" in page
   directory PD.  Later accesses to the page will fault.  Other
   bits in the page table entry are preserved.
   UPAGE need not be mapped. */
void
pagedir_clear_page (uint32_t *pd, void *upage) 
{
  uint32_t *pte;

  ASSERT (pg_ofs (upage) == 0);
  ASSERT (is_user_vaddr (upage));

  pte = lookup_page (pd, upage, false);
  if (pte != NULL && (*pte & PTE_P) != 0)
    {
      *pte &= ~PTE_P;
      invalidate_pagedir (pd);
    }
}
```
`pagedir_clear_page()`는 `pd`에서 `upage`를 삭제하는 함수이다. 이때 해당 
페이지는 할당 해제되는것이 아닌, 단지 `pd`에서 `upage`에서 해당 페이지로의 
대응이 삭제되는것임에 주의하자. 따라서, `pagedir_clear_page()`에서는 `upage`에
대응되는 PTE의 존재 여부를 나타내는 비트만 0으로 설정하며, 이후 TLB가 이전의
대응을 참조하여 오작동하지 않도록 `invalidate_pagedir()`을 호출하여 TLB를 
초기화한다. 

```C
/* From userprog/pagedir.c */
/* Loads page directory PD into the CPU's page directory base
   register. */
void
pagedir_activate (uint32_t *pd) 
{
  if (pd == NULL)
    pd = init_page_dir;

  /* Store the physical address of the page directory into CR3
     aka PDBR (page directory base register).  This activates our
     new page tables immediately.  See [IA32-v2a] "MOV--Move
     to/from Control Registers" and [IA32-v3a] 3.7.5 "Base
     Address of the Page Directory". */
  asm volatile ("movl %0, %%cr3" : : "r" (vtop (pd)) : "memory");
}
```
`pagedir_activate()`는 페이지 디렉터리 `pd`를 활성화하여, 이후 프로세서가 가상
주소를 번역할 때 해당 페이지 디렉터리를 참조하도록 한다. 동작을 자세히 살펴보면,
`pd`를 물리 주소로 번역하여 `movl` 명령어를 통해 해당 물리 주소를 cr3 레지스터에
적재하는 것을 볼 수 있다.

이외의 함수들은 `pagedir.c` 바깥으로 노출되지 않는 정적 함수이거나 동작이 지극히
단순하여 본 보고서에서 설명할 필요가 없으므로 서술을 생략한다.

### User Memory Access and Handling Page Fault
사용자의 시스템 호출을 처리하기 위해 커널은 종종 사용자가 제공한 주소를 
역참조(dereference)하여야 한다. 예를 들어서, 위 단락에서 서술한 바와 같이, 
사용자 프로그램이 시스템 호출을 발생시켰을 때, 커널은 사용자 프로세스의 해당 
시점의 esp 레지스터를 역참조하여 시스템 호출의 종류와 시스템 호출의 인자를 
알아내야 한다. 

문제가 되는 점은 이러한 역참조 시 사용자가 전달한 주소, 혹은 포인터는 신뢰할 수
없다는 점이다. 사용자는 시스템 호출 이전에 esp 레지스터를 조작하여 커널로 하여금
잘못된 값을 읽거나 수정하도록 만들 수 있다. 만약 esp 레지스터가 커널 영역을 
가리킨다면, 즉, esp가 `PHYS_BASE` 위를 가리키고 있다면 사용자 프로세스는 시스템
호출을 통해 본디 불가능해야 할 커널 영역 데이터를 읽거나 수정할 수 있게 된다.

따라서, 운영체제에서는 시스템 호출을 처리하기에 앞서서 사용자 프로세스에서 
전달된 포인터, 레지스터, 혹은 주소 값이 유효한지를 검증하여야 한다. Pintos 
문서에 따르면, 두 가지 접근이 가능하다. 첫 번째 접근법은 사용자 주소를 
역참조하기 이전에 이의 유효성을 검증하는 것이다. 이를 위해서는 먼저
`vaddr.h`의 `is_usesr_vaddr()`을 이용하여 해당 주소가 사용자 영역 가상 주소임을
검증한 후, `pagedir_get_page()`의 반환값을 확인하여 현재 프로세스의 페이지
디렉터리에 해당 주소에 대한 대응이 존재하는지를 확인하면 될 것이다.

하지만, 이러한 방식은 기본적으로 느리다. 그 이유는 다음과 같은데, 현대 
프로세서에는 MMU(Memory Management Unit)라는 주소 번역만을 담당하는 부분이 
존재한다. MMU는 전술한 페이지 디렉터리와 페이지 테이블을 통해 주소를 번역하는
작업을 전담하여 프로세서의 ALU가 실제 연산에 쓰일 수 있도록 돕는다. 첫 번째 
방법은 이러한 MMU를 이용하지 않고 프로세서가 메모리 번역 작업을 담당하게 되므로
느리다.

따라서, 일반적인 운영체제에서 주로 쓰이는 방식은 사용자가 전달한 포인터가
사용자 영역에 있는지만을 검증하고 해당 포인터를 바로 역참조하는 방식이다.
사용자 포인터가 사용자 영역에 있는지를 검증하는 작업은 컴파일러의 최적화에 따라 
하나의 명령어로 완료될 수도 있으므로 경제적이다. 그렇다면, 이 방식을 사용할
경우 사용자가 해당 프로세스의 페이지 디렉터리에 존재하지 않는 페이지에 대한
주소를 전달한다면 커널은 이를 어떻게 처리해야 할까?

프로세서는 현재 cr3 레지스터가 가리키는 페이지 디렉터리에 존재하지 않는 가상
주소를 역참조하고자 하면 페이지 폴트 예외를 발생시킨다. 즉, 두 번째 방식을
채택한 경우에는 페이지 폴트 예외 처리 핸들러에서 해당 상황을 처리해야 한다.
페이지 폴트 예외 처리 핸들러의 등록은 위 단락에서 서술하였다.

```C
/* From userprog/exceptioin.c */
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
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

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
페이지 폴트가 발생하면, 프로세서는 cr2 레지스터에 페이지 폴트를 유발한 가상 
주소를 저장하고, 커널 스택에 페이지 폴트의 원인에 따른 오류 코드를 push한 후,
위에서 설명한 예외 처리 과정에 따라 `page_fault()`로 실행 흐름을 옮긴다. 
`page_fault()`의 현재 구현에서는 페이지 폴트의 원인을 나타내는 불리언 변수를
오류 코드에서 추출한 값으로 설정하고 `falut_addr`에 cr2 레지스터에 저장된
가상 주소를 옮기지만, 정작 중요한 페이지 폴트 해결 과정이 구현되어 있지 않고
그저 페이지 폴트를 발생시킨 프로세스를 종료시키기만 한다.

만약 사용자 메모리 영역에 접근하는데 두 번째 방식을 취한다면, `page_fault()`
함수를 수정하여 만약 사용자가 제공한 포인터가 유효하지 않아 페이지 폴트가 
발생한 경우 이를 처리하도록 해야 할 것이다. 두 접근법 모두, 만약 사용자 
프로세스가 유효하지 않은 주소를 전달하였더라도 메모리 누수 등의 자원 낭비 없이
프로세스를 종료시켜야 한다.

### File System of Pintos
이번 프로젝트에서 구현해야 하는 몇몇 시스템 호출은 파일에 수정하거나 생성하고
제거하며 여는 등의, 파일 시스템과 상호작용하는 작업을 포함한다. 따라서, 이들을
구현하기에 앞서서 Pintos에서 제공하는 파일 시스템 인터페이스를 살펴보자.

Pintos에서 파일, 다시 말해 `struct file`은 `inode`와 파일의 현재 위치를 나타내는
`pos`으로 이루어져 있다. `pos`는 단지 파일에 읽거나 쓸 위치를 나타내므로,
실제 파일에 대한 작업은 `inode`를 통해 이루어진다고 볼 수 있다. 그렇다면, 
`inode`는 어떤 구조체, 혹은 자료구조일까?

```C
/* From filesys/inode.c */
/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[125];               /* Not used. */
  };

...

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };
```
`filesys/inode.c`의 정의를 보면 `struct inode`는 현재 블록에서 섹터 위치를
나타내는 `sector`와 실제 디스크 상에서 `inode`를 나타내는 `inode_disk` 형 
`data`의 두 원소와 함께 여러 메타데이터를 가지고 있음을 알 수 있다. 또한,
`struct inode_disk`는 다시 디스크 상에서 처음 섹터를 나타내는 `start`와
해당 `inode_disk`의 크기를 나타내는 `length`, 그리고 `inode_disk`의 크기를
한 섹터 (512B) 크기로 맞추기 위해 삽입된 `unused`의 멤버가 있음을 알 수 있다.

이때 블록, 혹은 `struct block`은 디스크와 같이 연속된 저장 공간을 가진 장치의
추상화이다. 따라서, Pintos에서는 블록에 대한 연산도 `devices/block.h`와 
`devices/block.c`에서 제공하지만, 해당 모듈에 대한 설명은 이 프로젝트의 범위를 
벗어나므로 생략한다. 

따라서, `inode_disk`는 디스크의 특정 섹터, `start`에서 시작해 `length`만큼 
이어지는 어떤 자료를 나타내는 자료구조이다. 이때, `length`가 한 섹터의 
크기보다 크다면 해당 자료는 여러 섹터에 나누어져 저장될 수도 있을 것이다.
`inode`는 이러한 `inode_disk`에 더해, 현재 읽거나 쓰고 있는 섹터를 저장하는 
자료구조이다. 

Pintos는 이러한 추상화를 통해 메모리 상에 저장되어 있는 한 묶음의 데이터를 
표현한다. (의도적으로 파일이라는 말을 배제하고 설명하는 중이다.) 그렇다면, 
이러한 `inode`에 대한 공개 인터페이스 함수에는 무엇이 있을까?

```C
/* From filesys/inode.c */
/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}
```
`inode` 모듈을 초기화하는 함수이다. `open_inode`는 현재 열려있는 `inode`들을
저장하는 전역 리스트이다.

```C
/* Frome filesys/inode.c */
/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      if (free_map_allocate (sectors, &disk_inode->start)) 
        {
          block_write (fs_device, sector, disk_inode);
          if (sectors > 0) 
            {
              static char zeros[BLOCK_SECTOR_SIZE];
              size_t i;
              
              for (i = 0; i < sectors; i++) 
                block_write (fs_device, disk_inode->start + i, zeros);
            }
          success = true; 
        } 
      free (disk_inode);
    }
  return success;
}
```
`length`만큼의 길이를 가지는 `inode`를 초기화하고, 이를 `sector`가 가리키는
섹터에 쓰는 명령어이다. `free_map_allocate()`는 `sectors` 만큼의 블록 섹터를
할당받는 함수이다. 이렇게 디스크 공간을 할당받은 후, `block_write()` 함수를
호출하여 `sector`가 가리키는 섹터에 해당 `disk_inode`를 쓴다. 이후 할당받은
섹터를 모두 0으로 초기화하고, 블록에 이미 저장되어 쓸모없어진 `disk_inode`를
할당 해제하며 함수 실행은 끝난다.

```C
/* Frome filesys/inode.c */
/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}
```
`sector`에 해당하는 `disk_inode`를 열어 그 결과를 `inode`로 반환하는 함수이다. 
만약 해당 `sector`에 해당하는 `disk_inode`가 이미 열려 있는 상태라면 
`inode_reopen()`을 호출하여 새로운 `inode`를 위한 공간을 할당받는 과정을
생략한다. 만약 해당 `inode`가 현재 열려있지 않다면 새로운 메모리를 할당받고,
`open_inodes`에 새로운 `inode`를 삽입하며, `inode` 정보를 초기화하고 블록에서
해당 `inode`에 해당하는 `disk_inode`를 읽어 이를 `inode->data`에 저장한다.

```C
/* Frome filesys/inode.c */
/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}
```
만약 현재 열고자 하는 `inode`가 이미 열려있는 경우 해당 `inode`의 `open_cnt`만을
증가시키는 함수이다.

```C
/* Frome filesys/inode.c */
/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}
```
해당 `inode`의 inode 번호를 반환하는 함수이다.

```C
/* Frome filesys/inode.c */
/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length)); 
        }

      free (inode); 
    }
}
```
`inode`를 닫는 함수이다. 만약 닫은 이후에도 `inode`의 `open_cnt`가 0이 아니라면,
해당 `inode`를 누군가 계속 열고 있다는 뜻이므로 해당 `inode`를 할당 해제하지는
않는다. `free_map_release`는 `inode_open()`에서 할당받았던 블록상의 공간을 다시 
할당 해제하는 함수이다. 이때, 만약 해당 `inode`를 열고 있는 다른 스레드가 
존재하지 않는다면, `inode`를 저장하는 메모리 공간을 할당 해제한다. 이에 더해, 
만약 해당 `inode`가 제거된 상태라면 `dick_inode`를 저장하던 섹터와 실제 데이터를
저장하던 섹터들 모두를 할당 해제하여 다른 `disk_inode`에서 해당 공간을 사용할 수
있게 한다.

```C
/* Frome filesys/inode.c */
/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}
```
`inode`를 삭제됨 상태로 바꿔 이후 `inode`가 닫힐 때 해당 블록 공간을 할당 해제할
수 있도록 하는 함수이다.

```C
/* Frome filesys/inode.c */
/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}
```
`inode`를 `offset`에서부터 시작하여 `size`만큼 읽어들여 `buffer_`에 저장하는
함수이다. 이 함수는 먼저 각 섹터에서 읽어들어야 할 `chunk_size`를 계산한다.
만약 `chunk_size`가 한 섹터의 크기와 같다면 해당 섹터를 통째로 `buffer`로 
복사한다. 만약 `chunk_size`가 한 섹터의 크기보다 작다면, 먼저 `bounce`라는
`buffer`의 버퍼에 섹터를 복사한 후 `bounce`에서 `chunk_size`만큼을 `buffer`에
복사한다. 이후 읽어들인 크기만큼을 남은 읽어들일 크기인 `size`에서 빼고,
다음 읽을 위치인 `offset`, 다음 `buffer`에 저장할 위치인 `bytes_read`에 읽어들인
크기만큼을 더한다.

```C
/* Frome filesys/inode.c */
/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}
```
`inode`가 가리키는 섹터의 `offset` 위치에 `size`만큼을 `buffer`로 쓰는 함수이다.
동작은 `block_read()`가 `block_write()`로 바뀐 것을 제외하고는 
`inode_read_at()`과 같으나, 특정 섹터 전부에 쓰는 것이 아니라면 일단 해당
섹터를 읽어서 수정될 부분만을 써야 하기 때문에 `block_read()`를 호출해 
일단 해당 섹터를 읽고, `bounce`에 쓰기를 수행한 이후, 해당 데이터를 블록에
쓴다는 차이점이 있다.

```C
/* Frome filesys/inode.c */
/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
```
해당 `inode`에 쓰기를 금지/허용하는 함수와 `inode`의 길이를 반환하는 함수이다.

```C
/* From filesys/file.c */
/* An open file. */
struct file 
  {
    struct inode *inode;        /* File's inode. */
    off_t pos;                  /* Current position. */
    bool deny_write;            /* Has file_deny_write() been called? */
  };
```
Pintos에서 파일은 상술한 `inode`와 파일에 읽거나 쓸 위치를 저장하는 `pos`, 
그리고 파일 쓰기를 거절 혹은 허용할지를 나타내는 `deny_write`의 멤버를 가지고
있다. 파일에 대한 인터페이스 함수들은 대부분 `inode`에 대한 인터페이스로 
구현되어 있다. 즉, Pintos에서 파일은 `inode`에 현재 읽거나 쓸 위치에 대한 개념이
더해진 자료구조라 볼 수 있다.

```C
/* From filesys/file.c */
/* Opens a file for the given INODE, of which it takes ownership,
   and returns the new file.  Returns a null pointer if an
   allocation fails or if INODE is null. */
struct file *
file_open (struct inode *inode) 
{
  struct file *file = calloc (1, sizeof *file);
  if (inode != NULL && file != NULL)
    {
      file->inode = inode;
      file->pos = 0;
      file->deny_write = false;
      return file;
    }
  else
    {
      inode_close (inode);
      free (file);
      return NULL; 
    }
}
```
`inode`에 해당하는 파일을 여는 함수이다. 만약 인자로 넘어온 `inode`가 NULL
포인터이거나 파일 구조체를 저장할 메모리 할당에 실패하면 NULL 포인터를 반환한다.
이후 파일 인터페이스 함수들은 단순한 `inode`에 대한 감싸기(wrapper) 함수이므로
설명을 생략한다.

Pintos 파일 시스템 추상화 계층의 최상위에는 `filesys` 모듈이 있다. `filesys`
모듈은 블록, `inode`, 파일까지 모든 것을 총망라하여 프로그래머에게 파일 처리
기능을 제공한다.

```C
/* From filesys/filesys.c */
/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}
```
파일 시스템을 초기화하는 함수이다. 먼저 블록 장치를 `block_get_role`을 통해 
가져온 후, 상술한 `inode`를 초기화하는 `inode_init()`, 현재 파일 시스템에
비어있는 섹터의 위치를 저장하는 자료구조를 초기화하는 `free_map_init()`을
호출하고, 만약 `format` 인자가 참이라면 파일 시스템을 포맷한다. 마지막으로
블록 장치에 파일로 저장된 `free_map`을 `free_map_open()`을 통해 열면서 함수
실행은 끝난다.

```C
/* From filesys/filesys.c */
/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}
```
파일 시스템 사용이 끝난 후 이를 종료하는 함수이다.

```C
/* From filesys/filesys.c */
/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, name, &inode);
  dir_close (dir);

  return file_open (inode);
}
```
`name`에 해당하는 파일을 여는 함수이다. 루트 디렉터리에서 파일 이름에 해당하는
`inode`를 `dir_lookup()` 함수를 이용해 찾고, 이후 `file_open()` 함수를 호출하여
해당 파일의 파일 포인터를 반환한다.

```C
/* From filesys/filesys.c */
/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir = dir_open_root ();
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 

  return success;
}
```
파일 시스템에서 `name`이라는 이름을 가진 파일을 삭제하는 함수이다. 이때, 열려
있는 파일을 삭제하더라도 이미 열려있는 파일에는 영향을 주지 않고 이후에 해당
파일에 대한 접근만을 막도록 구현되어있다.

주의해야 할 점은 현재 Pintos 구현의 파일 시스템은 내부적으로 데이터 일관성이
보장되지 않고, 파일의 크기를 바꿀 수 없으며, 파일 하나는 언제나 블록 장치 내의 
연속된 공간을 차지하도록 구현되어 있는 등의 다양한 제약조건이 있다는 점이다.
특히 파일 시스템 내부적으로 동기화가 구현되어있지 않다는 점은 이후 파일 시스템
인터페이스를 이용해 시스템 호출을 구현할 때 주의하여야 할 점이다.

## Design Description

### Process Termination Messages

### Argument passing

### System Calls
Pintos에서 사용자 프로세스가 시스템 호출을 발생시켰을 경우 상술한 대로 0x30
인터럽트를 발생시킨 후 `syscall_handler()`로 실행 흐름을 넘겨 해당 함수에서
시스템 호출을 처리하게 된다. 따라서, 각각의 시스템 호출에 대한 처리 방법을
고민하기에 앞서서 어떻게 시스템 호출의 종류를 알아내고 시스템 호출과 같이
넘겨진 인자를 처리할 것인지를 고민해야 한다.

상술한 바와 같이, 이는 `syscall_handler()`의 인자 `f`의 멤버 `esp`를 통해 접근
가능하다. 사용자 프로세스는 시스템 호출을 발생시키며 시스템 호출의 각 인자들과
시스템 호출 번호를 스택에 push하며, 따라서 `f->esp`를 역참조한 결과는 시스템
호출 번호가 될 것이다. 또한, `f->esp`를 역참조하기 전 `f->esp`가 유효한 사용자 
영역 가상 주소인지를 확인하여야 한다.

시스템 호출 번호를 통해 시스템 호출의 종류를 알아냈다면, 이제는 시스템 호출의
인자 목록을 알아낼 차례다. 전술한 바와 같이, 사용자 스택의 시스템 호출 번호 
아래에는 0개 혹은 그 이상의 인자들이 push되어 있다. 인자의 개수는 시스템 호출
종류를 통해 알 수 있다.

이후에는 시스템 호출의 종류에 따라 해당하는 처리 루틴을 실행하면 될 것이다. 
각 시스템 호출 전부를 `syscall_handler()`에서 처리하는 것은 코드의 가독성에 
좋지 못한 선택이므로, `static pit_t exec (void *esp)`와 같이 각 시스템 호출을 
처리하는 함수를 따로 정의하여 그 함수에서 각각의 시스템 호출을 처리하도록
하는 것이 좋을 것이다.

x86 호출 규약에 따르면 함수의 반환값은 eax 레지스터를 통해 전달되어야 한다.
따라서, `f->eax`를 각 시스템 호출의 처리 결과로 바꾸어 사용자 프로세스에게 
시스템 호출이 마치 일반적인 함수 호출과 같이 실행되는 것처럼 보이도록 하여야 
한다.

또한 마지막으로 사용자 영역 가상 주소를 역참조하는 도중 페이지 폴트가 
발생하였을 때, `page_fault()`에서 이를 적절히 처리할 수 있도록 `page_fault()`의
코드를 변경하여야 할 것이다.

위의 논의사항을 (완성되지 않은) 코드로 표현하면 다음과 같을 것이다.
```C
/* From userprog/syscall.c */
void
syscall_handler (struct intr_frame *f)
{
  int syscall_number;
  uint32_t retval;

  if (is_user_vaddr (f->esp))
    syscall_number = ((int *) f->esp)[0];
  else
    /* The user stack pointer is faulty. Make user process that invoked system 
       call exit. Be sure not to leak resources when killing the process. */

  switch (syscall_number) {
    case SYS_HALT:
      halt (void);
      break;
    ...
    case SYS_TELL:
      retval = (uint32_t) tell (f->esp);
      break;
    ...
  }

  f->eax = retval;
}
```
```C
/* From userprog/exception.c */
static void
page_fault (struct intr_frame *f) 
{
  ...

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

  if (not_present && !write && !user)
    /* This page fault is raised while handling system call. */

  ...
}
```

#### User-Process-Manipulating System Calls
이번 프로젝트에서 구현해야 할 시스템 호출의 종류를 크게 나누자면 다른 사용자
프로세스에 영향을 미치는 시스템 호출과, 파일 시스템을 수정하는 시스템 호출로
나눌 수 있을 것이다. 전자에는 `halt()`, `exit()`, `exec()`, `wait()`가 포함되며,
나머지 시스템 호출은 모두 파일 시스템과 관련된 시스템 호출이다.

`halt()` 시스템 호출의 경우 Pintos 문서에서 서술된 바와 같이 
`shutdown_power_off()` 호출로 시스템을 종료하면 될 것이다.

`exit()`, `wait()`의 경우 조금 복잡한데, 어떤 프로세스가 종료하는 경우 그 
프로세스를 기다리고 있는 모든 프로세스를 깨우고, `wait()`의 반환값으로 
기다리고 있던 프로세스의 종료 코드를 전달해야 하기 때문이다. 한 가지 가능한 
구현으로는 각 스레드에 해당 스레드를 기다리고 있는 스레드의 리스트를 저장하고, 
만약 해당 스레드가 종료될 경우 대기 스레드 목록에 있는 모든 스레드에 종료 코드를
전달하는 방법이 있을 것이다. Pintos의 프로세스는 단 하나의 스레드만을 가질 수 
있기 때문에, '프로세스'와 '스레드'는 맥락에 상관없이 서로 바뀌어 사용될 수 
있다는 점을 기억하자.

`exec()`은 `thread_create()`를 통해 `process.c`의 `load()`를 실행하고, 
`cmd_line`을 쪼갠 인자 목록을 `aux`로 전달받는 새로운 스레드를 생성하면 될 
것이다.

#### File-System-Manipulating System Calls
파일 시스템을 조작하는 시스템 호출의 경우, Pintos에서 제공하는 파일 시스템
인터페이스 함수를 이용하면 될 것이다. 대부분의 시스템 호출이 파일 시스템
인터페이스와 일대일로 대응되며, Pintos의 파일 시스템은 내부적으로 동기화를
보장하지 않아 사용자가 이를 사용할 때 명시적으로 임계 구역에 대한 lock 등의
동기화 대책을 강구해야 한다는 점을 주의하며 구현하면 될 것이다.₩:ㅈ

### Denying Write to Executables

## Design Discussion
