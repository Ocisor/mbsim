// mbsim runtime internals. Nothing here is visible to user programs.
#ifndef MBSIM_INTERNAL_H
#define MBSIM_INTERNAL_H

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "MicroBit.h"

// IMPORTANT: the user's global `MicroBit uBit` is constructed before any of the runtime's
// own globals are guaranteed to be. So every runtime global is either trivially
// initialised (plain data, atomics, std::mutex) or lives behind a function (opt(),
// shared()) that builds it on first use.

namespace mbsim
{
    constexpr uint64_t TICK_US = SCHEDULER_TICK_PERIOD_US;

    // Exit codes (see README, "Headless mode").
    enum
    {
        EXIT_OK = 0,
        EXIT_USAGE = 2,
        EXIT_PANIC = 3,
        EXIT_CRASH = 4,
        EXIT_WATCHDOG = 5,
        EXIT_RESET = 75  // interactive only: the CLI starts the program again
    };

    struct Options
    {
        bool headless = false;
        bool seedGiven = false;
        uint32_t seed = 0;
        double speed = 1.0;
        long untilMs = 10000;
        std::string script, outDir, name = "main.cpp", symbols, serialLog;
    };
    Options &opt();

    // ---------------------------------------------------------------- clock and ticks
    extern bool started;                 // false until the runtime's main() is ready
    uint64_t raw_now_us();               // simulated time; safe from any thread
    uint64_t sim_now_us();               // sim thread: runs due ticks first; tick time inside a tick
    uint64_t user_now_us();              // what user code sees (adds the headless busy-loop step)
    void catch_up();                     // run every tick that is due
    void idle_wait();                    // nothing runnable: let time move to the next tick
    void spin_wait_us(uint64_t us);      // wait without yielding (target_wait)
    void toggle_freeze();
    bool is_frozen();
    void start_clock();

    // ---------------------------------------------------------------- fibers
    void start_main_fiber(void (*fn)(void *));
    [[noreturn]] void run_idle_loop();
    void scheduler_wake_sleepers(uint64_t tickUs);
    int current_fiber_id();              // 0 = main(), -1 = the idle loop
    int live_fiber_count();              // runnable + sleeping + waiting
    extern std::atomic<int> runningFiber;        // for the UI: -1 when idle
    extern std::atomic<uint64_t> lastYieldNs;    // monotonic time of the last fiber switch
    uint64_t mono_ns();

    // Port of codal::FiberLock (wait / notify only).
    struct FiberLock
    {
        codal::Fiber *queue;
        int locked;
        void wait();
        void notify();
    };

    // RAII: listeners registered while one of these exists are runtime-internal.
    extern int internalDepth;
    struct InternalScope
    {
        InternalScope() { internalDepth++; }
        ~InternalScope() { internalDepth--; }
    };

    // ---------------------------------------------------------------- input lines
    enum { LINE_A = 0, LINE_B = 1, LINE_LOGO = 2, LINE_COUNT = 3 };
    void input_push(uint64_t tUs, int line, bool down);  // any thread
    void input_apply(uint64_t tickUs);                   // sim thread, from the tick
    bool line_active(int line);                          // sim thread
    bool input_idle();                                   // no queued actions, all buttons at rest
    bool ui_line_state(int line, uint64_t nowUs);        // UI thread: is the key "held"?

    // ---------------------------------------------------------------- display
    extern std::atomic<uint8_t> livePixels[25];
    extern std::atomic<int> liveMode;        // a DisplayMode
    extern std::atomic<int> liveBrightness;  // 0-255
    extern std::atomic<bool> liveEnabled;
    void display_changed(const uint8_t *pixels);   // the display image was written
    void display_register(const uint8_t *pixels);  // remember where the display image lives
    void flush_frame();                            // headless: write a frame if it changed
    int lit_level(int pixel, int mode, int brightness);  // 0-255 as shown

    // ---------------------------------------------------------------- status shared with the UI
    struct Shared
    {
        std::mutex mu;
        std::deque<std::string> serial;   // complete lines, newest last
        std::string serialPartial;
        std::deque<std::string> events;
        std::deque<std::string> notes;
        std::string soundName;
        uint64_t soundEndUs = 0;
        bool soundPlaying = false;
        bool mainReturned = false;
        int panicCode = -1;
    };
    Shared &shared();
    extern std::atomic<int> liveVolume;

    void note(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
    void note_once(const char *key, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

    // ---------------------------------------------------------------- event log
    bool is_internal_event(uint16_t source);
    void evlog_delivered(const codal::Event &evt, codal::Listener *l);
    void evlog_undelivered(const codal::Event &evt, const char *why);

    // ---------------------------------------------------------------- outputs
    void serial_out(const char *text, size_t len);
    void sound_out(const std::string &name, uint64_t startUs, uint64_t endUs, bool async);
    void sound_status(const std::string &name, uint64_t endUs, bool playing);
    void audio_flush_at_exit();

    // ---------------------------------------------------------------- lifecycle
    [[noreturn]] void finish(int code);       // flush everything, restore the terminal, exit
    bool program_finished();                  // headless: nothing left that could ever happen
    void main_returned(int rc);
    [[noreturn]] void do_panic(int code);
    uint32_t pick_seed();

    // ---------------------------------------------------------------- ui/terminal.cpp
    void ui_start();
    void ui_restore();                        // idempotent; async-signal-safe
    // ---------------------------------------------------------------- ui/headless.cpp
    void headless_start();
    void headless_frame(uint64_t tMs, int mode, int brightness, const uint8_t *lit);
    void headless_event(const std::string &jsonLine);
    void headless_serial(const char *text, size_t len);
    void headless_sound(const std::string &jsonLine);
    void headless_note(const char *text);
    void headless_close();
}

#endif
