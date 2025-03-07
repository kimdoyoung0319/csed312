#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/fixed.h"
#include "devices/timer.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes that
   are ready to run but not actually running.  This list is not used
   when kernel uses multilevel feedback queue scheduling. */
static struct list ready_list;

/* Array of lists that holds processes in THREAD_READY state with 
   certain priority.  This is used instead of ready_list when the 
   kernel uses multilevel feedback queue scheduling. */
static struct list mlfqs_queues[PRI_MAX + 1];

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Stack to save donation list. */
struct thread_donation_list
  {
    struct list_elem elem;
    struct thread *donorelem;
    struct thread *donee;
    int priority;
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multilevel feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

/* Current system's averge load. Used to calculate recent CPU usage 
   of a thread. */
fixed load_avg;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static struct thread *mlfqs_next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);
static int max_priority (void);
static int mlfqs_max_priority (void);
static void mlfqs_update (void);
static void mlfqs_organize (void);
static void organize_single_mlfqs_queue (int priority);
static void update_priorities (void);
static void update_recent_cpu (void);
static void update_load_avg (void);
static int calculate_priority (struct thread *);
static fixed calculate_recent_cpu (struct thread *t);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&all_list);

  if (thread_mlfqs)
      for (int priority = 0; priority < PRI_MAX + 1; priority++)
        list_init (&mlfqs_queues[priority]);
  else
    list_init (&ready_list);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  load_avg = 0;

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->process != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();

  /* Routines for multilevel feedback queue scheduling. */
  if (thread_mlfqs) 
    mlfqs_update ();

  if (thread_mlfqs && timer_ticks () % TIME_SLICE == 0) {
    intr_yield_on_return ();
    mlfqs_organize ();
  }
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
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

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /* Add to run queue. */
  thread_unblock (t);
  thread_check ();

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);

  if (thread_mlfqs)
    list_push_back (&mlfqs_queues[t->priority], &t->elem);
  else
    list_push_back (&ready_list, &t->elem);

  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. It does not destory its associated process, hence
   it might be dangling. Therefore, it should not be called when there's an
   associated user process of current thread. Use process_exit() instead. */
void
thread_exit (void) 
{
  struct thread *cur = thread_current ();

  ASSERT (!intr_context ());
  ASSERT (cur->process == NULL);

  intr_disable ();

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  list_remove (&cur->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU. The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  cur->status = THREAD_READY;

  if (thread_mlfqs && cur != idle_thread)
    list_push_back (&mlfqs_queues[cur->priority], &cur->elem);
  else if (cur != idle_thread)
    list_push_back (&ready_list, &cur->elem);

  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
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

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  struct list_elem *e;
  struct thread *t, *cur = thread_current ();

  cur->priority = new_priority;
  cur->original = new_priority;

  if (thread_mlfqs) 
    return;

  /* New priority should be ignored when current thread has been
     donated from someone else, and the donation is greater than
     new one. */
  for (e = list_begin (&cur->donors); e != list_end (&cur->donors); 
       e = list_next (e))
    {
      t = list_entry (e, struct thread, donorelem);

      if (t->priority > cur->priority)
        cur->priority = t->priority;
    }
  
  thread_check ();
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice) 
{
  struct thread *cur = thread_current ();

  cur->nice = nice < MAX_NICE ? (nice > MIN_NICE ? nice : MIN_NICE) : MAX_NICE;

  if (cur != idle_thread)
    cur->priority = calculate_priority (cur);

  thread_check ();
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  return thread_current ()->nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  enum intr_level old_level = intr_disable ();
  int result = x_to_n_near (mul_x_n (load_avg, 100));
  intr_set_level (old_level);

  return result;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  struct thread *cur = thread_current ();
  int result = x_to_n_near (mul_x_n (cur->recent_cpu, 100));

  return result;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
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
  t->magic = THREAD_MAGIC;

  /* Initialize mlfqs variables. */
  t->nice = 0;
  t->recent_cpu = (fixed) 0;

  /* PRIORITY should be ignored when thread_mlfqs enabled. If so, 
     calculate initial priority based on initial nice and
     recent_cpu. */
  if (thread_mlfqs)
    t->priority = calculate_priority (t);
  else
    t->priority = priority;
  
  /* Initialize list to manage donators and store original priority value. */
  list_init (&t->donors);
  t->original = t->priority;

#ifdef USERPROG
  /* Process is nonexist until start_process() is called on this thread. */
  t->process = NULL;
#endif

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled. Should
   return a thread from the run queue, unless the run queue is
   empty. (If the running thread can continue running, then it
   will be in the run queue.) If the run queue is empty, return
   idle_thread. This function must not be used when the kernel
   uses multilevel feedback queue scheduling. If so, use
   mlfqs_next_thread_to_run() instead. */
static struct thread *
next_thread_to_run (void) 
{
  ASSERT (!thread_mlfqs);

  if (list_empty (&ready_list))
    return idle_thread;

  struct list_elem *max_e = list_max (&ready_list, thread_compare, NULL);
  struct thread *max_t = list_entry (max_e, struct thread, elem);

  list_remove (max_e);
  return max_t;
}

/* Multilevel feedback queue scheduler version of 
   next_thread_to_run(). Returns thread that has highest priority 
   among those in feedback queues. If there exist more than one 
   threads having highest priority, returns thread that were in ready
   queue the longest. If all feedback queues are empty, return idle 
   thread. */
static struct thread *
mlfqs_next_thread_to_run (void)
{
  struct list_elem *e = NULL;

  for (int priority = PRI_MAX; priority >= 0; priority--)
    {
      if (list_empty (&mlfqs_queues[priority]))
        continue;
      
      e = list_pop_front (&mlfqs_queues[priority]);
      break;
    }
  
  return e ? list_entry (e, struct thread, elem) : idle_thread;
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new thread.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *next;
  struct thread *prev = NULL;
  struct thread *cur = running_thread ();

  if (thread_mlfqs)
    next = mlfqs_next_thread_to_run ();
  else
    next = next_thread_to_run ();

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
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

/* Checks ready queue, and yields current thread if thread with 
   maximum priority has higher priority than current thread. */
void
thread_check (void)
{
  int max;
  struct thread *cur = thread_current ();

  if (thread_mlfqs)
    max = mlfqs_max_priority ();
  else
    max = max_priority ();

  /* Note that a thread cannot yield in external interrupt handling context. */
  if (max > cur->priority && intr_context ())
    intr_yield_on_return ();
  else if (max > cur->priority)
    thread_yield ();
}

/* Donates priority of current thread to T, in current depth of DEPTH. 
   Notice that this function may be called recursively, donating 
   current thread's priority to all other threads holding the locks 
   required for current thread. */
void
thread_donate (struct thread *t, int depth)
{
  t->priority = thread_current ()->priority;

  if (depth >= MAX_DONATION_DEPTH || t->waiting == NULL)
    return;

  struct lock *waiting = t->waiting;
  thread_donate (waiting->holder, depth + 1);
}

/* Restores donated priority of current thread, caused by acquisition
   of LOCK. It first deletes all donators that donated its priority 
   to current thread because of LOCK from current thread's donorelem 
   list. Also, it restores current thread's priority into the maximum
   among those remaining in the donorelem list and previous priority of
   current thread. */
void
thread_restore (struct lock *lock)
{
  struct list_elem *e;
  struct thread *t, *max, *cur = thread_current ();
  struct list *li = &(cur->donors);

  if (list_empty(li))
    {
      cur->priority = cur->original;
      return;
    }

  for (e = list_begin (li); e != list_end (li); )
    {
      t = list_entry (e, struct thread, donorelem);

      if (t->waiting != NULL && t->waiting == lock)
        e = list_remove (e);
      else
        e = list_next (e);
    }

  max = list_entry (list_max (li, thread_compare, NULL), struct thread, 
                    donorelem);

  if (max->priority > cur->original)
    cur->priority = max->priority;
  else
    cur->priority = cur->original;
}

/* Compare two list elements that belong to thread according to their 
   priorities, and return true if thread A's priority is less than B's. */
bool 
thread_compare (const struct list_elem *a, const struct list_elem *b,
                void *aux UNUSED)
{
  struct thread *ta = list_entry (a, struct thread, elem);
  struct thread *tb = list_entry (b, struct thread, elem);

  return ta->priority < tb->priority;
}

/* Returns maximum priority among threads in current ready_list.
   If ready_list is empty, returns -1. Notice that this function
   must not be used when the kernel uses multilevel feedback queue
   scheduling. Use mlfqs_max_priority() instead. */
static int
max_priority (void)
{
  ASSERT (!thread_mlfqs);

  if (list_empty (&ready_list))
    return -1;
  
  struct thread *max = list_entry (list_max (&ready_list, thread_compare, NULL), 
                                   struct thread, elem);
  return max->priority;
}

/* Multilevel feedback queue scheduler version of max_priority(). */
static int
mlfqs_max_priority (void)
{
  ASSERT (thread_mlfqs);

  int max = PRI_MAX;
  for ( ; max >= 0 && list_empty(&mlfqs_queues[max]); max--);

  return max;
}

/* Updates statistics that are needed for multilevel feedback queue 
   scheduling. This function should be called on every ticks by timer 
   interrupt handler. 
   
   Notice that this function must called only when the kernel uses
   multilevel feedback queue scheduling, i.e. when thread_mlfqs is 
   enabled. */
static void
mlfqs_update (void)
{
  ASSERT (thread_mlfqs);

  struct thread *cur = thread_current ();
  cur->recent_cpu = add_x_n (cur->recent_cpu, 1);

  if (timer_ticks () % TIME_SLICE == 0)
    update_priorities ();
  
  if (timer_ticks () % TIMER_FREQ == 0) 
    {
      update_recent_cpu ();
      update_load_avg ();
    }
}

/* Organizes threads into ready queues according to their priorities.
   This function must be called only when the kernel uses multilevel 
   feedback queue scheduling. */
static void
mlfqs_organize (void)
{
  ASSERT (thread_mlfqs);

  for (int priority = 0; priority < PRI_MAX + 1; priority++)
    organize_single_mlfqs_queue (priority);
}

/* Checks single ready queue that is of PRIORITY. If a thread is 
   not in the queue that has same priority with the thread's priority,
   then organizes it into the queue that has same priority. */
static void
organize_single_mlfqs_queue (int priority)
{
  struct thread *t;
  struct list_elem *e, *next;
  struct list *queue = &mlfqs_queues[priority];

  for (e = list_begin (queue); e != list_end (queue); )
    {
      t = list_entry (e, struct thread, elem);

      /* Notice that the next element of E, i.e. NEXT, must be saved
         since QUEUE is manipluated while iterating it. */
      if (t->priority != priority)
        {
          next = list_remove (e);
          list_push_back (&mlfqs_queues[t->priority], e);
        }
      else
        next = list_next (e);
      
      e = next;
    }
}

/* Updates priorities for all threads in every state. This function must be
   called only when thread_mlfqs enabled. */
static void 
update_priorities (void) 
{
  ASSERT (thread_mlfqs);

  struct thread *t;
  struct list_elem *e;

  for (e = list_begin (&all_list); e != list_end (&all_list); e = list_next (e))
    {
      t = list_entry (e, struct thread, allelem);
      t->priority = calculate_priority (t);
    }
}

/* Updates recent_cpu for all threads in every states. */
static void
update_recent_cpu (void)
{
  struct thread *t;
  struct list_elem *e;

  for (e = list_begin (&all_list); e != list_end (&all_list); e = list_next (e))
    {
      t = list_entry (e, struct thread, allelem);
      t->recent_cpu = calculate_recent_cpu (t);
    }
}

/* Updates load_avg value according to the number of the threads 
   that are currently either running or ready, and not idle thread. */
static void
update_load_avg (void)
{
  struct thread *t;
  struct list_elem *e;
  int ready_threads = 0;

  for (e = list_begin (&all_list); e != list_end (&all_list); e = list_next (e))
    {
      t = list_entry (e, struct thread, allelem);

      if (t != idle_thread 
          && (t->status == THREAD_READY || t->status == THREAD_RUNNING))
          ready_threads++;
    }
    
  /* load_avg = (59/60)*load_avg + (1/60)*ready_threads */
  load_avg = add_x_y (mul_x_y (div_x_y (n_to_x (59), n_to_x (60)), load_avg),
                      div_x_n (n_to_x (ready_threads), 60));
}

/* Calculates new priority of thread T according to its recent_cpu 
   and nice. This function cuts result when it is out of boundary 
   [PRI_MIN, PRI_MAX]. */
static int
calculate_priority (struct thread *t) {

  /* result = PRI_MAX - (recent_cpu / 4) - (nice * 2)
     This result is truncated to nearest integer. */
  int result = x_to_n_near (sub_x_n (sub_x_y (n_to_x (PRI_MAX), 
                            div_x_n (t->recent_cpu, 4)), 2*(t->nice)));

  return result > PRI_MAX ? PRI_MAX : (result < PRI_MIN ? PRI_MIN : result);
}

/* Calculates new recent_cpu value of thread T according to current 
   load_avg. */
static fixed
calculate_recent_cpu (struct thread *t) 
{
  fixed load_avg_doubled = mul_x_n (load_avg, 2);

  /* result = (2*load_avg)/(2*load_avg + 1) * recent_cpu + nice */
  return add_x_n (mul_x_y (div_x_y (load_avg_doubled,
                  add_x_n (load_avg_doubled, 1)), t->recent_cpu), t->nice);
}


/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
