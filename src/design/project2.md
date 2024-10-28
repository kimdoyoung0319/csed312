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
Project 1 design report에서 서술한 바와 같이, IA32 아키텍처에서 `int` 명령어로
인터럽트가 발생하면, CPU는 IDTR이 가리키는 IDT를 참조해 해당하는 ISR을 호출한다.
Pintos에서는 `threads/interrupt.c`의 `intr_init()`에서 IDTR을 IDT를 가리키도록
초기화한다. 각 IDT에는 `intr_stubs.S`에 정의된 `intrNN_stub`(`NN`은 `00`부터 
`FF`까지의 16진수)가 등록되어 있어, 인터럽트 발생시 해당 루틴으로 실행 흐름을
바꾼다. 이후 `intrNN_stub`은 `intr_entry`로 점프하고, `intr_entry`는 
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
제공하지 않는다. 따라서, Pintos에서는 `PHYS_BASE` 위의 가상 주소 공간을 일대일로
물리적 주소에 대응시키는 방법을 사용한다. 정확히는, 물리적 주소 `paddr`에 대해
대응되는 커널 영역 가상 주소는 `paddr + PHYS_BASE`가 된다. Pintos는 
`threads/init.c`의 `paging_init()`을 통해, 해당 영역의 가상 주소가 물리적 주소에
바로 대응될 수 있도록 페이지 디렉터리와 페이지 테이블을 초기화한다.

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
/* threads/pte.h */
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
역참조하기 이전에 이의 유효성을 검증하는 것이다. Pintos에서는 이러한 유효성 
검증과 하드웨어 수준의 페이지 테이블에 대한 추상화를 위해 `userprog/pagedir.c`에
정의된 인터페이스 함수를 제공한다.







## Design Description
<!-- TODO(Doyoung): Add descriptions about how to pass return value to user 
                    program after system call. -->
## Design Discussion
