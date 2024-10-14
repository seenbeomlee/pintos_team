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
#define PRI_MAX 63               
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
             +---------------------------------+
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
    // alarm clock
    int64_t wakeup_time; /* block된 스레드가 꺠어나야 할 tick을 저장한 변수 추가 */

    // priority inversion(donation)
    int init_priority; // thread가 priority를 양도받았다가 다시 반납할 때 원래의 priority를 복원할 수 있도록 고유의 값 저장하는 변수

    // thread가 현재 얻기 위해 기다리고 있는 lock으로, thread는 이 lock이 release 되기를 기다린다.
    // 즉, thread B가 얻기 위해 기다리는 lock을 현재 보유한 thread A에게 자신의 priority를 주는 것이므로, 이를 받은 thread A의 donations에 thread B가 기록된다.
    struct lock *wait_on_lock; 

    struct list donations; // 자신에게 priority를 나누어진 thread들의 list. 왜 thread들이냐면, Multiple donation 때문이다.
    struct list_elem donation_elem; // 이 list를 관리하기 위한 element로, thread 구조체의 elem과 구분하여 사용한다.

   // advanced scheduler(mlfqs) 구현
   int nice;
   int recent_cpu;
  };

// for lab4 Advanced Scheduler
/** If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
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
void update_next_tick_to_awake(int64_t ticks);
int64_t get_next_tick_to_awake(void);

/** priority scheduler (1) */
// 여기에 const struct list_elem *l, const struct list_elem *s 해야 할 수도 있을 듯.
bool thread_compare_priority (const struct list_elem *l, const struct list_elem *s, void *aux UNUSED);
void thread_test_preemption (void);

/**  priority inversion(donation) */
bool thread_compare_donate_priority (const struct list_elem *l, const struct list_elem *s, void *aux UNUSED);
void donate_priority (void);
void remove_with_lock (struct lock *lock);
void refresh_priority (void);

/** advanced_scheduler (mlfqs) */
void mlfqs_calculate_priority (struct thread *t);
void mlfqs_calculate_recent_cpu (struct thread *t);
void mlfqs_calculate_load_avg (void);
void mlfqs_increment_recent_cpu (void);
void mlfqs_recalculate_recent_cpu (void);
void mlfqs_recalculate_priority (void);

#endif /**< threads/thread.h */
