#ifndef DEVICES_TIMER_H
#define DEVICES_TIMER_H

#include <round.h>
#include <stdint.h>

/** Number of timer interrupts per second. */
/* 1 tick의 시간을 설정할 수 있다. 현재 1 tick은 1ms로 설정되어 있다. */
/* 이에 따라 운영체제는 1ms 마다 timer intrerrupt를 실행시키고 ticks의 값을 1씩 증가시킨다. */
#define TIMER_FREQ 100

void timer_init (void);
void timer_calibrate (void);

int64_t timer_ticks (void);
int64_t timer_elapsed (int64_t);

/** Sleep and yield the CPU to other threads. */
void timer_sleep (int64_t ticks);
void timer_msleep (int64_t milliseconds);
void timer_usleep (int64_t microseconds);
void timer_nsleep (int64_t nanoseconds);

/** Busy waits. */
void timer_mdelay (int64_t milliseconds);
void timer_udelay (int64_t microseconds);
void timer_ndelay (int64_t nanoseconds);

void timer_print_stats (void);

#endif /**< devices/timer.h */
