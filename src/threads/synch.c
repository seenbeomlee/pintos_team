/** This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/** Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/** Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/** Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
  // 공유자원을 사용하고자 하는 thread는 sema_down을 실행한다.
  // 사용가능한 공유자원이 없는 sema->value == 0인 상태라면, 
    {
      // 기존의 코드는 sema->waiters list에 list_push_back() 함수로 맨 뒤에 넣는다.
      // list_push_back (&sema->waiters, &thread_current ()->elem); 
      list_insert_ordered (&sema->waiters, &thread_current ()->elem, thread_compare_priority, 0);
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}

/** Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
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

/** Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (!list_empty (&sema->waiters)) {
    // waiters list에 있던 동안 우선순위에 변경이 생겼을 수도 있으므로, waiters list를 내림차순으로 정렬하여 준다.
    list_sort (&sema->waiters, thread_compare_priority, 0);
    // 공유자원의 사용을 마친 thread가 sema_up을 하면 thread_unblock을 하는데,
    // list_pop_front 함수로 sema->waiters list의 맨 앞에서 꺼낸다.
    thread_unblock (list_entry (list_pop_front (&sema->waiters),
                                struct thread, elem));
  }
  sema->value++;
  // unblock된 thread가 running thread보다 우선순위가 높을 수 있으므로, CPU 선점이 일어나게 해준다.
  thread_test_preemption ();
  intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/** Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/** Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/** Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);
}

/** Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
// lock은 value == 1이고, holder 정보를 가지고 있다는 것을 제외하고는 semaphore와 동일하게 동작한다.
// 다만, lock은 semaphore와 달리, acquire를 호출한 thread만이 해당 lock을 다시 release할 수 있다는 점에서 다르다.
// lock_acquire function은 thread가 lock을 요청할 때 실행된다.
// lock을 현재 점유하고 있는 thread가 없다면 상관없지만, 
// 누군가 점유하고 있다면 자신의 prioirty를 양도하여 lock을 점유하고 있는 thread가 우선적으로 lock을 반환하도록 해야 한다.
void
lock_acquire (struct lock *lock) // lock을 양도받고 싶어하는 thread가 호출하는 함수이다.
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));
  
  /**  priority donation 구현 */
  // sema_down에 들어가기 전에 lock을 가지고 있는 thread에게 priority를 양도하는 작업이 필요하다.
  struct thread *cur = thread_current ();

  // lock->holder는 현재 lock을 소유하고 있는 thread를 가리킨다.
  // lock_acquire()을 요청하는 thread가 실행되고 있다는 자체가 이미 lock을 가지고 있는 thread보다 우선순위가 높다는 뜻이기 때문에,
  // if(cur->priority > lock->holder->priority) 등의 비교 조건은 필요하지 않다.
  if (lock->holder) { 
    cur->wait_on_lock = lock; // lock_acquire를 호출한 현재 thread의 wait_on_lock에 lock을 추가한다.
    list_insert_ordered (&lock->holder->donations, &cur->donation_elem,
                        thread_compare_donate_priority, 0); // lock->holder의 donations list에 현재 thread를 추가한다.
    if (!thread_mlfqs) {  /**  advanced scheduler (mlfqs) 구현 */
      // priority donation은 mlfqs scheduler에서는 사용하지 않는다.
      // 왜냐하면, 시간에 따라 priority가 재조정되기 때문이다.
      donate_priority (); 
    }
  }

  sema_down (&lock->semaphore); // lock에 대한 요청이 들어오면, sema_down에서 일단 멈췄다가,
  // lock->holder = thread_current (); // 기존 코드는 lock이 사용가능하게 되면 자신이 다시 lock을 선점한다.

  cur->wait_on_lock = NULL; // lock을 점유했으니 wait_on_lock에서 제거
  lock->holder = cur;
 /**  priority donation 구현 */

}

/** Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
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

/** Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
// lock은 value == 1이고, holder 정보를 가지고 있다는 것을 제외하고는 semaphore와 동일하게 동작한다.
// lock_release 함수는, semaphore와 달리, lock_acquire()을 호출한 thread만이 호출할 수 있다는 제약이 존재한다는 점에서 다르다.
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  /** priority inversion(donations) 구현 */
  // lock->holder = NULL;
  // sema_up (&lock->semaphore);
  // 현재(위에 두줄 있는 코드가 원래 코드)는 lock이 가진 holder를 비워주고, sema_up하는 것이 전부이다.
  // sema_up하여 lock의 점유를 반환하기 전에,
  // 이 lock을 사용하기 위해 나에게 priority를 빌려준 thread들을 donations list에서 제거하고,
  // priority를 재설정 해주는 작업이 필요하다.
  if (!thread_mlfqs) { /** advanced scheduler (mlfqs) 구현 */
     // priority donation은 mlfqs에서는 비활성화 한다.
    // 왜냐하면, mlfqs scheduler는 시간에 따라 priority가 재조정되기 때문이다.
    remove_with_lock (lock);
    refresh_priority ();
  }
  /** priority inversion(donations) 구현 */

  // 아래는 original code
  lock->holder = NULL;
  sema_up (&lock->semaphore);
}

/** Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/** One semaphore in a list. */
struct semaphore_elem 
  {
    struct list_elem elem;              /**< List element. */
    struct semaphore semaphore;         /**< This semaphore. */
  };

/** Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/** Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  // semaphore는 waiters가 thread들의 list 였지만,
  // condition vaiables의 waiters는 semaphore들의 list 이다.
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);
  // conditions variables에 묶여있는 여러 semaphore들의 list 중에서 가장 우선순위가 높음 하나의 semaphore를 깨워야 한다.
  // 이미 각 semaphore의 waiters list는 위의 semephore 함수에서 내림차순으로 정렬되게 하였으므로,
  // 각 semaphore의 waiters list의 맨 앞의 element가 각 semaphore에서 가장 우선순위가 큰 thread이다.
  // 따라서, 이들을 비교하여 가장 큰 우선순위를 갖는 thread를 가진 semaphore를 깨우면 된다.
  // 이번에도 역시 list_push_back 함수를 list_inserted_ordered로 바꾸는 것이 전부이다, 비교함수를 설정해야 한다.
  // list_push_back (&cond->waiters, &waiter.elem);

  // list_push_back 대신에 list_inserted_ordered 함수에 sema_compare_priority를 사용해서
  // 가장 높은 우선순위를 가진 thread가 묶여있는 semaphore가 가장 앞으로 오도록 내림차순으로 cond->waiters list에 push 한다.
  list_insert_ordered (&cond->waiters, &waiter.elem, sema_compare_priority, 0);
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/** If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters)) 
  {
    // 앞선 경우와 마찬가지로, pop을 그대로 하되, wait 도중에 우선순위가 바뀌었을 수 있으니, list_sort로 내림차순으로 정렬해준다.
    list_sort (&cond->waiters, sema_compare_priority, 0);
    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
  }
}

/** Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}

/* new function below. */

bool
sema_compare_priority (const struct list_elem *l, const struct list_elem *s, void *aux UNUSED)
{
  struct semaphore_elem *l_sema = list_entry (l, struct semaphore_elem, elem);
  struct semaphore_elem *s_sema = list_entry (s, struct semaphore_elem, elem);

  struct list *waiter_l_sema = &(l_sema->semaphore.waiters);
  struct list *waiter_s_sema = &(s_sema->semaphore.waiters);

   return list_entry (list_begin (waiter_l_sema), struct thread, elem)->priority
            > list_entry (list_begin (waiter_s_sema), struct thread, elem)->priority;
}