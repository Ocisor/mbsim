// mbsim: the terminal UI. One thread that reads keys and redraws 30 times a second.
//
// This thread never runs user code. It talks to the simulation thread only through the
// input queue (key presses, treated like hardware interrupts) and by reading the display
// and status snapshots. That is why `q` and the redraw keep working even when the user's
// program is stuck in a loop.
//
// Drawing uses plain ANSI escape codes on /dev/tty (no ncurses). The program's own
// stdout and stderr are captured while the UI is up, and printed when it closes.

#include "../runtime/internal.h"

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>

using namespace codal;

namespace
{
    int tty = -1;
    struct termios savedTermios;
    std::atomic<bool> uiActive{false};
    int savedStdout = -1;
    int savedStderr = -1;
    int captureFd = -1;

    bool trueColour = false;
    bool showEvents = true;
    bool showGrid = true;
    bool holdA = false;
    bool holdB = false;

    void tty_write(const char *s, size_t n)
    {
        while (n > 0)
        {
            ssize_t w = write(tty, s, n);
            if (w <= 0)
                return;
            s += w;
            n -= (size_t)w;
        }
    }

    // Cuts a line to `width` visible columns. Escape sequences take no room and every
    // UTF-8 character used here is one column wide.
    std::string clip(const std::string &s, int width)
    {
        std::string out;
        int col = 0;
        size_t i = 0;
        while (i < s.size())
        {
            unsigned char c = (unsigned char)s[i];
            if (c == 0x1b)
            {
                size_t j = i + 1;
                if (j < s.size() && s[j] == '[')
                {
                    j++;
                    while (j < s.size() && !(s[j] >= '@' && s[j] <= '~'))
                        j++;
                    j++;
                }
                out.append(s, i, j - i);
                i = j;
                continue;
            }
            size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xe ? 3 : 4;
            if (col >= width)
                break;
            out.append(s, i, len);
            col++;
            i += len;
        }
        out += "\x1b[0m";
        return out;
    }

    std::string led(int level)
    {
        if (level <= 0)
            return "\x1b[38;5;240m\xc2\xb7\xc2\xb7\x1b[0m";  // dim grey dots: off, but visible
        int r = 60 + (195 * level) / 255;
        char buf[64];
        if (trueColour)
            snprintf(buf, sizeof(buf), "\x1b[38;2;%d;0;0m\xe2\x96\x88\xe2\x96\x88\x1b[0m", r);
        else
        {
            // 256-colour fallback: the five reds of the 6x6x6 colour cube.
            static const int cube[] = {52, 88, 124, 160, 196};
            int idx = (r - 60) * 5 / 196;
            snprintf(buf, sizeof(buf), "\x1b[38;5;%dm\xe2\x96\x88\xe2\x96\x88\x1b[0m", cube[idx > 4 ? 4 : idx]);
        }
        return buf;
    }

    void render()
    {
        struct winsize ws;
        int width = 100;
        int height = 40;
        if (ioctl(tty, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        {
            width = ws.ws_col;
            if (ws.ws_row > 0)
                height = ws.ws_row;
        }

        uint64_t now = mbsim::raw_now_us();
        int mode = mbsim::liveMode.load();
        int brightness = mbsim::liveBrightness.load();
        bool enabled = mbsim::liveEnabled.load();
        bool frozen = mbsim::is_frozen();

        // Copy the status snapshot once, under the lock.
        std::deque<std::string> serial, events, notes;
        std::string serialPartial, soundName;
        uint64_t soundEnd;
        bool soundPlaying, mainReturned;
        int panicCode;
        {
            std::lock_guard<std::mutex> g(mbsim::shared().mu);
            mbsim::Shared &s = mbsim::shared();
            size_t n = s.serial.size();
            for (size_t i = n > 8 ? n - 8 : 0; i < n; i++)
                serial.push_back(s.serial[i]);
            serialPartial = s.serialPartial;
            n = s.events.size();
            for (size_t i = n > 6 ? n - 6 : 0; i < n; i++)
                events.push_back(s.events[i]);
            n = s.notes.size();
            for (size_t i = n > 3 ? n - 3 : 0; i < n; i++)
                notes.push_back(s.notes[i]);
            if (!showEvents)
                events.clear();
            soundName = s.soundName;
            soundEnd = s.soundEndUs;
            soundPlaying = s.soundPlaying;
            mainReturned = s.mainReturned;
            panicCode = s.panicCode;
        }
        if (!serialPartial.empty())
        {
            serial.push_back(serialPartial);
            if (serial.size() > 8)
                serial.pop_front();
        }

        std::vector<std::string> lines;
        char buf[512];

        const char *modeName = (mode == DISPLAY_MODE_GREYSCALE || mode == DISPLAY_MODE_GREYSCALE_LIGHT_SENSE) ? "GREYSCALE" : "BLACK & WHITE";
        std::string head;
        snprintf(buf, sizeof(buf), " \x1b[1mmbsim\x1b[0m  %s    t=%.2fs   seed=%u   %.2gx   %s",
                 mbsim::opt().name.c_str(), (double)now / 1e6, mbsim::opt().seed, mbsim::opt().speed, modeName);
        head = buf;
        if (brightness != 255)
        {
            snprintf(buf, sizeof(buf), "   brightness %d", brightness);
            head += buf;
        }
        if (!enabled)
            head += "   DISPLAY OFF";
        if (frozen)
            head += "   \x1b[7m FROZEN \x1b[0m";
        if (panicCode >= 0)
        {
            snprintf(buf, sizeof(buf), "   \x1b[1;31mPANIC %03d\x1b[0m", panicCode % 1000);
            head += buf;
        }
        else if (mainReturned)
            head += "   main() returned";
        lines.push_back(head);
        lines.push_back("");

        if (showGrid)
            lines.push_back("        0   1   2   3   4");
        bool a = mbsim::ui_line_state(mbsim::LINE_A, now);
        bool b = mbsim::ui_line_state(mbsim::LINE_B, now);
        bool logo = mbsim::ui_line_state(mbsim::LINE_LOGO, now);
        for (int row = 0; row < 5; row++)
        {
            std::string l;
            if (showGrid)
            {
                snprintf(buf, sizeof(buf), "   %d   ", row);
                l = buf;
            }
            else
                l = "       ";
            for (int col = 0; col < 5; col++)
            {
                int px = mbsim::livePixels[row * 5 + col].load(std::memory_order_relaxed);
                l += led(enabled ? mbsim::lit_level(px, mode, brightness) : 0);
                l += "  ";
            }
            if (row == 2)
            {
                l += "        ";
                l += a ? "\x1b[7m[A]\x1b[0m" : "[A]";
                l += "  ";
                l += b ? "\x1b[7m[B]\x1b[0m" : "[B]";
                l += "  logo ";
                l += logo ? "\x1b[33m\xe2\x97\x8f\x1b[0m" : "\xe2\x97\x8b";
            }
            lines.push_back(l);
        }
        lines.push_back("");

        int volume = mbsim::liveVolume.load();
        std::string vol;
        if (volume == 0)
            vol = "muted";
        else
        {
            snprintf(buf, sizeof(buf), "volume %d", volume);
            vol = buf;
        }
        if (soundPlaying)
        {
            double left = soundEnd > now ? (double)(soundEnd - now) / 1e6 : 0.0;
            snprintf(buf, sizeof(buf), " \xe2\x99\xaa %s (playing, %.1fs left)", soundName.c_str(), left);
        }
        else
            snprintf(buf, sizeof(buf), " \xe2\x99\xaa \xe2\x80\x94");
        std::string snd = buf;
        // pad by visible length (the note and dash are multi-byte, one column each)
        int visible = 0;
        for (unsigned char c : snd)
            if ((c & 0xC0) != 0x80)
                visible++;
        snd += std::string(visible < 40 ? 40 - visible : 2, ' ');
        lines.push_back(snd + vol + "   (timing only, no sound)");

        std::string warning;
        int rf = mbsim::runningFiber.load();
        if (rf != -1 && panicCode < 0)
        {
            double stuck = (double)(mbsim::mono_ns() - mbsim::lastYieldNs.load()) / 1e9;
            if (stuck > 2.0)
            {
                if (rf == 0)
                    snprintf(buf, sizeof(buf), " \x1b[1;33m! main() hasn't yielded for %.0f s. Events can't be delivered until it calls uBit.sleep().\x1b[0m", stuck);
                else
                    snprintf(buf, sizeof(buf), " \x1b[1;33m! A handler (fiber %d) hasn't yielded for %.0f s. Events can't be delivered until it calls uBit.sleep().\x1b[0m", rf, stuck);
                warning = buf;
            }
        }

        std::string help;
        if (panicCode >= 0)
            help = " \x1b[2mr reset  q quit\x1b[0m";
        else if (width >= 102)
            help = " \x1b[2ma/b click  A/B long click  z/x hold  space A+B  l logo  r reset  p freeze  e events  g grid  q quit\x1b[0m";
        else
            help = " \x1b[2ma/b A/B=long z/x=hold space=A+B l=logo r=reset p=freeze e g q=quit\x1b[0m";

        // Fit the screen: the panes give up their oldest lines before anything scrolls.
        // (A frame taller than the terminal would scroll it and make the display flicker.)
        int fixed = (int)lines.size() + (warning.empty() ? 0 : 1) + 2;  // + blank line + help
        size_t serialN = serial.empty() ? 1 : serial.size();
        size_t eventsN = showEvents ? (events.empty() ? 1 : events.size()) : 0;
        size_t notesN = notes.size();
        while (fixed + (int)(serialN + eventsN + notesN) > height)
        {
            if (eventsN > 1 && eventsN >= serialN)
                eventsN--;
            else if (serialN > 1)
                serialN--;
            else if (notesN > 1)
                notesN--;
            else if (eventsN > 0)
                eventsN--;
            else if (notesN > 0)
                notesN--;
            else
                break;
        }
        while (serial.size() > serialN)
            serial.pop_front();
        while (events.size() > eventsN)
            events.pop_front();
        while (notes.size() > notesN)
            notes.pop_front();

        if (serial.empty())
            lines.push_back(" serial \xe2\x94\x82");
        for (const std::string &s : serial)
            lines.push_back(" serial \xe2\x94\x82 " + s);
        if (eventsN > 0 && events.empty())
            lines.push_back(" events \xe2\x94\x82");
        for (const std::string &s : events)
            lines.push_back(" events \xe2\x94\x82 " + s);
        for (const std::string &s : notes)
            lines.push_back(" \x1b[33mnote\x1b[0m   \xe2\x94\x82 " + s);
        if (!warning.empty())
            lines.push_back(warning);
        if ((int)lines.size() + 2 <= height)
            lines.push_back("");
        lines.push_back(help);

        // A very small window: keep the top (status line and display) and the key help.
        if ((int)lines.size() > height && height >= 2)
        {
            lines.resize((size_t)height - 1);
            lines.push_back(help);
        }

        std::string out = "\x1b[H";
        for (size_t i = 0; i < lines.size(); i++)
        {
            out += clip(lines[i], width);
            out += "\x1b[K";
            if (i + 1 < lines.size())
                out += "\r\n";  // never a newline after the last line: it could scroll
        }
        out += "\x1b[J";
        tty_write(out.data(), out.size());
    }

    void press(int line, uint64_t now, uint64_t holdUs)
    {
        mbsim::input_push(now, line, true);
        mbsim::input_push(now + holdUs, line, false);
    }

    void handle_key(unsigned char k)
    {
        uint64_t now = mbsim::raw_now_us();
        switch (k)
        {
            case 'a': press(mbsim::LINE_A, now, 120000); break;
            case 'b': press(mbsim::LINE_B, now, 120000); break;
            case 'A': press(mbsim::LINE_A, now, 1100000); break;
            case 'B': press(mbsim::LINE_B, now, 1100000); break;
            case 'z':
                holdA = !holdA;
                mbsim::input_push(now, mbsim::LINE_A, holdA);
                break;
            case 'x':
                holdB = !holdB;
                mbsim::input_push(now, mbsim::LINE_B, holdB);
                break;
            case ' ':
                mbsim::input_push(now, mbsim::LINE_A, true);
                mbsim::input_push(now + 20000, mbsim::LINE_B, true);
                mbsim::input_push(now + 140000, mbsim::LINE_A, false);
                mbsim::input_push(now + 160000, mbsim::LINE_B, false);
                break;
            case 'l': press(mbsim::LINE_LOGO, now, 120000); break;
            case 'r': mbsim::finish(mbsim::EXIT_RESET);
            case 'p': mbsim::toggle_freeze(); break;
            case 'e': showEvents = !showEvents; break;
            case 'g': showGrid = !showGrid; break;
            case 'q':
            case 3:  // Ctrl+C (the terminal is in raw mode, so it arrives as a byte)
                mbsim::finish(mbsim::EXIT_OK);
        }
    }

    void *ui_thread(void *)
    {
        // Signals are handled on the simulation thread.
        sigset_t all;
        sigfillset(&all);
        sigdelset(&all, SIGSEGV);
        sigdelset(&all, SIGBUS);
        sigdelset(&all, SIGFPE);
        sigdelset(&all, SIGILL);
        pthread_sigmask(SIG_BLOCK, &all, NULL);

        for (;;)
        {
            struct pollfd pfd = {tty, POLLIN, 0};
            int r = poll(&pfd, 1, 33);
            if (r > 0 && (pfd.revents & POLLIN))
            {
                unsigned char keys[64];
                ssize_t n = read(tty, keys, sizeof(keys));
                if (n <= 0)
                    mbsim::finish(mbsim::EXIT_OK);  // the terminal went away
                for (ssize_t i = 0; i < n; i++)
                {
                    if (keys[i] == 0x1b)
                        break;  // arrow keys and the like: ignore the whole sequence
                    handle_key(keys[i]);
                }
            }
            else if (r > 0)
                mbsim::finish(mbsim::EXIT_OK);  // hang-up or error on the terminal
            render();
        }
        return NULL;
    }
}

void mbsim::ui_start()
{
    tty = open("/dev/tty", O_RDWR | O_CLOEXEC);
    if (tty < 0 || tcgetattr(tty, &savedTermios) != 0)
    {
        fprintf(stderr, "mbsim: this needs an interactive terminal. For scripts and tests use --headless.\n");
        _exit(EXIT_USAGE);
    }

    const char *ct = getenv("COLORTERM");
    trueColour = ct && (strcmp(ct, "truecolor") == 0 || strcmp(ct, "24bit") == 0);

    // While the UI owns the screen, send the program's own stdout/stderr to a temporary
    // file so a stray printf can't scramble the display. It is printed on exit.
    char path[] = "/tmp/mbsim-stdio-XXXXXX";
    captureFd = mkstemp(path);
    if (captureFd >= 0)
    {
        unlink(path);
        fflush(stdout);
        fflush(stderr);
        savedStdout = dup(1);
        savedStderr = dup(2);
        dup2(captureFd, 1);
#ifndef MBSIM_ASAN
        dup2(captureFd, 2);
#else
        // With --asan, stderr is left alone: a sanitizer report must not vanish into the
        // capture file when the sanitizer ends the process. The mbsim command sends
        // stderr to a log file for these runs and prints it afterwards.
#endif
    }

    struct termios raw = savedTermios;
    cfmakeraw(&raw);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(tty, TCSANOW, &raw);
    uiActive = true;
    atexit(ui_restore);

    const char *enter = "\x1b[?1049h\x1b[?25l\x1b[2J";  // alternate screen, hide cursor, clear
    tty_write(enter, strlen(enter));

    pthread_t th;
    pthread_create(&th, NULL, ui_thread, NULL);
    pthread_detach(th);
}

// Leaves the alternate screen, shows the cursor and resets termios. Safe to call twice,
// from any thread, and from a signal handler (only write/tcsetattr/dup2/read are used).
void mbsim::ui_restore()
{
    if (!uiActive.exchange(false))
        return;

    const char *leave = "\x1b[0m\x1b[?25h\x1b[?1049l";
    tty_write(leave, strlen(leave));
    tcsetattr(tty, TCSANOW, &savedTermios);

    if (captureFd >= 0)
    {
        if (savedStdout >= 0)
            dup2(savedStdout, 1);
        if (savedStderr >= 0)
            dup2(savedStderr, 2);

        // Replay whatever the program printed while the UI was up.
        if (lseek(captureFd, 0, SEEK_SET) == 0)
        {
            char buf[4096];
            ssize_t n;
            bool first = true;
            while ((n = read(captureFd, buf, sizeof(buf))) > 0)
            {
                if (first)
                {
                    const char *hdr = "--- the program's stdout/stderr ---\n";
                    ssize_t w = write(2, hdr, strlen(hdr));
                    (void)w;
                    first = false;
                }
                ssize_t off = 0;
                while (off < n)
                {
                    ssize_t w = write(2, buf + off, (size_t)(n - off));
                    if (w <= 0)
                        break;
                    off += w;
                }
            }
        }
    }
}
