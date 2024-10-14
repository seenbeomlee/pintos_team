#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>

/** States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /**< Running thread. */
    THREAD_READY,       /**< Not running but ready to run. */
    THREAD_BLOCKED,     /**< Waiting for an event to trigger. */
    THREAD_DYING        /**< About to be destroyed. */
  };

/** Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /**< Error value for tid_t. */

/** Thread priorities. */
#define PRI_MIN 0                       /**< Lowest priority. */
#define PRI_DEFAULT 31                  /**< Default priority. */
#define PRI_MAX 63                      /**< Highest priority. */

/** advanced scheduler(mlfqs) 구현 */             
#define NICE_DEFAULT 0
#define RECENT_CPU_DEFAULT 0
#define LOAD_AVG_DEFAULT 0

/** A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0). 이를 TCB라고 부른다.
   The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+ // 아래로는 TCB (thread control block) 영역이다.
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/** The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */

struct thread /* TCB 영역의 구성 */
  {
    /* Owned by thread.c. */
    tid_t tid;                          /**< Thread identifier. */
    enum thread_status status;          /**< Thread state. */
    char name[16];                      /**< Name (for debugging purposes). */
    uint8_t *stack;                     /**< Saved stack pointer. */
   /* *stack 포인터가 중요하다. running중인 thread A에서 scheduling이 일어나면, 현재 CPU가 실행중이던 상태,
   즉 현재 CPU의 Register들에 저장된 값들을? 값들의 포인터를 아닌가? 이 *stack에 저장한다. 후에 다시 thread A가
   실행되는 순간에 이 stack이 다시 CPU의 Register로 로드되어 실행을 이어갈 수 있게 된다. 
   */
    int priority;                       /**< Priority. */
    struct list_elem allelem;           /**< List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /**< List element. */


#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /**< Page directory. */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /**< Detects stack overflow. */

   /* new codes below */
  /** 1 alarm clock
	 * thread가 잠이 들 때, ready state가 아니라, block state로 보내고, 깨어날 시간이 되면 깨워서 ready state로 보낸다.
	 * 우선, block state에서는 스레드가 일어날 시간이 되었는지 매 tick 마다 주기적으로 확인하지 않기 때문에,
	 * thread마다 일어나야 하는 시간에 대한 정보를 저장하고 있어야 한다.
	 * wakeup_tick 이라는 변수를 만들어 thread 구조체에 추가한다.
	 */
    int64_t wakeup_time; /* block된 스레드가 꺠어나야 할 tick을 저장한 변수 추가 */

/** 1 priority inversion(donation)
 * priority inversion을 해결하기 위해 priority donation을 구현해야 한다.
 * 1. multiple donation
 * 2. nested donation
 * 을 모두 구현해야 한다.
 */
  int init_priority; // thread가 priority를 양도받았다가 다시 반납할 때 원래의 priority를 복원할 수 있도록 고유의 값 저장하는 변수

  // thread가 현재 얻기 위해 기다리고 있는 lock으로, thread는 이 lock이 release 되기를 기다린다.
  // 즉, thread B가 얻기 위해 기다리는 lock을 현재 보유한 thread A에게 자신의 priority를 주는 것이므로, 이를 받은 thread A의 donations에 thread B가 기록된다.
  struct lock *wait_on_lock; 

  struct list donations; // 자신에게 priority를 나누어진 thread들의 list. 왜 thread들이냐면, Multiple donation 때문이다.
  struct list_elem donation_elem; // 이 list를 관리하기 위한 element로, thread 구조체의 elem과 구분하여 사용한다.

/** 1 advanced scheduler(mlfqs) 구현
 * 각 스레드는 이 스레드가 다른 스레드에 대해 어떠한 성질을 가지는지를 나타내는 정수의 nice 값을 가진다.
 * 이 값은 다른 thread에게 자신의 CPU time을 얼마나 잘 양보하는지를 나타낸다.
 * -20 <= nice value <= 20의 값을 가진다.
 * 1. nice value < 0 값이 작을수록 자신의 CPU time을 다른 thread에게 양보하는 정도가 크다.
 * 2. nice value == 0 thread의 priority에 영향을 미치지 않는다.
 * 3. nice value > 0 값이 클수록 다른 thread의 CPU time을 더 많이 빼앗아 온다.
 */
   int nice;
/** 1 advanced scheduler(mlfqs) 구현
 * thread가 최근에 얼마나 많은 CPU time을 사용했는지를 나타내는 실수의 recent_cpu 값을 가진다.
 * 이 값이 클수록 최근에 더 많은 CPU를 사용했음을 의미하므로, 우선순위(priority)는 낮아진다.
 * RECENT_CPU_DEFAULT : 0
 * 매 tick마다 running thread의 recent_cpu 값이 1만큼 증가한다. (idle thread가 아닐 때)
 * 매 1초마다 모든 thread의 recent_cpu 값을 재계산해야 한다.
 * 
 * 지수 가중 이동평균 방식에 의해서 구할 수 있다.
 * recent_cpu == (2 * load_avg) / (2 * load_avg + 1) * recent_cpu + nice
 */
   int recent_cpu;
  };

// for lab4 Advanced Scheduler
/** If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
/** 1
 * thread_mlfqs가 true일 때는 advanced scheduler를,
 * false일 때는 기존의 priority scheduler를 사용하도록 구현한다.
 * 
 * 4BSD 스케줄러는 multilevel feedback queue scheduler의 구성을 갖는다.
 * 이러한 방식에서 ready queue가 여러개 존재하고, 각각의 ready queue는 서로 다른 priority를 갖는다.
 * 실행중이던 thread가 실행을 끝내면, 스케줄러는 가장 priority가 높음 ready queue에서부터 하나의 thread를 꺼내와 실행시키고,
 * 해당 ready queue에 여러 thread가 잇는 경우 'round-robin' 방식으로 thread들을 실행시킨다. (우선순위 없이 FIFO)
 */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/** Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

/* 추가한 함수 목록 */

/** alarm clock */
void thread_sleep(int64_t ticks);
void thread_awake(int64_t ticks);

/** priority scheduler (1) */
// 여기에 const struct list_elem *l, const struct list_elem *s 해야 할 수도 있을 듯.
bool thread_compare_priority (const struct list_elem *l, const struct list_elem *s, void *aux UNUSED);
void thread_test_preemption (void);

/**  priority inversion(donation) */
/** 1
 * thread_compare_donate_priority 함수는 thread_compare_priority 의 역할을 donation_elem 에 대하여 하는 함수이다.
 */
bool thread_compare_donate_priority (const struct list_elem *l, const struct list_elem *s, void *aux UNUSED);
void donate_priority (void);
void remove_with_lock (struct lock *lock);
void refresh_priority (void);

/********** ********** ********** project 1 : advanced scheduler ********** ********** **********/
/********** ********** ********** project 1 : advanced scheduler ********** ********** **********/
/********** ********** ********** project 1 : advanced scheduler ********** ********** **********/
void mlfqs_calculate_priority (struct thread *t);
void mlfqs_calculate_recent_cpu (struct thread *t);
void mlfqs_calculate_load_avg (void);

/** 1
 * 1. 1 tick 마다 running thread의 recent_cpu 값 + 1
 * 2. 4 ticks 마다 모든 thread의 priority 재계산
 * 3. 1초 마다 모든 thread의 recent_cpu 값과 load_avg 값 재계산
 * 각 함수를 해당하는 시간 주기마다 실행되도록 timer_interrupt ()를 바꾸어주면 된다.
 */
void mlfqs_increment_recent_cpu (void);
void mlfqs_recalculate_recent_cpu (void);
void mlfqs_recalculate_priority (void);

#endif /**< threads/thread.h */
