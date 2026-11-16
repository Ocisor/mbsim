// mbsim: the fiber scheduler, the simulated clock and the idle loop.
//
// The scheduling rules (run queue, sleep queue, wait queue, fork-on-block, idle) are a
// port of codal-core/source/core/CodalFiber.cpp.
// Copyright (c) 2017 Lancaster University. MIT licence, see LICENSE.
//
// What differs from the board, and why:
//   * Context switches use POSIX ucontext instead of CODAL's register save/restore, and
//     each fiber has a fixed stack (64 KB) instead of a paged one.
//   * CODAL's "fork on block" runs a handler on the caller's stack and only creates a
//     fiber if the handler blocks. Here the handler gets its own fiber straight away and
//     the caller resumes when it first blocks or returns. The observable order is the same.
//   * Interrupts don't exist. Everything CODAL does from its 4 ms timer interrupt (button
//     debounce, animation steps, waking sleepers) is done by run_tick(), which is called
//     for every tick that has become due whenever the simulation thread enters the
//     scheduler or reads the time. Each tick sees its own, exact, time.

#include "internal.h"

#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#ifdef MBSIM_ASAN
#include <sanitizer/asan_interface.h>
#include <sanitizer/common_interface_defs.h>
#endif

using namespace codal;

namespace codal
{
    struct Fiber
    {
        ucontext_t ctx;
        void *map = nullptr;       // whole mapping, including the guard page
        size_t mapSize = 0;
        void *stackLo = nullptr;   // usable stack
        size_t stackSize = 0;
        uint64_t context = 0;      // wake time (ms) or packed event id/value
        uint32_t flags = 0;
        Fiber **queue = nullptr;
        Fiber *qnext = nullptr;
        Fiber *qprev = nullptr;
        Fiber *parent = nullptr;   // who to return to while in fork-on-block
        int id = 0;
        bool dead = false;
        bool parameterised = false;
        void (*ep0)(void) = nullptr;
        void (*cp0)(void) = nullptr;
        void (*ep1)(void *) = nullptr;
        void (*cp1)(void *) = nullptr;
        void *pm = nullptr;
        void *fakeStack = nullptr; // AddressSanitizer bookkeeping
    };
}

namespace
{
    // ---- fibers (all touched only by the simulation thread)
    Fiber *currentFiber = nullptr;
    Fiber *idleFiber = nullptr;
    Fiber *runQueue = nullptr;
    Fiber *sleepQueue = nullptr;
    Fiber *waitQueue = nullptr;
    Fiber *zombies[64];
    int zombieCount = 0;
    uint8_t fiber_flags = 0;
    EventModel *messageBus = nullptr;
    int nextFiberId = 0;
    int liveFibers = 0;

#ifdef MBSIM_ASAN
    const size_t FIBER_STACK_SIZE = 512 * 1024;  // instrumented frames are much larger
#else
    const size_t FIBER_STACK_SIZE = 64 * 1024;
#endif

    // ---- clock
    std::mutex clockMu;
    uint64_t baseSimUs = 0;
    uint64_t baseRealNs = 0;
    bool frozen = false;
    uint64_t virtUs = 0;         // headless: the virtual clock
    uint64_t ticksDone = 0;
    bool tickActive = false;
    uint64_t tickTimeUs = 0;
}

bool mbsim::started = false;
std::atomic<int> mbsim::runningFiber{-1};
std::atomic<uint64_t> mbsim::lastYieldNs{0};
int mbsim::internalDepth = 0;

// ------------------------------------------------------------------------------- clock

uint64_t mbsim::mono_ns()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

void mbsim::start_clock()
{
    std::lock_guard<std::mutex> g(clockMu);
    baseSimUs = 0;
    baseRealNs = mono_ns();
    lastYieldNs = baseRealNs;
    started = true;
}

uint64_t mbsim::raw_now_us()
{
    if (!started)
        return 0;
    if (opt().headless)
        return virtUs;
    std::lock_guard<std::mutex> g(clockMu);
    if (frozen)
        return baseSimUs;
    return baseSimUs + (uint64_t)((double)(mono_ns() - baseRealNs) / 1000.0 * opt().speed);
}

void mbsim::toggle_freeze()
{
    std::lock_guard<std::mutex> g(clockMu);
    uint64_t now = mono_ns();
    if (!frozen)
        baseSimUs += (uint64_t)((double)(now - baseRealNs) / 1000.0 * opt().speed);
    baseRealNs = now;
    frozen = !frozen;
}

bool mbsim::is_frozen()
{
    std::lock_guard<std::mutex> g(clockMu);
    return frozen;
}

// One scheduler tick: what the board does from its periodic timer interrupt.
static void run_tick(uint64_t t)
{
    if (mbsim::opt().headless && t / 1000 >= (uint64_t)mbsim::opt().untilMs)
        mbsim::finish(mbsim::EXIT_OK);

    tickActive = true;
    tickTimeUs = t;

    mbsim::input_apply(t);

    for (int i = 0; i < DEVICE_COMPONENT_COUNT; i++)
    {
        CodalComponent *c = CodalComponent::components[i];
        if (c && (c->status & DEVICE_COMPONENT_STATUS_SYSTEM_TICK))
            c->periodicCallback();
    }

    mbsim::scheduler_wake_sleepers(t);
    mbsim::flush_frame();

    tickActive = false;
}

void mbsim::catch_up()
{
    if (tickActive || !started)
        return;
    uint64_t now = raw_now_us();
    while ((ticksDone + 1) * TICK_US <= now)
    {
        ticksDone++;
        run_tick(ticksDone * TICK_US);
    }
}

uint64_t mbsim::sim_now_us()
{
    if (tickActive)
        return tickTimeUs;
    catch_up();
    return raw_now_us();
}

uint64_t mbsim::user_now_us()
{
    if (tickActive)
        return tickTimeUs;
    // Headless time is virtual and only moves when the program waits. A loop that polls
    // the clock without sleeping would never end, so each poll costs one microsecond.
    if (opt().headless && started)
        virtUs++;
    catch_up();
    return raw_now_us();
}

void mbsim::idle_wait()
{
    runningFiber = -1;
    if (opt().headless)
    {
        if (program_finished())
            finish(EXIT_OK);
        uint64_t next = (ticksDone + 1) * TICK_US;
        if (virtUs < next)
            virtUs = next;
    }
    else
    {
        uint64_t target = (ticksDone + 1) * TICK_US;
        uint64_t now = raw_now_us();
        uint64_t waitUs = 2000;
        if (!is_frozen() && now < target)
        {
            waitUs = (uint64_t)((double)(target - now) / opt().speed) + 1;
            if (waitUs > 20000)
                waitUs = 20000;
        }
        else if (!is_frozen())
            waitUs = 0;
        if (waitUs)
            usleep((useconds_t)waitUs);
    }
    catch_up();
}

void mbsim::spin_wait_us(uint64_t us)
{
    if (!started)
        return;
    uint64_t end = sim_now_us() + us;
    for (;;)
    {
        uint64_t now = sim_now_us();
        if (now >= end)
            break;
        if (opt().headless)
        {
            uint64_t next = (ticksDone + 1) * TICK_US;
            virtUs = next < end ? next : end;
        }
        else
        {
            uint64_t waitUs = (uint64_t)((double)(end - now) / opt().speed) + 1;
            usleep((useconds_t)(waitUs > 2000 ? 2000 : waitUs));
        }
    }
}

CODAL_TIMESTAMP codal::system_timer_current_time()
{
    return mbsim::user_now_us() / 1000;
}

CODAL_TIMESTAMP codal::system_timer_current_time_us()
{
    return mbsim::user_now_us();
}

void codal::target_wait(uint32_t milliseconds)
{
    mbsim::spin_wait_us((uint64_t)milliseconds * 1000);
}

void codal::target_wait_us(uint32_t us)
{
    mbsim::spin_wait_us(us);
}

// ------------------------------------------------------------------------------ queues
// Ported from CodalFiber.cpp: fibers are appended to the tail, so each queue is FIFO.

static void queue_fiber(Fiber *f, Fiber **queue)
{
    f->queue = queue;

    if (*queue == NULL)
    {
        f->qnext = NULL;
        f->qprev = NULL;
        *queue = f;
    }
    else
    {
        Fiber *last = *queue;
        while (last->qnext != NULL)
            last = last->qnext;

        last->qnext = f;
        f->qprev = last;
        f->qnext = NULL;
    }
}

static void dequeue_fiber(Fiber *f)
{
    if (f->queue == NULL)
        return;

    if (f->qprev != NULL)
        f->qprev->qnext = f->qnext;
    else
        *(f->queue) = f->qnext;

    if (f->qnext)
        f->qnext->qprev = f->qprev;

    f->qnext = NULL;
    f->qprev = NULL;
    f->queue = NULL;
}

// --------------------------------------------------------------------- context switching

static void free_fiber(Fiber *f)
{
    if (f->map)
    {
#ifdef MBSIM_ASAN
        // Clear AddressSanitizer's record of this stack, so a later mapping at the same
        // address does not inherit stale "poisoned" bytes from frames that never returned.
        __asan_unpoison_memory_region(f->stackLo, f->stackSize);
#endif
        munmap(f->map, f->mapSize);
    }
    delete f;
}

static void reap_zombies()
{
    int kept = 0;
    for (int i = 0; i < zombieCount; i++)
    {
        if (zombies[i] == currentFiber)
            zombies[kept++] = zombies[i];
        else
            free_fiber(zombies[i]);
    }
    zombieCount = kept;
}

static void switch_to(Fiber *from, Fiber *to)
{
    mbsim::runningFiber = to == idleFiber ? -1 : to->id;
    mbsim::lastYieldNs = mbsim::mono_ns();
#ifdef MBSIM_ASAN
    __sanitizer_start_switch_fiber(from->dead ? nullptr : &from->fakeStack, to->stackLo, to->stackSize);
#endif
    swapcontext(&from->ctx, &to->ctx);
#ifdef MBSIM_ASAN
    __sanitizer_finish_switch_fiber(from->fakeStack, nullptr, nullptr);
#endif
}

static void do_schedule();
static void do_release();

static void fiber_trampoline()
{
#ifdef MBSIM_ASAN
    __sanitizer_finish_switch_fiber(nullptr, nullptr, nullptr);
#endif
    Fiber *f = currentFiber;
    // launch_new_fiber / launch_new_fiber_param
    if (f->parameterised)
    {
        f->ep1(f->pm);
        if (f->cp1 && f->cp1 != (void (*)(void *))release_fiber)
            f->cp1(f->pm);
    }
    else
    {
        f->ep0();
        if (f->cp0 && f->cp0 != (void (*)(void))release_fiber)
            f->cp0();
    }
    do_release();
    abort();  // not reached
}

static Fiber *new_fiber()
{
    Fiber *f = new Fiber();
    long page = sysconf(_SC_PAGESIZE);
    f->mapSize = FIBER_STACK_SIZE + (size_t)page;
    f->map = mmap(nullptr, f->mapSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (f->map == MAP_FAILED)
    {
        fprintf(stderr, "mbsim: out of memory creating a fiber\n");
        mbsim::finish(mbsim::EXIT_CRASH);
    }
    mprotect(f->map, (size_t)page, PROT_NONE);  // guard page: overflow faults instead of corrupting
    f->stackLo = (char *)f->map + page;
    f->stackSize = FIBER_STACK_SIZE;
    f->id = nextFiberId++;

    getcontext(&f->ctx);
    f->ctx.uc_stack.ss_sp = f->stackLo;
    f->ctx.uc_stack.ss_size = f->stackSize;
    f->ctx.uc_link = nullptr;
    makecontext(&f->ctx, fiber_trampoline, 0);
    liveFibers++;
    return f;
}

static void ensure_idle_fiber()
{
    if (idleFiber)
        return;
    idleFiber = new Fiber();
    idleFiber->id = -1;
    // The idle loop runs on the simulation thread's own stack.
    pthread_attr_t attr;
    if (pthread_getattr_np(pthread_self(), &attr) == 0)
    {
        pthread_attr_getstack(&attr, &idleFiber->stackLo, &idleFiber->stackSize);
        pthread_attr_destroy(&attr);
    }
    currentFiber = idleFiber;
}

// ------------------------------------------------------------------------ the scheduler

static void do_schedule()
{
    mbsim::catch_up();
    mbsim::flush_frame();
    reap_zombies();

    Fiber *oldFiber = currentFiber;

    // Fork on block. A handler that was started by invoke() is blocking (or finishing)
    // for the first time: hand control back to whoever invoked it.
    if (oldFiber->flags & DEVICE_FIBER_FLAG_FOB)
    {
        oldFiber->flags &= ~DEVICE_FIBER_FLAG_FOB;
        oldFiber->flags |= DEVICE_FIBER_FLAG_CHILD;

        Fiber *parent = oldFiber->parent;
        oldFiber->parent = nullptr;

        // A bare schedule() (a yield, not a sleep or wait) leaves the new fiber runnable.
        if (!oldFiber->dead && oldFiber->queue == NULL)
            queue_fiber(oldFiber, &runQueue);

        currentFiber = parent;
        switch_to(oldFiber, parent);
        return;
    }

    // We're in a normal scheduling context, so perform a round robin algorithm across
    // runnable fibers. If we've nothing to do, then run the IDLE task.
    if (runQueue == NULL)
        currentFiber = idleFiber;
    else if (currentFiber->queue == &runQueue)
        // If the current fiber is on the run queue, round robin.
        currentFiber = currentFiber->qnext == NULL ? runQueue : currentFiber->qnext;
    else
        // Otherwise, just pick the head of the run queue.
        currentFiber = runQueue;

    if (currentFiber != oldFiber)
        switch_to(oldFiber, currentFiber);
    else
        mbsim::lastYieldNs = mbsim::mono_ns();
}

static void do_release()
{
    Fiber *f = currentFiber;
    dequeue_fiber(f);
    f->dead = true;
    liveFibers--;
    if (zombieCount < (int)(sizeof(zombies) / sizeof(zombies[0])))
        zombies[zombieCount++] = f;
    do_schedule();
}

void codal::scheduler_init(EventModel &_messageBus)
{
    // If we're already initialised, then nothing to do.
    if (fiber_scheduler_running())
        return;

    messageBus = &_messageBus;

    void scheduler_event(Event evt);
    mbsim::InternalScope internal;
    // Register to receive events in the NOTIFY channel - this is used to implement
    // wait-notify semantics.
    messageBus->listen(DEVICE_ID_NOTIFY, DEVICE_EVT_ANY, scheduler_event, MESSAGE_BUS_LISTENER_IMMEDIATE);
    messageBus->listen(DEVICE_ID_NOTIFY_ONE, DEVICE_EVT_ANY, scheduler_event, MESSAGE_BUS_LISTENER_IMMEDIATE);

    fiber_flags |= DEVICE_SCHEDULER_RUNNING;
}

int codal::fiber_scheduler_running()
{
    if (fiber_flags & DEVICE_SCHEDULER_RUNNING)
        return 1;

    return 0;
}

// Ported from scheduler_tick(): wake every sleeper whose time has come, in the order
// they went to sleep.
void mbsim::scheduler_wake_sleepers(uint64_t tickUs)
{
    if (!fiber_scheduler_running())
        return;

    uint64_t timestamp = tickUs / 1000;
    Fiber *f = sleepQueue;
    Fiber *t;

    while (f != NULL)
    {
        t = f->qnext;

        if (timestamp >= f->context)
        {
            // Wakey wakey!
            dequeue_fiber(f);
            queue_fiber(f, &runQueue);
        }

        f = t;
    }
}

namespace codal
{
    void scheduler_event(Event evt)
    {
        Fiber *f = waitQueue;
        Fiber *t;
        int notifyOneComplete = 0;

        if (messageBus == NULL)
            return;

        // Check the wait queue, and wake up any fibers as necessary.
        while (f != NULL)
        {
            t = f->qnext;

            // extract the event data this fiber is blocked on.
            uint16_t id = f->context & 0xFFFF;
            uint16_t value = (f->context & 0xFFFF0000) >> 16;

            // Special case for the NOTIFY_ONE channel...
            if ((evt.source == DEVICE_ID_NOTIFY_ONE && id == DEVICE_ID_NOTIFY) && (value == DEVICE_EVT_ANY || value == evt.value))
            {
                if (!notifyOneComplete)
                {
                    // Wakey wakey!
                    dequeue_fiber(f);
                    queue_fiber(f, &runQueue);
                    notifyOneComplete = 1;
                }
            }

            // Normal case.
            else if ((id == DEVICE_ID_ANY || id == evt.source) && (value == DEVICE_EVT_ANY || value == evt.value))
            {
                // Wakey wakey!
                dequeue_fiber(f);
                queue_fiber(f, &runQueue);
            }

            f = t;
        }

        // Unregister this event, as we've woken up all the fibers with this match.
        if (evt.source != DEVICE_ID_NOTIFY && evt.source != DEVICE_ID_NOTIFY_ONE)
            messageBus->ignore(evt.source, evt.value, scheduler_event);
    }
}

void codal::fiber_sleep(unsigned long t)
{
    // If the scheduler is not running, then simply perform a spin wait and exit.
    if (!fiber_scheduler_running())
    {
        target_wait(t);
        return;
    }

    Fiber *f = currentFiber;

    // Calculate and store the time we want to wake up.
    f->context = mbsim::sim_now_us() / 1000 + t;

    // Remove fiber from the run queue
    dequeue_fiber(f);

    // Add fiber to the sleep queue.
    queue_fiber(f, &sleepQueue);

    // Finally, enter the scheduler.
    schedule();
}

int codal::fiber_wait_for_event(uint16_t id, uint16_t value)
{
    int ret = fiber_wake_on_event(id, value);

    if (ret == DEVICE_OK)
        schedule();

    return ret;
}

int codal::fiber_wake_on_event(uint16_t id, uint16_t value)
{
    if (messageBus == NULL || !fiber_scheduler_running())
        return DEVICE_NOT_SUPPORTED;

    Fiber *f = currentFiber;

    // Encode the event data in the context field.
    f->context = (uint32_t)value << 16 | id;

    // Remove ourselves from the run queue
    dequeue_fiber(f);

    // Add ourselves to the wait queue.
    queue_fiber(f, &waitQueue);

    // Register to receive this event, so we can wake up the fiber when it happens.
    // Special case for the notify channel, as we always stay registered for that.
    if (id != DEVICE_ID_NOTIFY && id != DEVICE_ID_NOTIFY_ONE)
    {
        mbsim::InternalScope internal;
        messageBus->listen(id, value, scheduler_event, MESSAGE_BUS_LISTENER_IMMEDIATE);
    }

    return DEVICE_OK;
}

static int do_invoke(Fiber *f)
{
    // Enter FORK ON BLOCK mode and run the function now. If it blocks, control comes
    // straight back here and the function carries on later as an ordinary fiber.
    f->flags |= DEVICE_FIBER_FLAG_FOB;
    f->parent = currentFiber;
    Fiber *old = currentFiber;
    currentFiber = f;
    switch_to(old, f);
    return DEVICE_OK;
}

int codal::invoke(void (*entry_fn)(void))
{
    if (entry_fn == NULL)
        return DEVICE_INVALID_PARAMETER;

    if (!fiber_scheduler_running())
        return DEVICE_NOT_SUPPORTED;

    if (currentFiber->flags & (DEVICE_FIBER_FLAG_FOB | DEVICE_FIBER_FLAG_PARENT | DEVICE_FIBER_FLAG_CHILD))
    {
        // If we attempt a fork on block whilst already in a fork on block context,
        // simply launch a fiber to deal with the request and we're done.
        create_fiber(entry_fn);
        return DEVICE_OK;
    }

    Fiber *f = new_fiber();
    f->parameterised = false;
    f->ep0 = entry_fn;
    f->cp0 = release_fiber;
    return do_invoke(f);
}

int codal::invoke(void (*entry_fn)(void *), void *param)
{
    if (entry_fn == NULL)
        return DEVICE_INVALID_PARAMETER;

    if (!fiber_scheduler_running())
        return DEVICE_NOT_SUPPORTED;

    if (currentFiber->flags & (DEVICE_FIBER_FLAG_FOB | DEVICE_FIBER_FLAG_PARENT | DEVICE_FIBER_FLAG_CHILD))
    {
        create_fiber(entry_fn, param);
        return DEVICE_OK;
    }

    Fiber *f = new_fiber();
    f->parameterised = true;
    f->ep1 = entry_fn;
    f->cp1 = release_fiber;
    f->pm = param;
    return do_invoke(f);
}

Fiber *codal::create_fiber(void (*entry_fn)(void), void (*completion_fn)(void))
{
    if (!fiber_scheduler_running())
        return NULL;

    if (entry_fn == NULL || completion_fn == NULL)
        return NULL;

    Fiber *f = new_fiber();
    f->parameterised = false;
    f->ep0 = entry_fn;
    f->cp0 = completion_fn;

    // Add new fiber to the run queue.
    queue_fiber(f, &runQueue);
    return f;
}

Fiber *codal::create_fiber(void (*entry_fn)(void *), void *param, void (*completion_fn)(void *))
{
    if (!fiber_scheduler_running())
        return NULL;

    if (entry_fn == NULL || completion_fn == NULL)
        return NULL;

    Fiber *f = new_fiber();
    f->parameterised = true;
    f->ep1 = entry_fn;
    f->cp1 = completion_fn;
    f->pm = param;

    queue_fiber(f, &runQueue);
    return f;
}

void codal::release_fiber(void *)
{
    release_fiber();
}

void codal::release_fiber(void)
{
    if (!fiber_scheduler_running())
        return;

    // The idle loop is not a fiber that can be released.
    if (currentFiber == idleFiber)
        return;

    do_release();
}

int codal::scheduler_runqueue_empty()
{
    return (runQueue == NULL);
}

int codal::scheduler_waitqueue_empty()
{
    return (waitQueue == NULL);
}

void codal::schedule()
{
    if (!fiber_scheduler_running())
        return;

    do_schedule();
}

void codal::idle()
{
    // Prevent an idle loop of death:
    // We will return to idle after processing any idle events that add anything
    // to our run queue, we use the DEVICE_SCHEDULER_IDLE flag to determine this
    // scenario.
    if (!(fiber_flags & DEVICE_SCHEDULER_IDLE))
    {
        fiber_flags |= DEVICE_SCHEDULER_IDLE;
        Event(DEVICE_ID_SCHEDULER, DEVICE_SCHEDULER_EVT_IDLE);
    }

    // If the above did create any useful work, enter power efficient sleep.
    if (scheduler_runqueue_empty())
    {
        // unset our DEVICE_SCHEDULER_IDLE flag, we have processed all of the events
        // because we enforce MESSAGE_BUS_LISTENER_IMMEDIATE for listeners placed
        // on the scheduler.
        fiber_flags &= ~DEVICE_SCHEDULER_IDLE;

        // target_scheduler_idle(): on the board, sleep until the next interrupt.
        mbsim::idle_wait();
    }
}

// ----------------------------------------------------------------------- FiberLock port

void mbsim::FiberLock::wait()
{
    // If the scheduler is not running, then simply exit, as we're running monothreaded.
    if (!fiber_scheduler_running())
        return;

    int l = --locked;

    if (l < 0)
    {
        Fiber *f = currentFiber;

        // Remove fiber from the run queue
        dequeue_fiber(f);

        // Add fiber to the lock's queue.
        queue_fiber(f, &queue);

        // Finally, enter the scheduler.
        schedule();
    }
}

void mbsim::FiberLock::notify()
{
    locked++;
    Fiber *f = queue;
    if (f)
    {
        dequeue_fiber(f);
        queue_fiber(f, &runQueue);
    }
}

// ------------------------------------------------------------------------ runtime entry

void mbsim::start_main_fiber(void (*fn)(void *))
{
    ensure_idle_fiber();
    Fiber *f = new_fiber();  // id 0
    f->parameterised = true;
    f->ep1 = fn;
    f->cp1 = nullptr;
    queue_fiber(f, &runQueue);
}

void mbsim::run_idle_loop()
{
    ensure_idle_fiber();
    // idle_task()
    for (;;)
    {
        idle();
        do_schedule();
    }
}

int mbsim::current_fiber_id()
{
    return currentFiber ? currentFiber->id : -1;
}

int mbsim::live_fiber_count()
{
    return liveFibers;
}
