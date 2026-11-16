// mbsim: headless mode. Reads the input script and writes what happened to files.
// There is no UI thread: the script feeds the input queue up front and the clock is
// virtual, so a run with a fixed seed is exactly repeatable.

#include "../runtime/internal.h"

#include <cerrno>
#include <cstring>
#include <unistd.h>

namespace
{
    FILE *framesFile = nullptr;
    FILE *eventsFile = nullptr;
    FILE *serialFile = nullptr;
    FILE *soundFile = nullptr;

    FILE *open_out(const char *name)
    {
        std::string path = mbsim::opt().outDir + "/" + name;
        FILE *f = fopen(path.c_str(), "w");
        if (!f)
        {
            fprintf(stderr, "mbsim: cannot write %s: %s\n", path.c_str(), strerror(errno));
            _exit(mbsim::EXIT_USAGE);
        }
        return f;
    }

    [[noreturn]] void script_error(int lineNo, const char *msg)
    {
        fprintf(stderr, "%s:%d: %s\n", mbsim::opt().script.c_str(), lineNo, msg);
        fprintf(stderr, "Script lines look like:  <time_ms> <click|longclick|down|up|touch> <A|B|LOGO>\n"
                        "(\"click AB\" presses both buttons together.)\n");
        _exit(mbsim::EXIT_USAGE);
    }

    int line_for(const char *target)
    {
        if (strcmp(target, "A") == 0)
            return mbsim::LINE_A;
        if (strcmp(target, "B") == 0)
            return mbsim::LINE_B;
        if (strcmp(target, "LOGO") == 0)
            return mbsim::LINE_LOGO;
        return -1;
    }

    void load_script()
    {
        FILE *f = fopen(mbsim::opt().script.c_str(), "r");
        if (!f)
        {
            fprintf(stderr, "mbsim: cannot read script %s: %s\n", mbsim::opt().script.c_str(), strerror(errno));
            _exit(mbsim::EXIT_USAGE);
        }

        char line[512];
        int lineNo = 0;
        while (fgets(line, sizeof(line), f))
        {
            lineNo++;
            char *hash = strchr(line, '#');
            if (hash)
                *hash = 0;

            long timeMs;
            char action[32], target[32];
            int n = sscanf(line, "%ld %31s %31s", &timeMs, action, target);
            if (n <= 0)
            {
                // blank or comment-only line, unless there is stray text on it
                char *p = line;
                while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
                    p++;
                if (*p)
                    script_error(lineNo, "expected a time in milliseconds at the start of the line");
                continue;
            }
            if (n != 3)
                script_error(lineNo, "expected: <time_ms> <action> <target>");
            if (timeMs < 0)
                script_error(lineNo, "the time must not be negative");

            uint64_t t = (uint64_t)timeMs * 1000;
            int ln = line_for(target);
            bool both = strcmp(target, "AB") == 0;

            if (strcmp(action, "click") == 0 && both)
            {
                // The same sequence as the Space key.
                mbsim::input_push(t, mbsim::LINE_A, true);
                mbsim::input_push(t + 20000, mbsim::LINE_B, true);
                mbsim::input_push(t + 140000, mbsim::LINE_A, false);
                mbsim::input_push(t + 160000, mbsim::LINE_B, false);
                continue;
            }

            if (ln < 0)
                script_error(lineNo, "unknown target (use A, B or LOGO)");

            if (strcmp(action, "click") == 0 || strcmp(action, "touch") == 0)
            {
                mbsim::input_push(t, ln, true);
                mbsim::input_push(t + 120000, ln, false);
            }
            else if (strcmp(action, "longclick") == 0)
            {
                mbsim::input_push(t, ln, true);
                mbsim::input_push(t + 1100000, ln, false);
            }
            else if (strcmp(action, "down") == 0)
                mbsim::input_push(t, ln, true);
            else if (strcmp(action, "up") == 0)
                mbsim::input_push(t, ln, false);
            else
                script_error(lineNo, "unknown action (use click, longclick, down, up or touch)");
        }
        fclose(f);
    }
}

void mbsim::headless_start()
{
    if (opt().outDir.empty())
    {
        fprintf(stderr, "mbsim: headless mode needs --out <folder>\n");
        _exit(EXIT_USAGE);
    }
    framesFile = open_out("frames.txt");
    eventsFile = open_out("events.jsonl");
    serialFile = open_out("serial.txt");
    soundFile = open_out("sound.jsonl");

    if (!opt().script.empty())
        load_script();
}

// Every record is flushed as it is written, so the files are complete even if the
// program crashes or the watchdog stops it.

void mbsim::headless_frame(uint64_t tMs, int mode, int brightness, const uint8_t *lit)
{
    if (!framesFile)
        return;
    bool grey = mode == DISPLAY_MODE_GREYSCALE || mode == DISPLAY_MODE_GREYSCALE_LIGHT_SENSE;
    fprintf(framesFile, "t=%llu mode=%s bright=%d\n", (unsigned long long)tMs, grey ? "GREY" : "BW", brightness);
    for (int row = 0; row < 5; row++)
        fprintf(framesFile, "%d %d %d %d %d\n", lit[row * 5], lit[row * 5 + 1], lit[row * 5 + 2],
                lit[row * 5 + 3], lit[row * 5 + 4]);
    fputc('\n', framesFile);
    fflush(framesFile);
}

void mbsim::headless_event(const std::string &jsonLine)
{
    if (!eventsFile)
        return;
    fprintf(eventsFile, "%s\n", jsonLine.c_str());
    fflush(eventsFile);
}

void mbsim::headless_serial(const char *text, size_t len)
{
    if (!serialFile)
        return;
    fwrite(text, 1, len, serialFile);
    fflush(serialFile);
}

void mbsim::headless_sound(const std::string &jsonLine)
{
    if (!soundFile)
        return;
    fprintf(soundFile, "%s\n", jsonLine.c_str());
    fflush(soundFile);
}

void mbsim::headless_note(const char *text)
{
    fprintf(stderr, "mbsim: %s\n", text);
}

void mbsim::headless_close()
{
    for (FILE **f : {&framesFile, &eventsFile, &serialFile, &soundFile})
    {
        if (*f)
            fclose(*f);
        *f = nullptr;
    }
}
