# Project 1 - Design Report (Team 28)
## Project Description
Project 1은 크게 3가지를 구현해야 한다.
1. Alarm Clock
2. Priority Scheduler
3. Advanced Scheduler

### Alarm Clock
devices/timer.c 에 위치한 timer_sleep 함수를 다시 구현하는 것이 핵심 사항이다. 기존에 구현된 timer_sleep은 함수가 계속 실행되면서 while loop 내에서 tick이 넘을 때까지 확인하게 되는데, 이는 CPU를 계속해서 점유하게 되므로 busy waiting 을 하게 된다. 따라서 새롭게 해당 함수를 구현하여 thread_block 을 통해 thread를 실행되지 않도록 바꿔주도록 새롭게 Alarm Clock을 구현해야 한다.

### Priority Scheduler
현재 구현된 Scheduler 같은 경우에는 Round-Robin으로 구성되어 있어 Time Slice에 따라 균일하게 thread를 나누게 된다. 하지만 이는 priority에 따라서 중요한 thread를 우선적으로 실행할 수 없게 된다. 따라서 Scheduler 를 새롭게 구현하여 priority를 기반의 priority scheduler를 구현해야 한다. highest priority 인 thread를 우선적으로 실행될 수 있도록 하며, 새로운 thread가 생기거나 기존 thread의 priority가 바뀌게 된다면 마찬가지고 가장 높은 priority 를 가진 thread를 실행하도록 한다.

### Advanced Scheduler
priority scheduler의 경우 기존에 결정된 priority를 기반으로 가장 높은 priority의 thread만 실행시키게 된다. Advanced Scheduler의 경우 4.4BSD의 MLFQS를 구현하는 것으로 MLFQS가 자동으로 nice 값과, recent_cpu 값 등등을 기반으로 priority 값을 조절하여 이를 기반으로 가장 높은 priority를 실행하게 된다.

## Code Analysis
### Thread - thread.h / thread.c
```C
struct thread
  {
    tid_t tid;                          
    enum thread_status status;          
    char name[16];                   
    uint8_t *stack;              
    int priority;                
    struct list_elem allelem;   
    struct list_elem elem;    
#ifdef USERPROG
    uint32_t *pagedir;           
#endif
    unsigned magic;       
  };
```
Thread의 정의를 살펴보면 Process 내에서 기존에 정의된 할일을 수행하게 되는 주체를 의미하는 것으로 thread 각각은 고유한 stack과 실행을 갖게 된다. 구현된 struct thread를 살펴보면, 고유하게 갖는 tid와 상태를 저장하는 status, 이름을 저장하는 name, 그리고 고유한 stack을 가리키는 포인터로 이뤄져 있다. 이외에도 우선순위를 위한 priority와 모든 thread를 저장하는 list를 위한 list_elem인 allelem, 그리고 synchronization 을 위해 사용되는 list_elem 인 elem, 그리고 USERPORG시 사용될 page의 위치를 가르킬 pagedir과 stack overflow를 감지하기 위한 magic 으로 구성되어 있다.

__enum thread_status__
```C
enum thread_status
  {
    THREAD_RUNNING,     
    THREAD_READY,    
    THREAD_BLOCKED,     
    THREAD_DYING      
  };
```
thread의 상태를 저장하기 위한 enum으로 현재 실행 중인 상태를 가르키는 THREAD_RUNNING, 그리고 실행될 준비가 되었지만 기다리는 상황인 THREAD_READY, 그리고 실행되는 중간에 중단된 상태인 THREAD_BLOCKED, 마지막으로 실행이 완료되어 종료를 기다리는 THREAD_DYING으로 구성되어 있다.

__thread_init__
```C
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);

  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}
```
thread를 처음 실행할 때 최초로 실행되는 함수로 lock과 ready 된 thread를 저장하는 ready_list, 그리고 모든 thread를 저장하는 all_list 각각의 list를 초기화하는 것으로 시작한다. 이후 현재 running 중인 thread(최초로 실행 중인 부팅 프로세스의 메인 쓰레드)를 받아서 init_thread 를 통해 초기화를 해준 뒤에, 상태와 tid를 할당하여 최초의 thread를 지정해주는 함수이다.

__thread_start__
```C
void
thread_start (void) 
{
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);
  intr_enable ();
  sema_down (&idle_started);
}
```
초기 thread 를 지원하기 위해 실행되는 함수 중 하나로 우선 함수의 실행 중 문제를 막기 위해 sema를 생성하며 시작된다. idle_thread를 생성하여 이후 cpu를 적절히 배분할 수 있도록 사용되며, interrupt를 enable 하고, 이후 sema_down을 통해 다시 해제하고 함수의 실행이 완료된다.

__thread_tick__
```C
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}
```
thread가 얼마나 tick을 소모했는지 확인하는 함수로 idle thread와 userprog, 그리고 kernel thread를 나눠서 확인하는 방식으로 구성되어 있다. 이외에도 thread_tick 이 TIME_SLICE를 넘기게 될 경우 intr_yield_on_return을 실행하여 다른 thread 가 실행될 수 있도록 한다.

__thread_create__
```C
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  thread_unblock (t);

  return tid;
}
```
새로운 thread를 생성할 때 쓰이는 함수로 kernel, switch 등을 위해서 frmae을 할당하며 init_thread 함수를 통해서 새로운 thread를 생성한다. 각 설정을 마무리한 뒤 thread_unblock을 통해서 thread를 ready 상태로 바꾸고 ready_list 에 넣은 뒤 tid를 반환한다.

__thread_block__
```C
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}
```
thread를 block하는 함수로 현재 실행되고 있는 thread를 blocked 상태로 바꾸고 schedule() 함수를 통해 현재 상태를 기반으로 새로운 thread 를 실행하도록 thread가 switch 된다.

__thread_unblock__
```C
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  list_push_back (&ready_list, &t->elem);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}
```
blocked 된 상태의 thread t 를 입력을 받으면, 우선 interrupt 를 disable 한 뒤에 t 의 상태를 ready로 바꾸고 ready_list에 넣어준다. 이후 interrupt를 다시 설정하고 반환된다.

__thread_current__
```C
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}
```
현재 실행 중인 thread를 반환하는 함수로, running_thread 를 통해서 thread를 받아와서 t가 thread인지 여부와 running 중인지 여부를 확인한 뒤 t를 반환한다.

__thread_exit__
```C
thread_exit (void) 
{
  ASSERT (!intr_context ());
#ifdef USERPROG
  process_exit ();
#endif
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}
```
thread가 exit 될 때, 즉 thread를 종료하는 역할을 수행하는 함수로 all_elem에서 현재 실행 중인 thread를 제거하고 현재 상태를 THREAD_DYING 으로 바꿔주는 역할을 수행한다. 이후 schedule() 함수 내에서 THREAD_DYING 상태인 thread를 제거하는 역할을 수행하게 되며, NOT_REACHED (); 에 도달하지 않게 된다.

__thread_yield__
```C
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) 
    list_push_back (&ready_list, &cur->elem);
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}
```
현재 CPU를 점유하며 실행 중인 thread를 다른 thread에게 점유를 넘겨주는 함수로 interrupt를 disable 한 뒤에 현재 thread를 ready_list에 push 하고, 상태를 ready 로 만들어준 뒤 schedule 함수를 실행하여 다른 thread로 switch가 일어나도록 한다. 이후 interrupt를 다시 실행한다.

__thread_foreach__
```C
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}
```
현재 실행되고 있는 모든 thread에 대해서 같은 작업을 수행하는 함수로 인자로 받은 func과 aux를 기반으로 현재 thread에 대해서 func을 실행하게 되는 과정으로 이뤄져 있다.

__thread_set_priority__
```C
void
thread_set_priority (int new_priority) 
{
  thread_current ()->priority = new_priority;
}
```
thread의 priority 를 새로운 priority 로 설정한다.

__thread_get_priority__
```C
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}
```
현재 thread의 priority 를 반환한다.

__idle__
```C
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      intr_disable ();
      thread_block ();

      asm volatile ("sti; hlt" : : : "memory");
    }
}
```
ready_list에 실행할 함수가 없을 때 idle_thread를 실행하기 위해 초기 설정하는 함수로 현재 thread를 idle_thread로 받아 sema_up 을 통해서 sema를 해제하며, interrupt 를 disable 한 뒤 무한 반복하는 for 문 안에서 thread_block 과 실행을 반복하게 된다.

__kernel_thread__
```C
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       
  function (aux);    
  thread_exit ();  
}
```
kernel thread가 실행되도록 지정해주는 함수로 interrupt를 enable 하는 것을 시작으로 thread function 을 실행하고, 실행이 완료된 이후 thread를 exit하는 것으로 실행을 마무리한다.

```C
struct thread *
running_thread (void) 
{
  uint32_t *esp;
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}
```
현재 실행 중인 thread를 반환하는 함수로 esp 가 담고 있는 현재 stack pointer 값을 기반으로 현재 실행 중인 thread를 반환한다.

```C
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}
```
현재 thread가 문제가 없는지 확인하는 함수로 t 가 NULL이 아니며 maigc 값이 thread에서 올바르게 초기화되어 THREAD_MAGIC으로 설정되었는지 확인하는 함수이다.

```C
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->magic = THREAD_MAGIC;

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}
```
thread의 초기 세팅을 위한 함수로, 상태를 Block으로 된 thread로 생성하고 interrupt를 disable 한 뒤에 all_list로 넣어서 thread를 생성하게 된다.

```C
static void *
alloc_frame (struct thread *t, size_t size) 
{
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}
```
thread에 frame을 할당하기 위한 함수로 thread 각각이 보유하고 있는 stack 에 size 만큼의 공간을 할당하게 된다.

```C
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}
```
다음에 실행될 thread를 ready_list, 즉 THREAD_READY 상태인 thread를 저장하고 있는 리스트에서 반환하게 되며, 실행될 thread가 없다면 idle_thread를 반환하게 된다.

```C
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  ASSERT (intr_get_level () == INTR_OFF);
  cur->status = THREAD_RUNNING;
  thread_ticks = 0;

#ifdef USERPROG
  process_activate ();
#endif

  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}
```
thread switch 중 내부에서 실행되는 함수로 현재 thread를 running thread로 설정하고 tick을 초기화하여 time_slice에 따라서 실행될 수 있도록 만들어준다. 이외에 thread가 dying 이었다면 이 함수 내에서 thread를 아예 제거하게 된다.

__schedule__
```C
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}
```
다음 실행될 thread와 현재 실행되고 있는 thread를 받아 서로를 switch 해주는 함수로 assembly 로 구현된 switch_threads 를 통해 이 함수 내에서 thread의 switch 가 발생하게 되며 switch 이후 이전에 실행되고 있는 thread를 tail에 schedule 하게 된다.

__allocate_tid__
```C
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}
```
static으로 선언된 next_tid를 토대로 새로운 tid를 할당해주는 함수로 lock을 얻은 뒤에 새로운 tid를 할당하고 lock을 해제한 뒤 tid를 반환하게 된다.

__switch.S__
```assembly
#include "threads/switch.h"

.globl switch_threads
.func switch_threads
switch_threads:
	pushl %ebx
	pushl %ebp
	pushl %esi
	pushl %edi

.globl thread_stack_ofs
	mov thread_stack_ofs, %edx
	movl SWITCH_CUR(%esp), %eax
	movl %esp, (%eax,%edx,1)
	movl SWITCH_NEXT(%esp), %ecx
	movl (%ecx,%edx,1), %esp

	popl %edi
	popl %esi
	popl %ebp
	popl %ebx
        ret
.endfunc

.globl switch_entry
.func switch_entry
switch_entry:
	addl $8, %esp
	pushl %eax
.globl thread_schedule_tail
	call thread_schedule_tail
	addl $4, %esp
	ret
.endfunc
```
switch_threads를 통해 현재 실행되고 있는 thread의 레지스터 값들을 스텍에 저장하고 thread_stack_ofs를 통해 새롭게 바뀔 thread의 stack pointer 값으로 바꿔준다. 이후 switch_entry 가 바뀐 thread 가 다시 실행되게 되며, 이 함수를 통해 thread의 switch 를 완료하게 된다.

### Semaphore - synch.h / synch.c
```C
struct semaphore 
  {
    unsigned value;        
    struct list waiters;
  };
```

각각의 Critical Section 에 하나의 Thread 만 접근 가능하도록 막아주는 역할을 하는 struct 로, value의 값을 통해 0보다 크면 접근 가능, 0 이면 접근이 불가능하도록 설정하며, 이외에도 기다리고 있는 Thread를 저장하는 waiters list를 함께 갖고 있다.

__sema_init__
```C
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}
```
semaphore 를 초기화시켜주는 함수로, 인자로 sema와 value를 받아 semaphore의 value를 저장해주고, semaphore의 waiters list를 초기화시켜주는 역할을 수행한다.

__sema_down__
```C
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
    {
      list_push_back (&sema->waiters, &thread_current ()->elem);
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}
```
semaphore의 value를 내리는 역할을 수행하는 함수이다. 혹시 모를 interrupt로 인해 문제가 될 수 있는 상황을 방지하고자, `intr_disable()` 을 통해 interrupt를 끄고 수행하는 함수이며, 만약 semaphore의 value가 이미 0이라 value를 내릴 수 없다면, value를 내릴 수 있을 때까지 while 문에서 해당 thread를 semaphore의 waiters로 추가하여, block하여 대기하도록 한다. 만약 value가 0이 아닌 상황이라면, value의 값을 내리고 interrupt를 다시 실행하여 semaphore를 down 하게 된다.

__sema_try_down__
```C
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}
```
`sema_down()` 과 크게 같은 역할을 수행하는 함수이지만, 기존에 사용하는 방식인 while 문 대신에 if 문 한 번만 수행하는 방식으로, semaphore의 value를 한 번만 확인하여 성공 여부만 반환하는 함수이다.

__sema_up__
```C
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (!list_empty (&sema->waiters)) 
    thread_unblock (list_entry (list_pop_front (&sema->waiters),
                                struct thread, elem));
  sema->value++;
  intr_set_level (old_level);
}
```
이 함수도 마찬가지로 우선 interrupt를 중지하고 함수를 시작하게 된다. 이후 waiters list에서 가장 맨 앞의 thread를 unblock 시키고, semaphore_value를 올리므로써 해당 thread에게 semaphore 를 넘겨주는 과정을 수행하게 된다. 이후 interrupt 를 다시 설정하고 함수를 종료하게 된다.

### Lock - synch.h / synch.c
```C
struct lock 
  {
    struct thread *holder;      
    struct semaphore semaphore; 
  };
```
Lock 도 Semaphore의 역할과 유사하게, 각각의 공유되고 있는 critical section에 여러 thread가 접근하지 못하게 막는 역할을 수행하고 있으며, lock의 경우 현재 점유하고 있는 thread를 저장하는 holder와 해당 critical section 에 따른 semaphore를 함께 저장하고 있는 구조로 설정되어 있다.

__lock_init__
```C
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);
}
```
lock을 초기화하는 함수로 lock의 holer와 semaphore를 초기화하며, semaphore의 value를 1로 설정한다.


__lock_acquire__
```C
void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));

  sema_down (&lock->semaphore);
  lock->holder = thread_current ();
}
```
lock을 소유하기 위해 확인하는 함수로 우선 lock 이 NULL이 아니며, lock이 현재 thread에 held가 되지 않은 상황인지 혹은 interrupt 가 실행되고 있지 않은 상황에서만 함수를 실행시키게 된다. 이 함수 내에서는 sema_down을 내부에서 실행시켜 semaphore의 value를 내릴 때 까지 기다리게 되고, 실행이 완료된다면, lock의 holder에 현재 thread가 들어가게 된다.

__lock_try_acquire__
```C
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}
```
기능은 위의 ```lock_acquire()``` 과 같은 기능을 수행하지만, ```sema_down()``` 대신 ```sema_try_down()``` 을 사용하여 마찬가지로 한 번만 lock 을 점유하도록 확인하도록 작동하는 방식으로, 성공 여부를 반환하게 된다.

__lock_release__
```C
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  lock->holder = NULL;
  sema_up (&lock->semaphore);
}
```
lock을 release하는 기능을 수행하는 함수로 lock 이 존재하며 이번에는 lock이 현재 thread가 점유하고 있는지 확인한다. 이후 lock의 holder를 NULL로 바꾸고   ```sema_up()```을 통해서 semaphore를 복구하게 된다.

__lock_held_by_current_thread__
```C
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}
```
lock이 현재 thread에게 점유되고 있는지를 확인하는 함수로 lock의 holder와 thread_current()를 통해 반환받은 현재 thread를 비교하여 여부를 알려주는 방식으로 구성되어 있다.

### Condition - synch.h / synch.c
```C
struct condition 
  {
    struct list waiters;        
  };
```
Condition은 특정 상황 (ex 비어있는 리스트 등)으로 인해 semaphore나 lock 을 갖고 있는 상황이지만, 더 이상 수행할 수 없을 때 condition을 통해서 다른 thread와의 실행을 관리하게 된다. struct는 보다 단순하게 해당 condition을 기다리는 waiters를 저장하는 list로만 이뤄져 있다.

__struct sempahore_elem__
```C
struct semaphore_elem 
  {
    struct list_elem elem;             
    struct semaphore semaphore;        
  };
```
Condition의 waiters에 저장될 semaphore 를 list로 관리하기 위해서 고안된 struct 로 semaphore를 linked list에서 사용하기 위해서 정의되었다.

__cond_init__
```C
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}
```
condition을 초기화하는 함수로, waiters list를 초기화화하는 역할을 수행한다.


__cond_wait__
```C
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);
  list_push_back (&cond->waiters, &waiter.elem);
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}
```
우선 cond_wait 이 실행된다면 해당 thread를 다른 condition이 signal을 전파하기 전 까지는 실행이 되어서는 안된다. 따라서 이를 위해 처음부터 value가 0인 semaphore를 생성하고 그 semaphore를 waiters list에 추가하고, 이후 해당 thread가 기존에 갖고 있던 lock을 해제하므로써 다른 thread가 해당 자원에 접근이 가능하도록 한다. 이후 sema_down을 통해서 이미 value가 0인 semaphore를 down 하려고 하므로 block 되며, 다른 thread에서 signal을 통해 sempahore의 value를 1로 올려준다면 lock을 다시 acquire하여 실행하게 된다.

__cond_signal__
```C
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters)) 
    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
}
```
condition의 waiters list 중에서 semaphore 즉, 기다리고 있는 sema_down 에서 멈춰있는 semaphore의 value를 1로 올려주면서 block당한 thread를 해제하는 방식으로 구성되어 있으며, waiters list 가장 첫 원소를 pop 하면서 sema_up을 실행시켜주게 된다.

__cond_broadcast__
```C
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}
```
condition에 있는 모든 waiters 에게 cond_signal을 통해 singal을 차례로 전파하며 모든 waiters를 block에서 해제하게 되는 방식으로 구성되어 있다.

### Timer - timer.h / timer.c
__timer_init__
```C
void
timer_init (void) 
{
  pit_configure_channel (0, 2, TIMER_FREQ);
  intr_register_ext (0x20, timer_interrupt, "8254 Timer");
```
타이머를 초기화시키는 함수로, pit_configure_channel과 intr_register_ext 두 함수를 호출한다. pit_configure_channel 함수 호출은 내부 진동자로 타이머 신호를 발생시키는 PIT(Programmable Interval Timer)의 0번 채널을 2번 모드와 TIMER_FREQ로 설정한다. x86 아키텍처에서 PIT의 0번 채널은 PIC(Programmable Interrupt Controller)의 interrupt line 0에 연결되어 있다. PIC는 이를 다시 interrupt number로 변환하여 CPU에 전달하고, CPU는 메모리의 IDT(Interrupt Descripter Table)을 참조하여 인터럽트 발생 시마다 타이머 인터럽트 핸들러를 호출한다.

intr_register_ext는 위에서 설명한 인터럽트 핸들러를 등록하는 함수이다. Interrupt number 0x20에 할당된 인터럽트의 인터럽트 핸들러를 timer_interrupt로 등록하여, 해당 인터럽트가 발생할 때마다 timer_interrupt가 호출되도록 한다.

__timer_calibrate__
```C
void
timer_calibrate (void) 
{
  unsigned high_bit, test_bit;

  ASSERT (intr_get_level () == INTR_ON);
  printf ("Calibrating timer...  ");

  /* Approximate loops_per_tick as the largest power-of-two
     still less than one timer tick. */
  loops_per_tick = 1u << 10;
  while (!too_many_loops (loops_per_tick << 1)) 
    {
      loops_per_tick <<= 1;
      ASSERT (loops_per_tick != 0);
    }

  /* Refine the next 8 bits of loops_per_tick. */
  high_bit = loops_per_tick;
  for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
    if (!too_many_loops (loops_per_tick | test_bit))
      loops_per_tick |= test_bit;

  printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}
```
실제 시간과 유사한 딜레이를 구현하기 위해, loops_per_tick을 초기화하는 함수이다. PIT에서 발생하는 인터럽트 신호는 1초에 TIMER_FREQ회, 즉 100Hz로 설정되어 있기 때문에 밀리세컨드 이하의 시간을 timer_ticks만으로 측정하기는 어렵다. 따라서, Pintos에서는 밀리세컨드 이하의 시간 단위를 위한 sleep을 위해서 먼저 1초에 아무것도 하지 않는 loop가 몇번 실행되는지 확인한다. 이후 sleep을 할 때는, 해당 측정에서와 같은 loop를 실행하도록 해 정밀한 sleep이 가능하게 한다. 이때 1초에 loop가 몇 번 실행되는지를 나타내는 변수가 loops_per_tick이며, 이를 초기화하는 함수가 timer_calibrate이다.

동작을 자세히 살펴보면, 먼저 loops_per_tick을 2^10으로 초기화한 후 이에 2씩 곱하며 too_many_loops가 실패하는 loops_per_tick 값을 찾는다. 이렇게 찾은 loops_per_tick은 아직 정확하지 않으므로, 1로 설정된 비트 아래 8개 비트도 하나씩 1로 설정하며 too_many_loops 함수를 실행시키고, 만약 함수 실행이 실패했다면 해당 비트도 1로 업데이트한다. 이렇게 만들어진 bit pattern이 loops_per_tick이 된다.

__timer_ticks__
```C
int64_t
timer_ticks (void) 
{
  enum intr_level old_level = intr_disable ();
  int64_t t = ticks;
  intr_set_level (old_level);
  return t;
}
```
현재 tick 값을 반환하는 함수이다.

__timer_elapsed__
```C
int64_t
timer_elapsed (int64_t then) 
{
  return timer_ticks () - then;
}
```
인자 then으로부터 지난 시간을 tick 단위로 반환하는 함수이다.

__timer_sleep__
```C
void
timer_sleep (int64_t ticks) 
{
  int64_t start = timer_ticks ();

  ASSERT (intr_get_level () == INTR_ON);
  while (timer_elapsed (start) < ticks) 
    thread_yield ();
}
```
인자 ticks만큼 현재 실행중인 thread의 실행을 멈추고, 다른 쓰레드에 실행 우선권을 넘겨주는 함수이다. timer_sleep의 원래 구현에서 주목할 점은 sleep의 시작 시간 start부터 지난 시간이 ticks보다 작은 동안 다른 thread에 실행 우선권을 넘겨주는 busy waiting 방식을 사용하고 있다는 점이다. 

Pintos의 현재 구현은 Round-Robin 방식을 사용하고 있어 thread_yield시 ready queue의 다음 thread가 실행되기 때문에 busy waiting을 하더라도 다른 thread의 실행이 가능하다. 하지만 만약 Priority Queue 기반 scheduling을 사용한다면, timer_sleep을 호출한 thread의 우선순위가 다른 thread들보다 높을 경우 timer_sleep을 호출한 thread가 sleep하는 동안 다른 thread들이 실행되지 못해 시스템 자원을 불필요하게 낭비하게 된다.

때문에 이번 과제에서는 Priority Queue 기반 scheduling을 구현하기에 앞서, 주어진 timer_sleep 구현을 busy waiting이 아닌 thread_block 기반으로 변경하여, sleep하는 thread가 불필요하게 다른 thread들의 실행을 막지 않도록 하는 것을 첫 번째 목표로 한다.

__timer_msleep__, __timer_usleep_, __timer__nsleep__
```C
void
timer_msleep (int64_t ms) 
{
  real_time_sleep (ms, 1000);
}
```
```C
void
timer_usleep (int64_t us) 
{
  real_time_sleep (us, 1000 * 1000);
}
```
```C
void
timer_nsleep (int64_t ns) 
{
  real_time_sleep (ns, 1000 * 1000 * 1000);
}
```
실제 시간에 기반하여 현재 thread가 밀리세컨드, 마이크로세컨드, 나노세컨드 단위로 sleep하도록 하는 함수이다.

```C
void
timer_mdelay (int64_t ms) 
{
  real_time_delay (ms, 1000);
}
```
```C
void
timer_udelay (int64_t us) 
{
  real_time_delay (us, 1000 * 1000);
}
```
```C
void
timer_ndelay (int64_t ns) 
{
  real_time_delay (ns, 1000 * 1000 * 1000);
}
```
실제 시간에 기반하여 현재 thread가 밀리세컨드, 마이크로세컨드, 나노세컨드 단위로 실행을 미루도록 하는 함수이다.

위에서 설명한 timer_msleep, timer_usleep, timer_nsleep 함수와 delay류 함수의 가장 큰 차이점은 sleep 함수들은 timer_sleep 함수를 이용하며 1틱 이하의 타이밍을 맞추기 위해서만 busy waiting을 이용하지만, delay 함수들은 timer_sleep 함수의 구현과 상관없이 무조건 busy waiting을 사용한다는 점이다. 때문에, 일반적인 프로그램에서 timer_mdelay, timer_udelay, timer_ndelay를 사용하는것은 비효율적이다.

__timer_print_stats__
```C
void
timer_print_stats (void) 
{
  printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}
```
타이머에 관련된 statistics를 출력하는 함수이다.

__timer_interrupt__
```C
static void
timer_interrupt (struct intr_frame *args UNUSED)
{
  ticks++;
  thread_tick ();
}
```
타이머 인터럽트 핸들러이다. PIT에 의해 발생하는 매 신호마다 호출되어 현재 ticks 값을 업데이트하고, thread_tick 함수를 호출하여 타이머 statistics를 업데이트하고 만약 현재 thread가 time slice로 할당된 시간 이상을 사용하였을경우 yield시킨다.

__too_many_loops__
```C
static bool
too_many_loops (unsigned loops) 
{
  /* Wait for a timer tick. */
  int64_t start = ticks;
  while (ticks == start)
    barrier ();

  /* Run LOOPS loops. */
  start = ticks;
  busy_wait (loops);

  /* If the tick count changed, we iterated too long. */
  barrier ();
  return start != ticks;
}
```
loops_per_tick값을 조율하기 위해 쓰이는 함수이다. 인자 loops만큼의 loop를 실행하여, 만약 해당 실행 이후에 ticks 값이 변하였다면 true를, ticks 값이 변하였다면 false를 반환한다. 달리 설명하자면, 인자로 넘어온 loop 횟수가 너무 많아 한 tick 내에 실행 불가능할 경우 true를, 아닌 경우 false를 반환하는 함수이다.

too_many_loops의 구현에서, barrior 매크로가 쓰인 것을 볼 수 있다. barrior는 컴파일러에 의한 순서 재배치(reordering), 코드 변경 혹은 삭제를 막는 코드이다. 첫 번째 while문을 보면, while의 조건을 검사하기 전에 start가 ticks로 초기화되기 때문에 컴파일러는 이를 infinite loop로 변경하려 한다. 하지만, 실제로는 while block이 실행되는 도중 타이머 인터럽트가 발생하여 ticks의 값이 변경될 수 있기 때문에 컴파일러가 이러한 최적화를 한다면 이는 틀린 코드가 된다. 따라서, 명시적으로 컴파일러가 코드를 변경하지 못하도록 barrior를 삽입하여 불필요하고 부정확한 최적화를 막는다.

__busy_wait__
```C
static void NO_INLINE
busy_wait (int64_t loops) 
{
  while (loops-- > 0)
    barrier ();
}
```
인자 loops만큼 반복하며 busy wait을 하는 함수이다. 이 함수에서 barrior는 컴파일러가 while loop의 body가 없어 while loop 자체를 삭제해버리지 않게 하기 위해 삽입되었다. 

__real_time_sleep__
```C
static void
real_time_sleep (int64_t num, int32_t denom) 
{
  /* Convert NUM/DENOM seconds into timer ticks, rounding down.
          
        (NUM / DENOM) s          
     ---------------------- = NUM * TIMER_FREQ / DENOM ticks. 
     1 s / TIMER_FREQ ticks
  */
  int64_t ticks = num * TIMER_FREQ / denom;

  ASSERT (intr_get_level () == INTR_ON);
  if (ticks > 0)
    {
      /* We're waiting for at least one full timer tick.  Use
         timer_sleep() because it will yield the CPU to other
         processes. */                
      timer_sleep (ticks); 
    }
  else 
    {
      /* Otherwise, use a busy-wait loop for more accurate
         sub-tick timing. */
      real_time_delay (num, denom); 
    }
}
```
인자 num을 denom으로 나눈 초 단위의 시간만큼 현재 thread를 sleep시키는 함수이다. timer_msleep, timer_usleep, timer_nsleep의 구현을 위해 쓰인다. real_time_sleep은 상술한 바와 같이, tick 단위의 sleep을 위해서는 timer_sleep을 이용하고 1 tick 이하의 타이밍 정확성을 위해서만 busy waiting을 이용한다. 현재의 구현에서는 timer_sleep 또한 busy waiting을 하기 때문에 큰 의미는 없지만, 만약 timer_sleep이 busy waiting이 아닌 좀 더 효율적인 방식을 이용한다면 아래의 real_time_delay보다 효율적일 것이다.

__real_time_delay__
```C
static void
real_time_delay (int64_t num, int32_t denom)
{
  /* Scale the numerator and denominator down by 1000 to avoid
     the possibility of overflow. */
  ASSERT (denom % 1000 == 0);
  busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000)); 
}
```
인자 num을 denom으로 나눈 초 단위의 시간만큼 현재 thread의 실행을 지연시키는 함수이다. timer_mdelay, timer_udelay, timer_ndelay의 구현을 위해 쓰인다. 상술한 바와 같이 busy waiting을 하기 때문에 비효율적이며, 특별한 경우가 아닌 이상 real_time_sleep과 이를 이용하는 timer_msleep, timer_usleep, timer_nsleep을 사용하는 것이 더 효율적이다.

## Design Description
### Alarm Clock


### Priority Scheduler & Priority Donation


### Advanced Scheduler
