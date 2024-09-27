## Semaphore
```C
struct semaphore 
  {
    unsigned value;        
    struct list waiters;
  };
```

각각의 Critical Section 에 하나의 Thread 만 접근 가능하도록 막아주는 역할을 하는 struct 로, value의 값을 통해 0보다 크면 접근 가능, 0 이면 접근이 불가능하도록 설정하며, 이외에도 기다리고 있는 Thread를 저장하는 waiters list를 함께 갖고 있다.

### sema_init
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

### sema_down
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

### sema_try_down
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

### sema_up
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

## Lock
```C
struct lock 
  {
    struct thread *holder;      
    struct semaphore semaphore; 
  };
```
Lock 도 Semaphore의 역할과 유사하게, 각각의 공유되고 있는 critical section에 여러 thread가 접근하지 못하게 막는 역할을 수행하고 있으며, lock의 경우 현재 점유하고 있는 thread를 저장하는 holder와 해당 critical section 에 따른 semaphore를 함께 저장하고 있는 구조로 설정되어 있다.

### lock_init
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


### lock_acquire
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

### lock_try_acquire
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

### lock_release
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

### lock_held_by_current_thread
```C
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}
```
lock이 현재 thread에게 점유되고 있는지를 확인하는 함수로 lock의 holder와 thread_current()를 통해 반환받은 현재 thread를 비교하여 여부를 알려주는 방식으로 구성되어 있다.

## Condition
```C
struct condition 
  {
    struct list waiters;        
  };
```
Condition은 특정 상황 (ex 비어있는 리스트 등)으로 인해 semaphore나 lock 을 갖고 있는 상황이지만, 더 이상 수행할 수 없을 때 condition을 통해서 다른 thread와의 실행을 관리하게 된다. struct는 보다 단순하게 해당 condition을 기다리는 waiters를 저장하는 list로만 이뤄져 있다.

### struct sempahore_elem
```C
struct semaphore_elem 
  {
    struct list_elem elem;             
    struct semaphore semaphore;        
  };
```
Condition의 waiters에 저장될 semaphore 를 list로 관리하기 위해서 고안된 struct 로 semaphore를 linked list에서 사용하기 위해서 정의되었다.

### cond_init
```C
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}
```
condition을 초기화하는 함수로, waiters list를 초기화화하는 역할을 수행한다.


### cond_wait
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

### cond_signal
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

### cond_broadcast
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