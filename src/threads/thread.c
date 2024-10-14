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
/** 1
 * thread.c에서 fp 연산을 할 수 있도록 fixed_point.h 파일을 include한다.
 */
#include "threads/fixed_point.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/** Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/** List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* list for sleep */
/** 1
 * 현재 핀토스에서 thread를 관리하는 list는 ready_list와 all_list 두 개만 존재한다.
 * 잠이 들어 block 상태가 된 thread들은 all_list에 존재하지만,
 * sleep state인 thread들만 보관하는 리스트를 만들어 관리한다.
 */
static struct list sleep_list; /* block된 스레드를 저장할 공간 */

/** List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/** Idle thread. */
static struct thread *idle_thread;

/** Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/** Lock used by allocate_tid(). */
static struct lock tid_lock;

/** Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /**< Return address. */
    thread_func *function;      /**< Function to call. */
    void *aux;                  /**< Auxiliary data for function. */
  };

/** Statistics. */
static long long idle_ticks;    /**< # of timer ticks spent idle. */
static long long kernel_ticks;  /**< # of timer ticks in kernel threads. */
static long long user_ticks;    /**< # of timer ticks in user programs. */

/** Scheduling. */
#define TIME_SLICE 4            /**< # of timer ticks to give each thread. */
static unsigned thread_ticks;   /**< # of timer ticks since last yield. */

/** If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

/**  advanced scheduler(mlfqs) 구현 */
/** 1
 * for advanced scheduler
 * 최근 1분 동안 수행 가능한 thread의 평균 개수를 나타내며, 실수 값을 가진다. <= 이 역시도 recent_cpu와 값이 지수 가중 평균이동으로 구한다.
 * priority, recent_cpu 값은 각 thread 별로 그 값을 가지는 반면 load_avg 는 system-wide 값으로 시스템 내에서 동일한 값을 가진다.
 * 매 1초마다 load_avg 값을 재계산한다.
 * LOAD_AVG_DEFAULT == 0
 * load_avg = (59/60) * load_avg + (1/60) * ready_threads이다.
 * 이때, ready_threads값은 업데이트 시점에 ready(running + ready to run) 상태의 스레드의 개수를 나타낸다.
 * 
 * 이 값이 크면 recent_cpu 값은 천천히 감소(priority는 천천히 증가)하고,
 * 이 값이 작으면 recent_cpu 값은 빠르게 감소(priority는 빠르게 증가)한다.
 * 
 * 왜냐하면, 수행 가능한 thread의 평균 개수가 많을 때(load_avg 값이 클때)는 
 * 모든 thread가 골고루 CPU time을 배분받을 수 있도록 이미 사용한 thread의 priority가 천천히 증가해야 한다.
 * 
 * 반대로, 수행 가능한 thread의 평균 개수가 적을 때(load_avg 값이 작을때)는
 * 조금 더 빠르게 증가해도 모든 thread가 골고루 CPU time을 배분받을 수 있기 때문이다.
 */
int load_avg;
/**  advanced scheduler(mlfqs) 구현 */

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/** Initializes the threading system by transforming the code
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
  list_init (&ready_list);
  list_init (&sleep_list); /* sleep_list를 사용할 수 있도록 sleep_list를 초기화하는 코드 */
  list_init (&all_list);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/** Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  // advanced scheduler(mlfqs) 구현
  load_avg = LOAD_AVG_DEFAULT;
  // 새롭게 추가한 변수를 초기화 하는 과정

  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started); // thread_create하는 순간 idle thread가 생성되고, 동시에 idle 함수가 실행된다.

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/** Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
/* ticks가 TIME_SLICE보다 커지는 순간에 intr_yield_on_return() 이라는 인터럽트가 실핸된다. */
/* 이는 running 중인 thread 에서 외부 개입에 의해 thread_yield()를 실행시킨다. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
	/** 1
	 * 이렇게 증가한 ticks가 TIME_SLICE보다 커지는 순간에 intr_yield_on_return()이라는 인터럽트가 실행된다.
	 * 이 인터럽트는 결과적으로 thread_yield()를 실행시킨다.
	 * 즉, 하나의 thread에서 scheduling 함수들이 호출되지 않더라도, time_interrupt에 의해서
	 * 일정 시간(ticks >= TIME_SLICE)마다 자동으로 scheduling이 발생한다.
	 */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/** Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/** Creates a new kernel thread named NAME with the given initial
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
  /** 1
	 * thread_unblock(), thread_yield, thread_create()의 경우 list_puch_back이 list_insert_ordered로 수정되어야 한다.
	 * thread_create()의 경우, 새로운 thread가 ready_list에 추가되지만, thread_unblock() 함수를 포함하기 때문에 unblock() 수정하면 얘도 같이 수정된다.
	 */
  thread_unblock (t);
  // for priority scheduleing (1)
  thread_cpu_acquire (); // runnning thread의 priority 변경으로 인한 priority 재확인

  return tid;
}

/** Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF); /* interrupt가 off된 상태로 block 되어야 하므로, 이를 확인한다. */

  thread_current ()->status = THREAD_BLOCKED;
  schedule (); // running thread가 CPU를 양보한다.
}

/** Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_BLOCKED);

  enum intr_level old_level;
  old_level = intr_disable ();

  // list_push_back (&ready_list, &t->elem); // list push back 함수는 round-robin 방식에서 elem을 list의 맨 뒤에 push 하는 함수이다.
  list_insert_ordered (&ready_list, &t->elem, thread_compare_priority, 0);
  t->status = THREAD_READY;

  intr_set_level (old_level);
}

/** Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/** Returns the running thread.
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
  ASSERT (is_thread (t)); /* t가 NULL 값이 아닌지 확인하는 작업 */
  ASSERT (t->status == THREAD_RUNNING); /* t가 실제로 running 되는 thread가 반환되었는지 확인하는 작업 */

  return t;
}

/** Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/** Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule (); // running thread가 CPU를 양보한다.
  NOT_REACHED ();
}

/** Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  ASSERT (!intr_context ());

  struct thread *cur = thread_current ();
  enum intr_level old_level;
  old_level = intr_disable ();

  if (cur != idle_thread) {
    // list_push_back (&ready_list, &cur->elem); 이는 round-robin 방식에 사용되는 단순 list_push_back() 함수이다.
    list_insert_ordered (&ready_list, &cur->elem, thread_compare_priority, 0);
  }
  cur->status = THREAD_READY;
  schedule (); // running thread가 CPU를 양보한다.

  intr_set_level (old_level);
}

/** Invoke function 'func' on all threads, passing along 'aux'.
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

/** Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  /** advanced scheduler (mlfqs) 구현 */
  // mlfqs scheduler 에서는 priority를 임의로 변경할 수 없기 때문에, thread_set_priority() 함수 역시 비활성화 시켜야 한다.
  if (thread_mlfqs) {
    return;
  }
  /** advanced scheduler (mlfqs) 구현 */
  else {
    /**  priority inversion(donation) 구현 */
    // 만약, 현재 진행중인 running thread의 priority 변경이 일어났을 때,
    // donations list들에 있는 thread들보다 priority가 높아지는 경우가 생길 수 있다.
    // 이 경우, priority는 donations list 중 가장 높은 priority가 아니라, 새로 바뀐 priority가 적용될 수 있게 해야 한다.
    // 이는 thread_set_priority에 refresh_priority() 함수를 추가하는 것으로 간단하게 가능하다.
    // thread_current ()->priority = new_priority; 원래 존재하던 코드 priority -> init_priority로 변경.
    thread_current ()->init_priority = new_priority;
    refresh_priority ();
    /**  priority inversion(donation) 구현 */

    /**  alarm clock 구현. */
    thread_cpu_acquire (); // runnning thread의 priority 변경으로 인한 priority 재확인
    /**  alarm clock 구현. */
  }
}

/** Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/** advanced scheduler (mlfqs) 구현 */
/** 1
 * 각 값들을 변경할 시에는 interrupt의 방해를 받지 않도록 interrupt를 비활성화 해야 한다.
 * 1. void thread_set_nice (int);
 * 2. int thread_get_nice (void);
 * 3. int thread_get_load_avg (void);
 * 4. thread_get_recent_cpu (void);
 */

/** Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) 
{ // 현재 thread의 nice 값을 새 값으로 설정한다.
  enum intr_level old_level;
  old_level = intr_disable (); // 각 값들을 변경할 시에는 interrupt를 비활성화 해야한다.

  struct thread *cur = thread_current();
  cur->nice = nice;
  mlfqs_calculate_priority (cur);
  thread_cpu_acquire ();

  intr_set_level (old_level); // 다시 interrupt를 활성화 해준다.
  return;
}

/** Returns the current thread's nice value. */
int
thread_get_nice (void) 
{ // 현재 thread의 nice 값을 반환한다.
  enum intr_level old_level;
  old_level = intr_disable ();

  struct thread *cur = thread_current();
  int nice = cur-> nice;

  intr_set_level (old_level);
  return nice;
}

/** Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{ // 현재 system의 load_avg * 100 값을 반환한다.
  enum intr_level old_level;
  old_level = intr_disable ();

  // pintos document의 지시대로 100을 곱한 후 정수형으로 만들고 반올림하여 반환한다.
  // 정수형 반환값에서 소수점 둘째 자리까지의 값을 확인할 수 있도록 하는 용도이다.
  int load_avg_value = FP_TO_INT_ROUND (MULT_MIXED (load_avg, 100));

  intr_set_level (old_level);
  return load_avg_value;
}

/** Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{ // 현재 thread의 recent_cpu * 100 값을 반환한다.
  enum intr_level old_level;
  old_level = intr_disable ();
  // pintos document의 지시대로 100을 곱한 후 정수형으로 만들고 반올림하여 반환한다.
  // 정수형 반환값에서 소수점 둘째 자리까지의 값을 확인할 수 있도록 하는 용도이다.
  int recent_cpu = FP_TO_INT_ROUND (MULT_MIXED (thread_current ()-> recent_cpu, 100));
  
  intr_set_level (old_level);
  return recent_cpu;
}
/** advanced scheduler (mlfqs) 구현 */


/** Idle thread.  Executes when no other thread is ready to run.

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
/** 1
 * idle thread는 한 번 schedule을 받고, 바로 sema_up을 하여 thread_start()의 마지막 sema_down을 풀어준다.
 * thread_start가 작업을 끝내고 run_action()이 실행될 수 있도록 해주고, idle 자신은 block 된다.
 * idle thread는 pintos에서 실행 가능한 thread가 하나도 없을 때, wake 되어 다시 작동하는데,
 * 이는 CPU가 무조건 하나의 thread 는 실행하고 있는 상태를 만들기 위함이다.
 * => 아마 껐다 키는데 소모되는 자원보다 하나를 실행하고 있는 상태에서 소모되는 자원이 더 적기 때문일듯?
 */
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

/** Function used as the basis for a kernel thread. */
/** 1
 * thread_func *function은 이 kernel이 실행할 함수를 가리킨다.
 * void *aux는 보조 파라미터로, synchronization을 위한 semaphore 등이 들어온다.
 * 여기서 실행시키는 function은 이 thread가 종료될 때까지 실행되는 main 함수이다.
 * 즉, 이 function은 idle thread라고 불리는 thread를 하나 실행시키는데,
 * 이 idle thread는 하나의 c 프로그램에서 하나의 main 함수 안에서 여러 함수의 호출이 이루어지는 것처럼,
 * pintos kernel위에서 여러 thread들이 동시에 실행될 수 있도록 하는 단 하나의 main thread인 셈이다.
 * pintos의 목적은, 이러한 idle thread 위에 여러 thread들이 동시에 실행될 수 있도록 만드는 것이다.
 */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /**< The scheduler runs with interrupts off. */
  function (aux);       /**< Execute the thread function. */
  thread_exit ();       /**< If function() returns, kill the thread. */
}

/** Returns the running thread. */
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

/** Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/** Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  enum intr_level old_level;

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;

  // priority inversion(donation)
  t->init_priority = priority;
  t->wait_on_lock = NULL;
  list_init (&t->donations);
  // 새롭게 추가한 요소를 초기화 하는 과정

  // advanced scheduler(mlfqs) 
  t->nice = NICE_DEFAULT;
  t->recent_cpu = RECENT_CPU_DEFAULT;
  // 새롭게 추가한 요소를 초기화 하는 과정

  t->magic = THREAD_MAGIC;

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/** Allocates a SIZE-byte frame at the top of thread T's stack and
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

/** Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    // 현재 pintos는 ready_list에 push는 맨 뒤에, pop은 맵 앞에서 하는 round-robin 방식을 채액하고 있다.
    // 이러한 방식은 thread 간의 우선순위 없이 ready_list에 들어온 순서대로 실행하여 간단하지만, 제대로 된 우선순위 스케줄링은 일어나지 않고 있다.
    // 따라서, 이하에서는 pintos가 제대로 우선순위 스케줄링을 하도록 만든다. 2가지 방법이 존재한다.
    //(1) ready_list에 push할 때 priority 순서에 맞추어 push하는 방법
    //(2) ready_list에서 pop할 때 prioirty가 가장 높은 thread를 찾아 pop하는 방법
    //전자는 항상 ready_list가 정렬된 상태를 유지하기 때문에 새로운 thread를 push할 때 list의 끝까지 모두 확인할 필요가 없다.
    //후자는 ready_list가 정렬된 상태를 유지하지 않으므로 pop할 때마다 list의 처음부터 끝까지 모두 확인해야 한다.
    //전자의 방식이 더 효율적이므로 이를 따르도록 한다.
    //그렇다면, 바꾸어야 하는 함수는 list_push_back이 실행되는 thread_unblock(), thread_yield() 함수이다.

    /* thread_create()함수에서도 새로운 thread가 ready_list에 추가되지만, thread_create() 함수 자체에서
       thread_unblock() 함수를 포함하고 있기 때문에 thread_unblock() 함수를 수정함으로써 동시에 처리된다. */
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
    /** 1
	    * 반환값을 보면, !list_empty일 때는, list_pop_front (&ready_list)를 하고 있다.
	    * 즉, ready_list의 맨 앞 항목을 반환하는 round-robin 방식을 채택하고 있다는 것을 알 수 있다.
	    * 이러한 방식은 우선순위 없이 ready_list에 들어온 순서대로 실행하여 가장 간단하지만,
	    * 제대로 된 우선순위 스케쥴링이 이루어지고 있지 않다고 할 수 있다.
	    * 이를 유지시키면서 priority scheduling을 구현할 수 있도록 -> ready_list에 push()할 때, priority 순서에 맞추어 push 하도록 한다.
	    */
}

/** Completes a thread switch by activating the new thread's page
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

/** Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  ASSERT (intr_get_level () == INTR_OFF); /* scheduling 도중에는 inturrupt가 발생하면 안되기에 INTR_OFF 인지 확인한다. */
  
  struct thread *cur = running_thread (); /* 현재 실행중인 thread A를 반환한다. */
  struct thread *next = next_thread_to_run (); /* thread B (ready queue에서 다음에 실행될 thread를 반환함) */
  struct thread *prev = NULL; /* thread A가 CPU의 소유권을 thread B에게 넘겨준 후 thread A를 가리키는 포인터 */

  ASSERT (cur->status != THREAD_RUNNING); /* CPU의 소유권을 넘겨주기 전에 running thread는 그 상태를 running 외로 변경헸어야 하고, 이를 확인한다. */
  ASSERT (is_thread (next)); /* next_thread_to_run()에 의해 올바른 next thread가 return 되었는지 확인한다. */

  if (cur != next)
    /* thread/switch.S에 assembly 언어로 작성되어 있다. 핵심 함수이다. */
    /* CPU 점유를 thread A에서 thread B로 넘기고, cur thread 였던 thread A의 주소가 반환되어 prev에 삽입된다. */
    prev = switch_threads (cur, next); 
  thread_schedule_tail (prev);
}

/** Returns a tid to use for a new thread. */
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


/* new functions below */

/** 1
 * 일어날 시간을 저장한 다음에 재워야 할 스레드를 sleep_list에 추가하고,
 * 스레드 상태를 block state로 만들어 준다.
 * CPU가 항상 실행 상태를 유지하게 하기 위해서 idle 스레드는 sleep되지 않아야 한다.
 */
void
thread_sleep (int64_t wakeup_time)
{
  enum intr_level old_level;
  old_level = intr_disable (); // interrupt off & get previous interrupt state(INTR_ON maybe)
  thread_update_wakeuptime (wakeup_time);
  intr_set_level (old_level); // interrupt on
}

/** 1
 * timer_sleep() 함수가 호출되면 thread가 block state로 들어간다.
 * 이렇게 block된 thread들은 일어날 시간이 되었을 때 awake 되어야 한다.
 * 1. sleep_list()를 돌면서 일어날 시간이 지난 thread들을 찾아서 ready_list로 옮겨주고,
 * 2. thread state를 ready state로 변경시킨다.
 */
void
thread_awake (int64_t ticks)
{
  enum intr_level old_level;
  old_level = intr_disable();

  struct list_elem *e = list_begin (&sleep_list);

  /* sleep_list를 돌면서 일어날 시간이 지난 thread들을 찾아서 ready list로 옮겨주고, thread 상태를 ready state로 변경시킨다. */
  while (e != list_end (&sleep_list)){
    struct thread *t = list_entry (e, struct thread, elem);
    /* thread e가 일어날 시간이 되었는지 확인한다. */
    if (t->wakeup_time <= ticks){
      e = list_remove (e); // 일어날 시간이 되었다면 sleep_list에서 제거한다.
      thread_unblock(t); // 그 후, thread를 unblock한다.
    } 
    else 
      break;
  }

  intr_set_level(old_level);
}

void thread_update_wakeuptime(int64_t wakeup_time) {
  struct thread *cur = thread_current ();
  ASSERT (cur != idle_thread); // CPU가 항상 실행 상태를 유지하게 하기 위해 idle thread는 sleep되지 않아야 한다.
  cur->wakeup_time = wakeup_time; // 현재 running 중인 thread A가 일어날 시간을 저장
  list_insert_ordered (&sleep_list, &cur->elem, thread_compare_wakeuptime, NULL); // sleep_list wakeup_time에 따라 오름차순으로 추가한다.
  thread_block (); //thread A를 block 상태로 변경한다.
}

// alarm clock 구현
bool
thread_compare_wakeuptime (const struct list_elem *l, const struct list_elem *s, void *aux UNUSED) {
  struct thread *prev = list_entry (l, struct thread, elem);
  struct thread *next = list_entry (s, struct thread, elem);

  if(prev->wakeup_time < next->wakeup_time) return true;
  else return false;
}

// priority schedulder(1) 구현
// list_insert_ordered(struct list *list, struct list_elem *elem, list_less_func *less, void *aux)
// list_insert(struct list_elem *before, struct list_elem *elem) 해당 함수는 elem을 e의 '앞에' 삽입한다.
// 우리는 ready_list에서 thread를 pop할 때 가장 앞에서 꺼내기로 하였다.
// 따라서, 가장 앞에 priority가 가장 높은 thread가 와야 한다.
// 즉, ready_list는 내림차순으로 정렬되어야 한다.
// if(less (elem, e, aux))가 elem > e인 순간에 break; 를 해주어야 한다.
// 즉, 우리가 만들어야 하는 order 비교함수 less(elem, e, aux)는 elem > e일 때 true를 반환하는 함수이다.
bool
thread_compare_priority (const struct list_elem *l, const struct list_elem *s, void *aux UNUSED) {
  struct thread *prev = list_entry (l, struct thread, elem);
  struct thread *next = list_entry (s, struct thread, elem);

  if(prev->priority > next->priority) return true;
  else return false;
}

// priority inversion(donation) 구현
// thread_compare_donate_priority 함수는 thread_compare_priority 의 역할을 donation_elem 에 대하여 하는 함수이다. 
bool
thread_compare_donate_priority (const struct list_elem *l, const struct list_elem *s, void *aux UNUSED) {
  struct thread *prev = list_entry (l, struct thread, donation_elem);
  struct thread *next = list_entry (s, struct thread, donation_elem);

  if(prev->priority > next->priority) return true;
  else return false;
}

// priority schedulder(1) 구현
// 현재 실행중인 running thread의 priority가 바뀌는 순간이 있다.
// 이때 바뀐 priority가 ready_list의 가장 높은 priority보다 낮다면 CPU 점유를 넘겨주어야 한다.
// 현재 실행중인 thread의 priority가 바뀌는 순간은 두 가지 경우이다.
// (1) thread_create() -- thread가 새로 생성되어서 ready_list에 추가된다.
// (2) thread_set_priority() -- 현재 실행중인 thread의 우선순위가 재조정된다.
// 두 경우의 마지막에 running thread와 ready_list의 가장 앞의 thread의 priority를 비교하는 코드를 넣어주어야 한다.
// running thread와 ready_list의 가장 앞 thread의 priority를 비교하고,
// 만약 ready_list의 thread가 더 높은 priority를 가진다면 thread_yield()를 실행하여 CPU의 점유권을 넘겨준다.
// 이 함수를 (1), (2)에 추가한다.
void
thread_cpu_acquire (void)
{
  if (list_empty (&ready_list)) return;
  else {
  struct thread* thread_tester = list_entry (list_front (&ready_list), struct thread, elem);
  // priority1 < priority2 라면, priority2의 우선순위가 더 높음을 의미한다. 또한 이것이 list의 맨 앞에 추가된다.
    if(thread_current ()->priority > thread_tester->priority) return;
    else thread_yield();
  }
}

// priority inversion(donation) 구현
// 자신의 priority를 필요한 lock을 점유하고 있는 thread에게 빌려주는 함수이다.
// 주의할 점은, nested donation을 위해 하위에 연결된 모든 thread에 donation이 일어나야 한다는 것이다.
void priority_donation (void) {
  struct thread *current_thread = thread_current();
  int max_depth = 8; // nested의 최대 깊이를 지정해주기 위해 사용한다. max_depth == 8

  priority_donation_recursive(current_thread, max_depth, 0);
}

void priority_donation_recursive (struct thread *cur, int max_depth, int level) {
  if (level >= max_depth || cur->wait_on_lock == NULL) { 
    return;
  }
  struct thread *lock_holder = cur->wait_on_lock->holder;
  if (lock_holder == NULL) { // thread의 wait_on_lock이 NULL이라면 더이상 donation을 진행할 필요가 없으므로 멈춘다.
    return;
  }
/** 1
  * cur->wait_on_lock이 NULL이면 요청한 해당 lock을 acquire()할 수 있다는 말이다.
  * 그게 아니라면, 스레드가 lock에 걸려있다는 말이므로, 그 lock을 점유하고 있는 holder thread에게 priority를 넘겨주는 방식을
  * 최대 깊이 8의 스레드까지 반복한다.
  */
  lock_holder->priority = cur->priority;
  priority_donation_recursive(lock_holder, max_depth, level + 1);
}

// priority inversion(donation) 구현
/** 1
 * thread curr에서 lock_release(B)를 수행한다.
 * lock B를 사용하기 위해서 curr에 priority를 나누어 주었던 thread H는 priority를 빌려줘야 할 이유가 없다.
 * donations list에서 thread H를 지워주어야 한다.
 * 그 후, thread H가 빌려주었던 priority를 지우고, 다음 priority로 재설정(refresh)해야 한다.
 */
void
remove_with_lock (struct lock *lock)
{
  struct list_elem *e;
  struct thread *cur = thread_current ();

  for (e = list_begin (&cur->donations); e != list_end (&cur->donations); e = list_next (e)){
    struct thread *t = list_entry (e, struct thread, donation_elem);
    if (t->wait_on_lock == lock) // wait_on_lock이 이번에 release하는 lock이라면, 해당 thread를 donation list에서 지운다.
      list_remove (&t->donation_elem); // 모든 donations list와 관련된 작업에서는 elem이 아니라, donation_elem을 사용한다.
  }
}

// priority inversion(donation) 구현
/** 1
 * init_priority와 donations list의 max_priority중 더 높은 값으로 curr->priority를 설정한다.
 * 1. lock_release ()를 실행하였을 경우, curr thread의 priority를 재설정(refresh)해야 한다.
 * 2. thread_set_priority ()에서 활용한다.
 */
void
refresh_priority (void)
{
  struct thread *cur = thread_current ();

  cur->priority = cur->init_priority; // donations list가 비어있을 때는 cur thread에 init_priority를 삽입해준다.

  // 허나, donations list에 thread가 남아있다면 thread 중에서 가장 높은 priority를 가져와서 삽입해야한다.
  if (!list_empty (&cur->donations)) { // list_empty()라면, cur->priority = cur->init_priority 하면 끝임.
    list_sort (&cur->donations, thread_compare_donate_priority, 0); // list_sort()는 priority가 가장 높은 thread를 고르기 위해 priority를 기준으로 내림차순 정렬한다. 

    // 그 후, 맨 앞의 thread(우선순위가 가장 큰)를 가져온다.
    struct thread *front = list_entry (list_front (&cur->donations), struct thread, donation_elem);
    // init_priority와 비교하여 더 큰 priority를 삽입한다.
    if (front->priority > cur->priority) // 만일, front->priority > cur->priority라면,
      cur->priority = front->priority; // priority donation을 수행한다.
  }
}

/********** ********** ********** project 1 : advanced scheduler ********** ********** **********/
/********** ********** ********** project 1 : advanced scheduler ********** ********** **********/
/********** ********** ********** project 1 : advanced scheduler ********** ********** **********/
/** 1
 * 4BSD scheduler priority를 0 (PRI_MIN)부터 63(PRI_MAX)의 64개의 값으로 나눈다.
 * 각 priority 별로 ready queue가 존재하므로 64개의 ready queue가 존재하며,
 * priority 값이 커질수록 우선순위가 높아짐(먼저 실행됨)을 의미한다.
 * => 다만, 구현의 한계로 인해 64개의 multiple-queue는 두지않고, priority를 재계산하기만 했다.
 * 
 * thread의 priority는 thread 생성 시에 초기화되고, 4 ticks의 시간이 흐를 때마다 모든 thread의 priority가 재계산된다.
 * priority의 값을 계산하는 식은 아래와 같다.
 * priority = PRI_MAX - (recent_cpu / 4) - (nice * 2)
 * 
 * recent_cpu 값은 이 thread가 최근에 cpu를 얼마나 사용하였는지를 나타내는 값으로, thread가 최근에 사용한 cpu 양이 많을수록 큰 값을 가진다.
 * 이는 오래된 thread 일수록 우선순위를 높게 가져서(recent_cpu 값이 작아짐) 모든 thread들이 골고루 실행될 수 있게 한다.
 * priority는 정수값을 가져야 하므로, 계산 시 소수점은 버림한다.
 */

// advanced scheduler(mlfqs) 구현
// mlfqs_caculate_priority 함수는 priority를 계산한다.
// idle_thread의 priority는 고정이므로 제외하고, fixed_point.h에서 만든 fp 연산 함수를 사용하여 priority를 구한다.
// 계산 결과의 소수점 부분은 버림하고, 정수의 priority로 설정한다.
void
mlfqs_calculate_priority (struct thread *t)
{
  if (t != idle_thread) {
    t->priority = FP_TO_INT_ZERO (ADD_MIXED (DIV_MIXED (t->recent_cpu, -4), PRI_MAX - t->nice * 2));
  }
  else
    return;
}

// mlfqs_calculate_recent_cpu 함수는 특정 thread의 priority를 계산하는 함수이다.
void mlfqs_calculate_recent_cpu (struct thread *t)
{
  if (t != idle_thread) {
  t->recent_cpu = ADD_MIXED (MULT_FP (DIV_FP (MULT_MIXED (load_avg, 2), 
      ADD_MIXED (MULT_MIXED (load_avg, 2), 1)), t->recent_cpu), t->nice);
  }
  else
    return;
}

/** 1
 * load_avg 값을 계산하는 함수이다.
 * load_avg 값은 thread 고유의 값이 아니라 system wide한 값이기 때문에, idle_thread가 실행되는 경우에도 계산한다.
 * ready_threads는 현재 시점에서 실행 가능한 thread의 수를 나타내므로,
 * ready_list에 들어 있는 thread의 숫자에 현재 running thread 1개를 더한다.
 * (이때, idle thread는 실행 가능한 thread에 포함시키지 않는다.)
 */
void
mlfqs_calculate_load_avg (void)
{
  int ready_threads;

// ready_threads는 현재 시점에서 실행 가능한 thread의 수를 나타내므로,
// ready_list에 들어있는 thread의 숫자에 현재 running thread 1개를 더한다.
// idle thread는 실행 가능한 thread에 포함시키지 않는다.
  if (thread_current () == idle_thread) {
    ready_threads = list_size (&ready_list);
  }
  else {
    ready_threads = list_size (&ready_list) + 1;
  }

  load_avg = ADD_FP (MULT_FP (DIV_FP (INT_TO_FP (59), INT_TO_FP (60)), load_avg), 
                     MULT_MIXED (DIV_FP (INT_TO_FP (1), INT_TO_FP (60)), ready_threads));
}

/** 1 Advanced Scheduler */
// 각 값들이 변하는 시점에 수행할 함수를 만든다. 값들이 변화하는 시점은 3가지가 있다.
// (1) 1 tick마다 running thread의 recent_cpu 값 + 1
// (2) 4 tick마다 모든 thread의 priority 값 재계산
// (3) 1 초마다 모든 thread의 recent_cpu값과 load_avg 값 재계산 
// 아래의 mlfqs_increment_recent_cpu, mlfqs_recalculate_recent_cpu, mlfqs_recalculate_priority 함수를
// 해당하는 시간 주기마다 실행되도록 timer_interrupt 함수를 바꾸어주면 된다. -> <devices/timer.c> -> timer_interrupt ()

// 현재 thread의 recent_cpu 값을 1 증가시키는 함수이다.
void
mlfqs_increment_recent_cpu (void)
{
  if(thread_current () == idle_thread)
    return;
  else
    thread_current ()->recent_cpu = ADD_MIXED (thread_current ()->recent_cpu, 1);
} 

// 모든 thread의 recent_cpu를 재계산하는 함수이다.
void
mlfqs_recalculate_recent_cpu (void)
{
  struct list_elem *e;

  for (e = list_begin (&all_list); e != list_end (&all_list); e = list_next (e)) {
    struct thread *t = list_entry (e, struct thread, allelem);
    mlfqs_calculate_recent_cpu (t);
  }
}

// 모든 thread의 priority를 재계산하는 함수이다.
void
mlfqs_recalculate_priority (void)
{
  struct list_elem *e;

  for (e = list_begin (&all_list); e != list_end (&all_list); e = list_next (e)) {
    struct thread *t = list_entry (e, struct thread, allelem);
    mlfqs_calculate_priority (t);
  }
}
/** 1 Advanced Scheduler */

/** Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
/* 이때 stack 포인터는 kernel stack이 아니라, TCB 안에 있는 *stack을 의미한다. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);

