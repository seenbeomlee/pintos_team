#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/* include 추가 */
#include <debug.h>
#include <stdint.h>

/** A counting semaphore. */
// 하나의 공유자원을 사용하기 위해 여러 thread가 sema_down 되어 대기하고 있다고 해보자.
// 이 공유자원을 사용하던 thread가 공유자원의 사용을 마치고 sema_up 할 때, 
// 어떤 thread가 가장 먼저 이 공유자원을 차지할 것인지에 대해 scheduling에서 해결해야 한다.
struct semaphore 
  {
    unsigned value;             /**< Current value. 공유 자원의 개수를 나타낸다. */
    struct list waiters;        /**< List of waiting threads. 공유 자원을 사용하기 위해 대기하는 waiters의 리스트이다. */
  };

void sema_init (struct semaphore *, unsigned value);
void sema_down (struct semaphore *);
bool sema_try_down (struct semaphore *);
void sema_up (struct semaphore *);
void sema_self_test (void);

/** Lock. value = 1인 특별한 semaphore로 구현되어 있다. */
struct lock 
  {
    struct thread *holder;      /**< Thread holding lock (for debugging). */
    struct semaphore semaphore; /**< Binary semaphore controlling access. */
  };

void lock_init (struct lock *);
void lock_acquire (struct lock *);
bool lock_try_acquire (struct lock *);
void lock_release (struct lock *);
bool lock_held_by_current_thread (const struct lock *);

/** Condition variable. */
struct condition 
  {
    struct list waiters;        /**< List of waiting threads. condition variables가 만족하기를 기다리는 waitersd의 list를 가진다.*/
  };

void cond_init (struct condition *);
void cond_wait (struct condition *, struct lock *);
void cond_signal (struct condition *, struct lock *);
void cond_broadcast (struct condition *, struct lock *);

/* new functions below */

// priority scheduler (2)
bool sema_compare_priority (const struct list_elem *l, const struct list_elem *s, void *aux UNUSED);

/** Optimization barrier.

   The compiler will not reorder operations across an
   optimization barrier.  See "Optimization Barriers" in the
   reference guide for more information.*/
#define barrier() asm volatile ("" : : : "memory")

#endif /**< threads/synch.h */
