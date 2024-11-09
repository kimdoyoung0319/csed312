# Project 2 - Final Report (Team 28)
프로젝트 2는 프로세스 종료 메세지 출력, 인자 전달, 시스템 호출 구현, 실행 파일에
대한 쓰기 금지의 네 가지 Task를 완료하여야 한다. 이를 위해 본 팀에서는 먼저 
기존의 Pintos 코드를 사용자 프로세스 관점에서 재작성하였으며, 이를 토대로 시스템
호출을 구현하였다. 따라서, 본 보고서에서는 먼저 사용자 프로세스를 나타내기 위한
자료구조와 사용자 프로세스에 대한 인터페이스 함수들의 구현에 대해서 먼저 다루고,
이를 토대로 구현된 시스템 호출 함수들에 대해 다룬다.

## Process Data Structures and Implementations

### Improvements
프로세스는 독립된 주소 공간과 실행 흐름을 가지는, 프로그램 실행의 한 단위이다.
또한, 프로그램이 디스크 내의 코드의 나열으로 나타내어지는 정적인 대상이라면,
프로세스는 이 프로그램이 실체화되어 메모리에 적재된 동적인 대상이라고 볼 수도
있다. 따라서, 프로세스를 구현하기 위해서는 디스크에 ELF(Executable and Linkable
File format) 파일로 존재하는 프로그램을 메모리에 적재하는 과정과, 각 
프로세스마다 독립된 주소 공간을 제공하는 과정이 구현되어야 한다.

Pintos의 기존 구현은 둘 모두를 어느 정도 구현하지만, 프로세스를 실행하는데 
필수적인 함수들, `process_execute()`, `process_wait()`, `process_exit()` 등이
미완성된 상태로 아직 제대로 동작하지 않는 상태이다. 따라서, 이번 프로젝트에서는
이러한 사용자 프로세스에 대한 커널 계층에서의 인터페이스 함수들을 구현해야 한다.

이러한 인터페이스 함수들을 구현하는 데 있어 한 가지 난점은, Pintos의 기존 구현은
사용자 프로세스와 커널 스레드를 구분하지 않는다는 점이다. `thread.h`에 정의된
기존의 `struct thread`의 코드를 살펴보면 다음과 같다.

```C
/* From former version of thread.h */
struct thread
  {
    ...

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif

    ...
  };
```

`pagedir`은 메모리 상의 페이지 디렉터리, 혹은 최상위 계층의 페이지 테이블을 
가리키는 포인터이다. Pintos의 기존 구현에서는, `pagedir`이 `struct thread`의
멤버로 구현되어 있다. 다시 말해, Pintos의 기존 구현은 모든 스레드가 각각의 주소 
공간을 가지고 있으며 프로세스와 분리되지 않는다. 이는 이후 `process_wait()`와
`process_exit()` 등의 함수를 구현할 때 한 가지 난점을 불러일으키며, 개념적으로도
커널 스레드와 그와 연관된 사용자 프로세스는 분리되어 존재하는 것이 맞기 때문에
사용자 프로세스를 나타내는 `struct process`를 새롭게 정의하기로 결정하였다. 
`struct process`는 일반적으로 PCB(Process Control Block)으로 불리는 자료구조로,
자세한 설명은 다음 단락에서 다룰 것이다.

### Details and Rationales
<!-- To be filled by Doyoung. -->

### Discussions
<!-- To be filled by Doyoung. -->

## System Calls

### Improvements
<!-- To be filled by Taeho. -->

### Details and Rationales
<!-- To be filled by Taeho. -->

### Discussions
<!-- To be filled by Taeho. -->

## Overall Discussion
