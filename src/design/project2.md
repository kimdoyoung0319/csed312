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

이러한 시스템 콜에는 시스템의 실행을 중지하는 `halt()`, 프로세스의 실행을 자식
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

### How system calls are handled in Pintos?
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


## Design Description
## Design Discussion