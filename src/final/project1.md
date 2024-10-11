# Project 1 - Final Report (Team 28)
Project 1은 Alarm Clock, Priority Queue Scheduling, Multilevel Priority Queue Scheduling의 세 부분으로 나눌 수 있다. 본 보고서에서는 각 부분을 이전의 Pintos 구현과 비교하여 변화한 점을 서술하는 Improvement 단락, 구현의 세부사항과 설계상의 결정사항을 설명하는 Detail and Rationale 단락, 마지막으로 해당 부분에서 설계상의 난점, 이후 발전될 수 있는 사항, 현재 구현의 한계점 등을 논의하는 Discussion 단락으로 나누서 서술한다.

## Task 1 - Alarm Clock 
Pintos Project 1의 첫 번째 과제는 기존에 구현된 바쁜 대기(Busy waiting) 방식 `timer_sleep()`을 개선하는 것을 목표로 한다. 

### Improvement
기존의 `timer_sleep()`은 계속해서 반복문을 돌며 `start`로부터 현재까지 지난 틱을 측정하고, 만약 `start`로부터 지난 틱이 `ticks`보다 작다면 `thread_yield()`를 호출해 다른 스레드에 실행 흐름을 넘겨주는 방식으로 구현되어 있다. 이 방식은 round robin 방식 스케줄러에서는 `thread_yield()` 호출 시, 현재 스레드가 대기열(Ready queue)의 가장 끝에 추가되므로 비효율적이나 최소한 기능적으로는 잘 작동한다. 하지만, 우선순위 큐 기반 스케줄러에서는 만약 해당 스레드가 현재 대기열에 있는 다른 어떤 스레드보다 높은 우선순위를 가지고 있을 경우, `timer_sleep()`을 호출한 스레드만이 계속 실행되므로 심각한 기아(Starvation) 문제를 야기할 가능성이 있다.

이러한 문제를 해결하기 위해, 새로 구현된 `timer_sleep()`에서는 `set_alarm()`이라는 새로운 함수를 호출한다. `set_alarm()`은 인간이 알람을 맞추고 잠에 들듯이, `ticks` 이후에 울리는 알람을 알람 목록에 추가하는 함수이다. `timer_sleep()`이 호출되면, 이를 호출한 스레드는 `set_alarm()`을 이용해 알람을 설정하고, `thread_block()`을 이용해 잠에 든다. 

이렇게 잠에 든 스레드를 깨우는 일은 타이머 인터럽트 핸들러가 담당한다. Pintos는 타이머 모듈을 초기화하는 함수 `timer_init()`에서 `intr_register_ext()`를 이용해 인터럽트 벡터 0x20에 해당하는 인터럽트 핸들러로 `timer_interrupt()`를 등록한다. 이렇게 등록된 타이머 인터럽트 핸들러 `timer_interrupt()`는 PIC(Programmable Interrupt Controller)에 의해 0x20에 해당하는 인터럽트가 발생할 때마다 실행된다. `timer_interrupt()`의 기존 구현은 스레드 실행에 관한 통계치를 업데이트하고 `TIME_SLICE`가 지나면 운영체제가 실행 흐름을 선점하는 `thread_tick()` 함수만이 호출되는 형태로 구현되어 있다. 

`timer_interrupt()`의 새로운 구현은 `check_alarm()`함수 또한 호출한다. `check_alarm()`은 현재 설정된 알람들을 저장하는 리스트인 `alarm_list`를 순회하며 깨워야 할 스레드를 찾아내 `thread_unblock()`을 호출해 깨우는 것으로 구현되어 있다. 이후 이렇게 종료된 알람은 `alarm_list`에서 제거된다.

### Detail and Rationale
#### `struct alarm`
```C
struct alarm 
  {
    struct list_elem elem;      /* List element for alarm_list. */ 
    struct thread *t;           /* Thread to awaken. */
    int64_t start, ticks;       /* Start time and duration of alarm. */
  };
```
`alarm` 구조체는 `set_alarm()`에 의해 설정되는 하나의 알람을 나타내기 위한 구조체이다. 알람은 전역 리스트에 저장되고 확인되므로, 리스트에 추가하기 위해서는 `elem` 멤버를 가지고 있다. 또한 알람으로 깨울 스레드를 나타내는 `t`, 알람의 시작 시각과 스레드를 깨울 시각을 저장하기 위한 `start`, `ticks` 원소를 가지고 있다. 

#### `alarm_list`, `alarm_list_lock`
```C
/* List of alarms set currently. */
static struct list alarm_list;

/* Lock for manipulating alarm_list. */
static struct lock alarm_list_lock;
```
`alarm_list`와 `alarm_list_lock`은 알람들의 목록을 저장할 리스트와 이 리스트를 다룰 때 획득(Acquire)해야 하는 lock으로 구성되어 있다. 여기서 알람 리스트를 lock을 이용해 보호해야 하는 이유는 이후 `timer_sleep()`과 `set_alarm()` 함수를 다루며 설명할 예정이다. 

#### `timer_init()`
```C
/* Sets up the timer to interrupt TIMER_FREQ times per second,
   and registers the corresponding interrupt. */
void
timer_init (void) 
{
  pit_configure_channel (0, 2, TIMER_FREQ);
  intr_register_ext (0x20, timer_interrupt, "8254 Timer");

  list_init (&alarm_list);
  lock_init (&alarm_list_lock);
}
```
`timer_init()` 함수는 타이머 모듈을 초기화하는 함수로, Pintos 부팅 이후 `main()`의 초기화 과정 중 실행된다. 이전 구현과 비교하였을 때 `alarm_list`와 `alarm_list_lock`을 초기화하는 `list_init()`과 `lock_init()` 호출이 추가되었다는 차이점이 있다.

#### `timer_sleep()`
```C
void
timer_sleep (int64_t ticks) 
{
  ASSERT (intr_get_level () == INTR_ON);

  struct alarm *alarm = set_alarm (ticks);

  intr_disable ();
  thread_block ();
  free (alarm);
  intr_enable ();
}
```
`timer_sleep()` 함수는 기존의 바쁜 대기 기반 구현에서 벗어나 `thread_block()`을 이용하도록 구현되었다. 이때, 상술한 바과 같이 정지(Block) 상태인 스레드는 다른 스레드 혹은 인터럽트 핸들러에 의해 다시 실행되기 전까지 자력으로 다시 실행될 수 없으므로, `set_alarm()`을 호출해 알람을 설정한 후 타이머 인터럽트 핸들러에 의해 깨워지도록 구현되어 있다.

이때 주목할 점은 `timer_sleep()` 함수는 `set_alarm()`이 반환하는 알람의 주소를 현재 실행중인 스레드의 맥락(Context)안에 넣고 `thread_block()`을 호출한다는 사실이다. 즉, 알람을 설정하고 잠에 든 이후 일어났을 때 해당 알람에 대한 메모리를 해제할 책임은 `timer_sleep()`과 이를 호출한 스레드에 있다. `set_alarm()`은 알람을 설정할 때 `malloc()`을 이용한 동적 메모리 할당을 이용하며, 이렇게 할당된 메모리는 반드시 어디에선가 해제되어야 한다. 이러한 메모리 해제 작업은 알람을 확인하고 스레드를 깨우는 `check_alarm()` 함수에서 수행해도 기능상 문제는 없지만, 코드의 단순성과 가독성을 위해 이렇게 설계하였다. 

#### `timer_interrupt()`
```C
/* Timer interrupt handler. */
static void
timer_interrupt (struct intr_frame *args UNUSED)
{
  ticks++;

  check_alarm ();
  thread_tick ();
}
```
타이머 인터럽트 핸들러이다. 기존의 구현에 더해 알람 목록을 확인하고 알람으로 설정된 틱이 다 지난 스레드를 깨우는 `check_alarm()` 호출이 추가되었다.

#### `check_alarm()`
```C
/* Checks alarm lists, and rings alarm if there exists a thread that
   needs to be awakened. */
static void 
check_alarm (void)
{
  struct list_elem *e;
  struct alarm *cur;

  for (e = list_begin (&alarm_list); e != list_end (&alarm_list); )
    {
      cur = list_entry (e, struct alarm, elem);
      
      if (timer_elapsed (cur->start) >= cur->ticks) 
        {
          thread_unblock (cur->t);
          e = list_remove (e);
        }
      else
        {
          e = list_next (e);
        }
    }
}
```
타이머 인터럽트 핸들러에서 호출되어 알람 목록을 확인하고 대기 시간이 지난 스레드를 깨우는 함수이다. 상술한 `alarm_list`를 순회하며 알람 설정으로부터 지난 시간이 설정된 알람 시간보다 큰 스레드를 찾아 `thread_unblock()` 함수를 이용하여 깨우고, 해당 알람은 알람 목록에서 삭제한다.

`check_thread()`의 구현에서 주목할 점은 크게 두 가지이다. 첫째로, 몇몇 알람 시계 구현에서는 알람의 목록인 `alarm_list`, 혹은 이에 상응하는 역할을 하는 리스트를 *오름차순으로 정렬된 상태*로 저장하여, 이후 알람 목록 순회에 소요되는 시간을 줄이고자 한다. 이 구현에서는 그러한 접근을 채택하지 않았다. n개의 알람이 설정되고, 이후 n개의 알람이 울리는 경우를 생각해 보자. 

이때 현재의 구현은 알람 설정 (리스트 삽입)에 각각 상수 시간이 걸리므로 O(n), 모든 알람을 깨우는 데 O(n + (n - 1) + ... + 1) = O(n^2) 시간이 걸려 전체적으로 O(n^2) 시간이 걸린다. 알람을 삽입할 때 정렬된 상태를 유지할 수 있도록 리스트를 한 번 순회하는 경우 (Pintos의 `list_insert_ordered()` 구현은 리스트를 순회하여 새로이 삽입되는 원소가 들어갈 곳을 찾는다.), n개의 알람을 삽입하는 데 O(1 + 2 + ... + n) = O(n^2), n개의 알람을 울리는 데 O(n) 시간이 걸리므로 전체 시간복잡도는 같다.

알람 목록의 정렬을 유지하기 위해 알람을 삽입할 때 리스트를 순회하는 것이 아닌, 이진 탐색 알고리즘을 이용하거나 정렬 알고리즘을 이용하는 경우 삽입에 O(log1 + log2 + ... + logn) = O(nlogn) 시간이 걸린다고 주장할 수도 있을 것이다. 타당한 주장이며 실제로 n이 커질 경우 정렬 알고리즘을 이용하는 방식이 더 빠를 수도 있다. 하지만, 정렬 알고리즘을 이용하는 과정에서 함수 호출로 인해 생기는 오버헤드 또한 무시할 수 없으며 충분히 작은 n에 대해서는 추가적인 함수 호출이 없는 리스트 순회 알고리즘이 더 좋은 성능을 낼 것이라 생각할 수 있다.

두 번째 주목할 점은 `check_alarm()`에서는 `alarm_list`를 조작할 때 `alarm_list_lock`을 이용하지 않는다는 점이다. 이를 이해하기 위해서는 `check_alarm()`은 오직 인터럽트 맥락(Interrupt context) 내에서만 호출되는 함수라는 점을 이해해야 한다. 단일 프로세서 환경에서 공유 자원에 대한 일관성(Consistency) 문제가 일어나기 위해서는 선점형 스케줄러로 인해 다른 스레드로 실행 흐름이 넘어가야 한다. 이때 운영체제에 의한 프로세서 선점은 타이머 인터럽트에 의해서만 발생하고, 외부 인터럽트 실행 맥락에서 인터럽트는 비활성화된다. Pintos는 단일 프로세서(Uniprocessor) 환경에서 실행되는 운영체제이므로, 인터럽트 핸들러 내에서의 공유 자원 접근은 안전하다는 것을 알 수 있다. 무엇보다도, 인터럽트 핸들러 내에서는 함수 실행을 정지시킬 수 없기 때문에, lock을 획득하려 시도할 수 없다.

#### `set_alarm()`
```C
/* Sets an alarm that rings after TICKS, and awakens current thread. 
   */
static struct alarm *
set_alarm (int64_t ticks)
{
  struct thread *current = thread_current();
  struct alarm *new = (struct alarm *) malloc(sizeof(struct alarm));

  new->t = current;
  new->start = timer_ticks();
  new->ticks = ticks;

  lock_acquire (&alarm_list_lock);
  list_push_back (&alarm_list, &new->elem);
  lock_release (&alarm_list_lock);

  return new;
}
```
`thread_block()`을 호출해 정지 상태에 들어간 스레드를 깨우기 위한 알람을 설정하는 함수이다. 새로운 알람에 사용될 메모리를 동적 할당해 해당 알람을 알람 리스트의 맨 끝에 추가하도록 구현되어 있다.

알람을 구현하기 위한 방법으로 위와 같이 동적 할당을 이용할 수도 있고, `thread` 구조체에 `alarm_list`에 대응하는 `list_elem` 멤버와 알람이 울릴 tick을 저장하는 멤버를 추가하여 구현할 수도 있을 것이다. 동적 할당을 이용하는 구현은 필요할 때만 알람에 대한 정보를 저장할 메모리 공간을 힙 영역에 할당하므로 메모리에 효율적이지만, 메모리 동적 할당은 느린 작업이므로 시간에는 비효율적이다. 반대로 `thread` 구조체를 수정하는 방식은 알람을 설정하지 않은 스레드들도 알람에 대한 정보를 스레드 페이지 내에 저장해야 하므로 메모리에 비효율적이다. 현재 구현과 같은 방식을 채택한 이유는 `set_alarm()` 호출이 충분히 많지 않은 이상 상술한 메모리 동적 할당 지연시간 문제는 무시할 만하고, 반대로 `thread` 구조체의 크기가 늘어나는 것은 하나의 스레드에 4KiB의 메모리만이 할당되고 이 안에 스레드 스택, `thread` 구조체 모두를 저장하는 Pintos의 특성상 치명적이기 때문이다.

`set_alarm()`은 `check_alarm()`과는 다르게 알람 목록에 새로운 알람을 추가할 때 대응되는 lock을 먼저 획득한다. 이는 `set_alarm()`은 인터럽트 실행 맥락이 아닌 일반적인 실행 맥락에서도 호출될 수 있는 함수이기 때문이다. 따라서, `list_push_back()` 실행 도중 충분히 타이머 인터럽트에 의해 실행 흐름이 다른 스레드로 넘어갈 수 있으며, lock을 이용해 임계 구역의 실행을 통제하고 공유 자원을 보호해야 한다.

### Discussion
이번 과제에서 구현한 `timer_sleep()`은 기존의 바쁜 대기 방식에 비해 발전하였으나, 몇 가지 개선사항이 존재한다. 

TODO

## Task 2 - Priority Queue Scheduling 
### Improvement
TODO
### Detail and Rationale
TODO
### Discussion
TODO

## Task 3 - Multilevel Priority Queue Scheduling
### Improvement
TODO
### Detail and Rationale
TODO
### Discussion
TODO

## Overall Discussion
TODO