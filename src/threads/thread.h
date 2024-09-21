#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0). -> 0부터 정의되는 아래 블록을 thread 고유의 정보를 담은 TCB라고 부른다.
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
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread /* Owned by thread.c. TCB 영역의 구성 */
  {
    /* tid_t tid
      Thread의 thread identifier 혹은 tid를 의미한다.
      모든 threads에는 커널의 전체 수명 동안 고유한 tid가 있어야 한다.
      기본적으로 tid_t는 int에 대한 typedef이며,
      각 새 thread는 initial process의 1부터 시작하여 
      numerically next higher한 tid를 받는다.
      원한다면 type과 Numbering을 변경할 수 있다. -> 허나 하지 않아도 project 3까지 이상없음 */
    tid_t tid;                          /* Thread identifier. */
    /* thread state
      thread의 state를 나타내며, RUNNING, READY, BLOCKED 중 하나이다. 
         project 1의 alarm clock에서 sleep_state를 추가하여 Round-Robin 대신 scheduling을 수행하게 된다.
      THREAD_RUNNING : pintos는 single process를 지원한다. 따라서, 특정 시간에 정확히 하나의 thread가 실행되고 있다.
         thread_current()가 running thread를 리턴한다.
      THREAD_READY : 다음에 스케줄러가 호출될 때 실행되도록 thread를 선택할 수 있다.
         ready threads는 ready_list라고 하는 doubly linked list에 보관된다.
      THREAD_BLOCKED : thread is waiting for somthing, like a lock or an interrupt to be invoked. 
      THREAD_DYING : thread will be destroyed by the scheduler after switching to the next thread. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    /* stack
      모든 thread에는 state를 추적할 수 있는 stack이 있다.
      thread A가 running 중일 때, CPU의 스택 포인터 레지스터(rsp)가 stack의 상단을 추적하고, 이 멤버는 사용되지 않는다.
      그러나, CPU가 다른 thread B로 전환되면 thread A의 TCB에 있는 이 멤버는 thread A의 스택 포인터를 저장한다.
      thread B가 실행되다가 다시 thread A가 실행되는 순간에 이 stack이 다시 CPU의 Register로 로드되어 실행을 이어갈 수 있게 된다. 
      저장해야 하는 다른 레지스터는 스택에 저장되기 때문에 thread의 레지스터를 저장하는데에 다른 멤버는 필요하지 않다. */
    uint8_t *stack;                     /* Saved stack pointer. */
    /* priority
      thread의 우선순위, pintos에서는 PRI_MIN(0)부터~ PRI_MAX(63)을 나타낸다. 
      project 1의 priority scheduling 이후에는 priority를 활용하게 된다. */
    int priority;                       /* Priority. */
    /* allelem
      thread를 the list of all threads에 연결하는 데 사용된다.
      각 thread는 생성쇨 때 이 목록에 삽입되고 종료될 때 제거된다.
      thread_foreach() 함수를 활용하여 모든 thread에 대해 iterate할 수 있다. */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG // for project 2
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif

    /* Owned by thread.c. -> 건드릴 필요가 없다. */
    /* magic
      항상 magic == THREAD_MAGIC으로 설정하면 된다.
      이것은 threads/thread.c에 정의되어 stack overflow를 감지하는 데 사용되는 임의의 번호일 뿐이다.
      thread_current()는 running thread의 magic member가 THREAD_MAGIC 인지 체크한다. */
    unsigned magic;                     /* Detects stack overflow. */

/* ********** ********** ********** project 1 ********** ********** ********** */
/* add new element */

};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". 
  
   thread_mlfqs가 true일 때는 advanced scheduler를,
   false일 때는 기존의 priority scheduler를 사용하도록 구현한다.
 
   4BSD 스케줄러는 multilevel feedback queue scheduler의 구성을 갖는다.
   이러한 방식에서 ready queue가 여러 개 존재하고, 각각의 ready queue는 서로 다른 priority를 갖는다.
   실행중이던 thread가 실행을 끝내면, 스케줄러는 가장 priority가 높음 ready queue에서부터 하나의 thread를 꺼내와 실행시키고,
   해당 ready queue에 여러 thread가 잇는 경우 'round-robin' 방식으로 thread들을 실행시킨다. (우선순위 없이 FIFO) 
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

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

/* ********** ********** ********** project 1 ********** ********** ********** */
/* add new function */

#endif /* threads/thread.h */
