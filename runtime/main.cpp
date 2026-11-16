// mbsim: the runtime's entry point, the event log, notes, output routing, exit paths,
// crash handling and the headless watchdog.
//
// The user's files are linked with -Wl,--wrap=main, so the program's real entry point is
// __wrap_main() below and the user's own main() is reachable as __real_main(). The user's
// source is never edited or recompiled differently to make this work.

#include "internal.h"

#include <cxxabi.h>
#include <dlfcn.h>
#include <link.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <set>

using namespace codal;

extern "C" int __real_main(int argc, char **argv);

namespace
{
    char **savedArgv = nullptr;
    uint32_t chosenSeed = 0;
    std::atomic<bool> finishing{false};
    uint64_t lastActivityUs = 0;
    bool mainHasReturned = false;
    FILE *serialLogFile = nullptr;
    std::mutex serialLogMu;

    void activity()
    {
        lastActivityUs = mbsim::raw_now_us();
    }
}

mbsim::Options &mbsim::opt()
{
    static Options o;
    return o;
}

mbsim::Shared &mbsim::shared()
{
    static Shared s;
    return s;
}

uint32_t mbsim::pick_seed()
{
    return chosenSeed;
}

// ------------------------------------------------------------------------------- notes

static void add_note(const char *text)
{
    if (mbsim::opt().headless)
    {
        mbsim::headless_note(text);
        return;
    }
    std::lock_guard<std::mutex> g(mbsim::shared().mu);
    auto &notes = mbsim::shared().notes;
    if (!notes.empty() && notes.back() == text)
        return;
    notes.push_back(text);
    while (notes.size() > 50)
        notes.pop_front();
}

void mbsim::note(const char *fmt, ...)
{
    char buf[400];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    add_note(buf);
}

void mbsim::note_once(const char *key, const char *fmt, ...)
{
    static std::set<std::string> seen;
    if (!seen.insert(key).second)
        return;
    char buf[400];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    add_note(buf);
}

// --------------------------------------------------------------------------- event log

bool mbsim::is_internal_event(uint16_t source)
{
    return source == DEVICE_ID_SCHEDULER || source == DEVICE_ID_COMPONENT ||
           source == DEVICE_ID_MESSAGE_BUS_IGNORED || source == DEVICE_ID_MESSAGE_BUS_LISTENER ||
           source == DEVICE_ID_NOTIFY_ONE || source == DEVICE_ID_NOTIFY;
}

static std::string event_name(const Event &e)
{
    char buf[64];
    const char *id = NULL;
    bool button = false;
    switch (e.source)
    {
        case DEVICE_ID_BUTTON_A: id = "BUTTON_A"; button = true; break;
        case DEVICE_ID_BUTTON_B: id = "BUTTON_B"; button = true; break;
        case DEVICE_ID_BUTTON_AB: id = "BUTTON_AB"; button = true; break;
        case ID_PIN_LOGO: id = "LOGO"; button = true; break;
        case DEVICE_ID_DISPLAY: id = "DISPLAY"; break;
        case DEVICE_ID_SOUND_EMOJI_SYNTHESIZER_0: id = "SOUND"; break;
    }
    const char *value = NULL;
    if (button)
    {
        static const char *names[] = {NULL, "DOWN", "UP", "CLICK", "LONG_CLICK", "HOLD", "DOUBLE_CLICK"};
        if (e.value >= 1 && e.value <= 6)
            value = names[e.value];
    }
    else if (e.source == DEVICE_ID_DISPLAY && e.value == DISPLAY_EVT_ANIMATION_COMPLETE)
        value = "ANIMATION_COMPLETE";
    else if (e.source == DEVICE_ID_SOUND_EMOJI_SYNTHESIZER_0)
        value = e.value == DEVICE_SOUND_EMOJI_SYNTHESIZER_EVT_DONE ? "DONE" : e.value == DEVICE_SOUND_EMOJI_SYNTHESIZER_EVT_PLAYBACK_COMPLETE ? "PLAYBACK_COMPLETE" : NULL;

    char idBuf[24], valBuf[24];
    if (!id)
    {
        snprintf(idBuf, sizeof(idBuf), "id=%u", e.source);
        id = idBuf;
    }
    if (!value)
    {
        snprintf(valBuf, sizeof(valBuf), "value=%u", e.value);
        value = valBuf;
    }
    snprintf(buf, sizeof(buf), "%s %s", id, value);
    return buf;
}

// ---- handler names: the symbol list the CLI wrote, then dladdr, then the raw address.

namespace
{
    struct Sym
    {
        uintptr_t addr;
        std::string name;
    };

    std::vector<Sym> &symbols()
    {
        static std::vector<Sym> v;
        return v;
    }
    bool symbolsLoaded = false;
    uintptr_t loadBase = 0;

    int phdr_cb(struct dl_phdr_info *info, size_t, void *data)
    {
        *(uintptr_t *)data = (uintptr_t)info->dlpi_addr;
        return 1;  // the first entry is the program itself
    }

    std::string strip_signature(const std::string &name)
    {
        // "onButtonA(codal::Event)" -> "onButtonA"
        size_t paren = name.find('(');
        return paren == std::string::npos ? name : name.substr(0, paren);
    }

    void load_symbols()
    {
        symbolsLoaded = true;
        dl_iterate_phdr(phdr_cb, &loadBase);
        if (mbsim::opt().symbols.empty())
            return;
        FILE *f = fopen(mbsim::opt().symbols.c_str(), "r");
        if (!f)
            return;
        char line[1024];
        while (fgets(line, sizeof(line), f))
        {
            unsigned long long addr;
            char type;
            int used = 0;
            if (sscanf(line, "%llx %c %n", &addr, &type, &used) < 2 || used == 0)
                continue;
            if (type != 'T' && type != 't' && type != 'W' && type != 'w')
                continue;
            std::string name = line + used;
            while (!name.empty() && (name.back() == '\n' || name.back() == '\r'))
                name.pop_back();
            symbols().push_back({(uintptr_t)addr & ~(uintptr_t)1, strip_signature(name)});
        }
        fclose(f);
    }

    std::string resolve(void *fn)
    {
        if (!symbolsLoaded)
            load_symbols();

        // Thumb function pointers have bit 0 set; clear it before comparing.
        uintptr_t addr = ((uintptr_t)fn & ~(uintptr_t)1) - loadBase;
        for (const Sym &s : symbols())
            if (s.addr == addr)
                return s.name;

        Dl_info info;
        if (dladdr(fn, &info) && info.dli_sname && info.dli_saddr == fn)
        {
            int status = 0;
            char *dem = abi::__cxa_demangle(info.dli_sname, NULL, NULL, &status);
            std::string name = strip_signature(status == 0 && dem ? dem : info.dli_sname);
            free(dem);
            return name;
        }

        char buf[32];
        snprintf(buf, sizeof(buf), "%p", fn);
        return buf;
    }

    std::string handler_name(Listener *l)
    {
        if (l->flags & MESSAGE_BUS_LISTENER_METHOD)
            return "a member function";
        return resolve((void *)l->cb);
    }

    std::string json_escape(const std::string &s)
    {
        std::string out;
        for (char c : s)
        {
            if (c == '"' || c == '\\')
            {
                out += '\\';
                out += c;
            }
            else if ((unsigned char)c < 0x20)
            {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            }
            else
                out += c;
        }
        return out;
    }

    void ui_event_line(const std::string &line)
    {
        std::lock_guard<std::mutex> g(mbsim::shared().mu);
        auto &ev = mbsim::shared().events;
        ev.push_back(line);
        while (ev.size() > 200)
            ev.pop_front();
    }
}

void mbsim::evlog_delivered(const Event &evt, Listener *l)
{
    activity();
    uint64_t queuedMs = evt.timestamp / 1000;
    uint64_t nowUs = sim_now_us();
    std::string name = event_name(evt);
    std::string handler = handler_name(l);
    int fiber = current_fiber_id();

    if (opt().headless)
    {
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "{\"id\":%u,\"value\":%u,\"name\":\"%s\",\"queued_ms\":%llu,\"delivered_ms\":%llu,"
                 "\"handler\":\"%s\",\"fiber\":%d}",
                 evt.source, evt.value, json_escape(name).c_str(), (unsigned long long)queuedMs,
                 (unsigned long long)(nowUs / 1000), json_escape(handler).c_str(), fiber);
        headless_event(buf);
    }
    else
    {
        char buf[512];
        snprintf(buf, sizeof(buf), "%.2f %s queued \xe2\x86\x92 %.2f delivered to %s (fiber %d)",
                 (double)evt.timestamp / 1e6, name.c_str(), (double)nowUs / 1e6, handler.c_str(), fiber);
        ui_event_line(buf);
    }
}

void mbsim::evlog_undelivered(const Event &evt, const char *why)
{
    activity();
    std::string name = event_name(evt);
    if (opt().headless)
    {
        char buf[512];
        if (why)
            snprintf(buf, sizeof(buf),
                     "{\"id\":%u,\"value\":%u,\"name\":\"%s\",\"queued_ms\":%llu,\"delivered_ms\":null,"
                     "\"handler\":null,\"fiber\":null,\"dropped\":\"%s\"}",
                     evt.source, evt.value, json_escape(name).c_str(),
                     (unsigned long long)(evt.timestamp / 1000), json_escape(why).c_str());
        else
            snprintf(buf, sizeof(buf),
                     "{\"id\":%u,\"value\":%u,\"name\":\"%s\",\"queued_ms\":%llu,\"delivered_ms\":null,"
                     "\"handler\":null,\"fiber\":null}",
                     evt.source, evt.value, json_escape(name).c_str(),
                     (unsigned long long)(evt.timestamp / 1000));
        headless_event(buf);
    }
    else if (why)
    {
        // Events nobody listens for are left out of the on-screen log; drops are shown.
        char buf[512];
        snprintf(buf, sizeof(buf), "%.2f %s queued \xe2\x86\x92 %s", (double)evt.timestamp / 1e6, name.c_str(), why);
        ui_event_line(buf);
    }
}

// ----------------------------------------------------------------------------- outputs

void mbsim::serial_out(const char *text, size_t len)
{
    activity();
    if (opt().headless)
    {
        headless_serial(text, len);
        return;
    }

    {
        std::lock_guard<std::mutex> g(serialLogMu);
        if (!serialLogFile && !opt().serialLog.empty())
            serialLogFile = fopen(opt().serialLog.c_str(), "w");
        if (serialLogFile)
        {
            fwrite(text, 1, len, serialLogFile);
            if (memchr(text, '\n', len))
                fflush(serialLogFile);
        }
    }

    std::lock_guard<std::mutex> g(shared().mu);
    Shared &s = shared();
    for (size_t i = 0; i < len; i++)
    {
        char c = text[i];
        if (c == '\n')
        {
            s.serial.push_back(s.serialPartial);
            s.serialPartial.clear();
            while (s.serial.size() > 200)
                s.serial.pop_front();
        }
        else if (c == '\t')
            s.serialPartial += "    ";
        else if ((unsigned char)c >= 0x20 && c != 0x7f)
            s.serialPartial += c;
    }
}

void mbsim::sound_out(const std::string &name, uint64_t startUs, uint64_t endUs, bool async)
{
    activity();
    if (!opt().headless)
        return;
    char buf[256];
    snprintf(buf, sizeof(buf), "{\"name\":\"%s\",\"start_ms\":%llu,\"end_ms\":%llu,\"async\":%s}",
             json_escape(name).c_str(), (unsigned long long)(startUs / 1000),
             (unsigned long long)(endUs / 1000), async ? "true" : "false");
    headless_sound(buf);
}

void mbsim::sound_status(const std::string &name, uint64_t endUs, bool playing)
{
    activity();
    std::lock_guard<std::mutex> g(shared().mu);
    shared().soundName = name;
    shared().soundEndUs = endUs;
    shared().soundPlaying = playing;
}

// --------------------------------------------------------------------------- lifecycle

void mbsim::main_returned(int)
{
    mainHasReturned = true;
    activity();
    std::lock_guard<std::mutex> g(shared().mu);
    shared().mainReturned = true;
}

bool mbsim::program_finished()
{
    // Nothing is running, nothing is waiting, no input is queued, and it has been quiet
    // for a full simulated second (so async animations and sounds have had time to end).
    if (!mainHasReturned || live_fiber_count() != 0 || !input_idle())
        return false;
    {
        std::lock_guard<std::mutex> g(shared().mu);
        if (shared().soundPlaying)
            return false;
    }
    return raw_now_us() - lastActivityUs > 1000000;
}

void mbsim::finish(int code)
{
    if (finishing.exchange(true))
    {
        // Another thread is already shutting down; let it.
        for (;;)
            pause();
    }

    if (code != EXIT_PANIC)
    {
        flush_frame();
        audio_flush_at_exit();
    }

    {
        std::lock_guard<std::mutex> g(serialLogMu);
        if (serialLogFile)
            fclose(serialLogFile);
        serialLogFile = nullptr;
    }

    if (opt().headless)
        headless_close();
    else
        ui_restore();

    fflush(stdout);
    fflush(stderr);
    _exit(code);
}

// ------------------------------------------------------------------- crashes and signals

static void write_str(int fd, const char *s)
{
    ssize_t r = write(fd, s, strlen(s));
    (void)r;
}

static void crash_handler(int sig)
{
    mbsim::ui_restore();
    const char *name = sig == SIGSEGV ? "SIGSEGV, a bad memory access"
                     : sig == SIGBUS ? "SIGBUS, a bad memory access"
                     : sig == SIGFPE ? "SIGFPE, an arithmetic error such as dividing by zero"
                     : sig == SIGILL ? "SIGILL, an illegal instruction"
                     : "SIGABRT";
    write_str(2, "\nmbsim: the program crashed (");
    write_str(2, name);
    write_str(2, ").\nOften an array index out of range. Rerun with --asan to find the line.\n");
    _exit(mbsim::EXIT_CRASH);
}

static void term_handler(int sig)
{
    mbsim::ui_restore();
    _exit(128 + sig);
}

static void watchdog_handler(int)
{
    if (mbsim::runningFiber.load() == -1)
        return;
    uint64_t stuckNs = mbsim::mono_ns() - mbsim::lastYieldNs.load();
    if (stuckNs > 5000000000ull)
    {
        write_str(2, "mbsim: watchdog: no fiber has yielded for 5 s of real time. "
                     "Is there a loop that never calls uBit.sleep()?\n");
        _exit(mbsim::EXIT_WATCHDOG);
    }
}

static void install_handlers()
{
    static char altStack[64 * 1024];
    stack_t ss;
    ss.ss_sp = altStack;
    ss.ss_size = sizeof(altStack);
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);

#ifdef MBSIM_ASAN
    // The sanitizers report memory errors themselves, with the file and line, and then
    // exit straight away. The mbsim command puts the terminal back afterwards: the
    // sanitizer runtime offers no hook that is called for every kind of error.
    (void)crash_handler;
#else
    sa.sa_handler = crash_handler;
    sa.sa_flags = SA_ONSTACK;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
#endif

    sa.sa_handler = term_handler;
    sa.sa_flags = SA_ONSTACK;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    if (mbsim::opt().headless)
    {
        sa.sa_handler = watchdog_handler;
        sa.sa_flags = SA_ONSTACK | SA_RESTART;
        sigaction(SIGALRM, &sa, NULL);
        struct itimerval tv;
        tv.it_interval.tv_sec = 1;
        tv.it_interval.tv_usec = 0;
        tv.it_value = tv.it_interval;
        setitimer(ITIMER_REAL, &tv, NULL);
    }
}

// ------------------------------------------------------------------------------- entry

static void usage_error(const char *msg)
{
    fprintf(stderr, "mbsim runtime: %s\n(This program is meant to be started by the mbsim command.)\n", msg);
    _exit(mbsim::EXIT_USAGE);
}

static void parse_options(int argc, char **argv)
{
    mbsim::Options &o = mbsim::opt();
    for (int i = 1; i < argc; i++)
    {
        std::string a = argv[i];
        auto value = [&]() -> const char * {
            if (i + 1 >= argc)
                usage_error(("missing value after " + a).c_str());
            return argv[++i];
        };
        if (a == "--headless")
            o.headless = true;
        else if (a == "--seed")
        {
            o.seed = (uint32_t)strtoul(value(), NULL, 0);
            o.seedGiven = true;
        }
        else if (a == "--speed")
        {
            o.speed = atof(value());
            if (o.speed < 0.25)
                o.speed = 0.25;
            if (o.speed > 4)
                o.speed = 4;
        }
        else if (a == "--until")
            o.untilMs = atol(value());
        else if (a == "--script")
            o.script = value();
        else if (a == "--out")
            o.outDir = value();
        else if (a == "--name")
            o.name = value();
        else if (a == "--symbols")
            o.symbols = value();
        else if (a == "--serial-log")
            o.serialLog = value();
        else
            usage_error(("unknown option " + a).c_str());
    }
}

static void main_entry(void *)
{
    char *args[2] = {savedArgv ? savedArgv[0] : (char *)"main", NULL};
    int rc = __real_main(1, args);
    mbsim::main_returned(rc);
}

extern "C" int __wrap_main(int argc, char **argv)
{
    savedArgv = argv;
    parse_options(argc, argv);

    if (mbsim::opt().seedGiven)
        chosenSeed = mbsim::opt().seed;
    else
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        // Keep clock-picked seeds short enough to retype after reading them off the screen.
        chosenSeed = (uint32_t)(((uint64_t)ts.tv_nsec / 1000 + (uint64_t)ts.tv_sec * 7919 + (uint64_t)getpid()) % 1000000) + 1;
        mbsim::opt().seed = chosenSeed;
    }

    if (mbsim::opt().headless)
        mbsim::headless_start();
    else
        mbsim::ui_start();

    install_handlers();

    mbsim::start_clock();
    mbsim::start_main_fiber(main_entry);
    mbsim::run_idle_loop();
}
