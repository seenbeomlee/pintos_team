#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "devices/pit.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
  
/** See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/** Number of timer ticks since OS booted. */
static int64_t ticks;

/** Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);
static void real_time_delay (int64_t num, int32_t denom);

/** Sets up the timer to interrupt TIMER_FREQ times per second,
   and registers the corresponding interrupt. */
void
timer_init (void) 
{
  pit_configure_channel (0, 2, TIMER_FREQ);
  intr_register_ext (0x20, timer_interrupt, "8254 Timer");
}

/** Calibrates loops_per_tick, used to implement brief delays. */
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
    if (!too_many_loops (high_bit | test_bit))
      loops_per_tick |= test_bit;

  printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/** Returns the number of timer ticks since the OS booted. */
int64_t
timer_ticks (void) 
{
  enum intr_level old_level = intr_disable ();
  int64_t t = ticks;
  intr_set_level (old_level);
  return t;
}

/** Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */
int64_t
timer_elapsed (int64_t then) 
{
  return timer_ticks () - then;
}

/** Sleeps for approximately TICKS timer ticks.  Interrupts must
   be turned on. */
/**
 * 기존 핀토스에 구현되어 있는 busy-waitng을 이용한 timer_sleep() 코드는 아래와 같다.
 * ticks란 핀토스 내부에서 시간을 나타내기 위한 값으로, 부팅 이후에 일정한 시간마다 1씩 증가한다.
 * 1 tick의 시간을 설정해 줄 수 있는데, 현재 핀토스의 1tick은
 * #define TIMER_FREQ 100
 * 으로 인해서 1ms로 설정되어 있다.
 * 이에 따라, 운영체제는 1ms마다 timer interrupt를 실행시키고 ticks의 값을 1씩 증가시킨다.
 */
void
timer_sleep (int64_t ticks) 
{
  int64_t start = timer_ticks (); /* ticks란, PintOS 내부에서 시간을 나타내기 위한 값으로 부팅 이후에 일정한 시간마다 1씩 증가한다. */
  ASSERT (intr_get_level () == INTR_ON);
  /** 
    * 필요없어진 기존 코드 삭제 
    * timer_elased 함수는 특정시간 이후로 경과된 시간(ticks)을 반환한다.
    * 즉, timer_elapsed(start)는 start 이후로 경과된 시간(ticks)을 반환한다.
    */  
  // while (timer_elapsed (start) < ticks) 
  // thread_yield ();

  // start 변수는 os가 시작된 후부터 지난 tick을 반환하는 timer_ticks() 값을 저장한다. 
  // timer_sleep() 함수가 호출되면 thread가 block 상태로 들어간다. 
  // 이렇게 block 된 thread 들은 일어날 시간이 되었을 때 awake 되어야 한다. 
  thread_sleep(start + ticks); 
}

/** Sleeps for approximately MS milliseconds.  Interrupts must be
   turned on. */
void
timer_msleep (int64_t ms) 
{
  real_time_sleep (ms, 1000);
}

/** Sleeps for approximately US microseconds.  Interrupts must be
   turned on. */
void
timer_usleep (int64_t us) 
{
  real_time_sleep (us, 1000 * 1000);
}

/** Sleeps for approximately NS nanoseconds.  Interrupts must be
   turned on. */
void
timer_nsleep (int64_t ns) 
{
  real_time_sleep (ns, 1000 * 1000 * 1000);
}

/** Busy-waits for approximately MS milliseconds.  Interrupts need
   not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_msleep()
   instead if interrupts are enabled. */
void
timer_mdelay (int64_t ms) 
{
  real_time_delay (ms, 1000);
}

/** Sleeps for approximately US microseconds.  Interrupts need not
   be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_usleep()
   instead if interrupts are enabled. */
void
timer_udelay (int64_t us) 
{
  real_time_delay (us, 1000 * 1000);
}

/** Sleeps execution for approximately NS nanoseconds.  Interrupts
   need not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_nsleep()
   instead if interrupts are enabled.*/
void
timer_ndelay (int64_t ns) 
{
  real_time_delay (ns, 1000 * 1000 * 1000);
}

/** Prints timer statistics. */
void
timer_print_stats (void) 
{
  printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}


/** Scheduling
 * 하나의 스레드를 실행시키다가 다른 스레드로 CPU의 소유권을 넘겨야 할 때 scheduling이 일어난다.
 * 이 scheduling이 일어나는 순간은
 * 1. thread_yield()
 * 2. thread_block()
 * 3. thread_exit()
 * 함수가 실행될 때이다. 이 함수들은 현재 실행중인 스레드에서 호출해주어야 하는데,
 * 실행중인 스레드가 이 함수들을 실행시키지 않고 계속 CPU 소유권을 가지는 것을 방지하기 위하여 time_interrupt를 활용한다.
 */
static void
timer_interrupt (struct intr_frame *args UNUSED) {
	ticks++; // 매 tick마다 ticks라는 변수를 증가시킴으로써 시간을 잰다.
	thread_tick ();

/** 1
 * 1. 1 tick 마다 running thread의 recent_cpu 값 + 1
 * 2. 4 ticks 마다 모든 thread의 priority 재계산
 * 3. 1초 마다 모든 thread의 recent_cpu 값과 load_avg 값 재계산
 * 각 함수를 해당하는 시간 주기마다 실행되도록 timer_interrupt ()를 바꾸어주면 된다.
 * TIMER_FREQ 값은 1초에 몇 개의 ticks 이 실행되는지를 나타내는 값으로 thread.h 에 100 으로 정의되어 있다. 
 * 이에 따라 pintos kernel 은 1 초에 100 ticks 가 실행되고 1 ticks = 1 ms 를 의미한다.
 */
	/** project1-Advanced Scheduler */	
    if (thread_mlfqs) { // 1 tick 마다 running thread의 recent_cpu 값 + 1
      mlfqs_increment_recent_cpu();
      if (!(ticks % 4)) {
        mlfqs_recalculate_priority(); // 4 tick 마다 모든 thread의 priority 재계산
      if (!(ticks % TIMER_FREQ)) { // 1초 마다 모든 thread의 recent_cpu값과 load_avg값 재계산
        mlfqs_calculate_load_avg();
        mlfqs_recalculate_recent_cpu();
      }
    }
  }
	thread_awake (ticks); // ticks가 증가할때마다 awake 작업을 수행한다.
}

/** Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
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

/** Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
static void NO_INLINE
busy_wait (int64_t loops) 
{
  while (loops-- > 0)
    barrier ();
}

/** Sleep for approximately NUM/DENOM seconds. */
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

/** Busy-wait for approximately NUM/DENOM seconds. */
static void
real_time_delay (int64_t num, int32_t denom)
{
  /* Scale the numerator and denominator down by 1000 to avoid
     the possibility of overflow. */
  ASSERT (denom % 1000 == 0);
  busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000)); 
}
