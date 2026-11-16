// mbsim: fiber scheduler API, ported from codal-core/inc/core/CodalFiber.h
// Copyright (c) 2017 Lancaster University. MIT licence, see LICENSE.
#ifndef MBSIM_FIBER_H
#define MBSIM_FIBER_H

#include <stdint.h>
#include "Event.h"

namespace codal
{
    struct Fiber;
    class EventModel;

    void scheduler_init(EventModel &_messageBus);
    int fiber_scheduler_running();

    void schedule();
    void fiber_sleep(unsigned long t);
    int fiber_wait_for_event(uint16_t id, uint16_t value);
    int fiber_wake_on_event(uint16_t id, uint16_t value);

    void release_fiber(void);
    void release_fiber(void *param);
    Fiber *create_fiber(void (*entry_fn)(void), void (*completion_fn)(void) = release_fiber);
    Fiber *create_fiber(void (*entry_fn)(void *), void *param, void (*completion_fn)(void *) = release_fiber);

    int invoke(void (*entry_fn)(void));
    int invoke(void (*entry_fn)(void *), void *param);

    int scheduler_runqueue_empty();
    int scheduler_waitqueue_empty();
    void idle();

    /** Milliseconds / microseconds of simulated time since the program started. */
    CODAL_TIMESTAMP system_timer_current_time();
    CODAL_TIMESTAMP system_timer_current_time_us();

    /** Spin waits. These do NOT yield: no other fiber runs and no event is delivered. */
    void target_wait(uint32_t milliseconds);
    void target_wait_us(uint32_t us);
    inline void wait_ms(uint32_t milliseconds) { target_wait(milliseconds); }
    inline void wait_us(uint32_t us) { target_wait_us(us); }
}

#endif
