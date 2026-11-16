# mbsim — run your micro:bit CODAL program on your desktop

mbsim compiles your own CODAL `main.cpp` on Linux and runs it with no micro:bit attached.
It swaps the real `MicroBit.h` for a fake one with the same classes and calls, and draws
the 5×5 display in your terminal. Your source is used exactly as it is.

> **mbsim gets behaviour close, not identical. Always do a real CODAL build and test on
> the board before relying on the result.** See [Known fidelity limits](#known-fidelity-limits).

mbsim never writes into your source folder: all build output goes to `~/.cache/mbsim/`.

```text
 mbsim  main.cpp    t=12.48s   seed=1234   1x   GREYSCALE

        0   1   2   3   4
   0   ··  ██  ··  ··  ··
   1   ··  ██  ··  ··  ··
   2   ··  ··  ··  ██  ··          [A]  [B]  logo ○
   3   ··  ··  ··  ··  ··
   4   ··  ··  ··  ··  ··

 ♪ giggle (playing, 0.6s left)          volume 255   (timing only, no sound)
 serial │ count=3
 events │ 12.40 BUTTON_A CLICK queued → 12.41 delivered to onButtonA (fiber 3)

 a/b click  A/B long click  z/x hold  space A+B  l logo  r reset  p freeze  e events  g grid  q quit
```

## Install

Ubuntu 24.04 on x86_64 is the target; any Linux with glibc and g++ 11 or newer should
work. On Windows, use WSL2 running Ubuntu, in Windows Terminal (there is no native Windows
build).

```sh
sudo apt install build-essential                      # needed for everything
sudo apt install g++-arm-linux-gnueabihf qemu-user    # only for assembly mode (.S files)
sudo apt install gdb-multiarch                        # only for --gdb

make                 # builds the runtime library into ~/.cache/mbsim/
./mbsim doctor       # checks the tools above and your terminal's colours
```

If your Ubuntu only offers `qemu-user-static`, install that instead: mbsim accepts either
`qemu-arm` or `qemu-arm-static`, and `mbsim doctor` tells you which it found.

## Use

```sh
./mbsim run path/to/main.cpp       # one file
./mbsim run path/to/source/        # a folder: every .c, .cpp and .S in it is built
./mbsim check path/to/source/      # compile only, with -Wall -Wextra
```

| Command / option | What it does |
| --- | --- |
| `mbsim run <path>` | Build, then open the terminal simulator |
| `mbsim check <path>` | Compile only, with `-Wall -Wextra`; print warnings |
| `mbsim doctor` | Check dependencies and the terminal's colour support |
| `--seed N` | Fix the random seed so a run can be repeated (the seed in use is always shown on screen) |
| `--speed F` | Scale simulated time, 0.25–4 (default 1) |
| `--arm` | Force assembly mode (automatic when a `.S` file is present) |
| `--asan` | Build with AddressSanitizer and UBSan to catch out-of-bounds array access (native mode only) |
| `--gdb` | Assembly mode: wait for `gdb-multiarch` on port 1234 |
| `--headless ...` | Run without a UI for automated tests (see [Headless mode](#headless-mode)) |

If your program is split over several files (for example `main.cpp` plus a `.S` file),
pass the folder. A `samples/` subfolder is skipped, and a `main.cpp` that still has
`#include "samples/..."` is refused: replace CODAL's demo `main.cpp` with your own program.

### Keys

| Key | Action |
| --- | --- |
| `a` / `b` | Click button A / B (held for 120 ms) |
| `A` / `B` (Shift) | Long click (held for 1100 ms) |
| `z` / `x` | Toggle holding A / B down, so you can make any press ordering |
| `Space` | Press both: A down, B down 20 ms later, A up at 140 ms, B up at 160 ms |
| `l` | Touch the logo (120 ms) |
| `r` | Reset: restart the program, like the button on the back of the board |
| `p` | Freeze or unfreeze simulated time (a debugging aid) |
| `e` | Show or hide the event log |
| `g` | Show or hide the x/y labels around the grid |
| `q` or `Ctrl+C` | Quit |

`x` is the column (0–4, left to right) and `y` is the row (0–4, top to bottom), as in
`uBit.display.image.setPixelValue(x, y, value)`.

Serial output (`uBit.serial.printf`, `send`) appears in the serial pane and in full in
`~/.cache/mbsim/serial.log`. On the real board it is only visible in a serial terminal at
115200 baud. Anything your program prints with plain `printf` is held back while the UI is
open and shown when it closes.

## The thing to understand: handlers only run when your code yields

CODAL is **cooperative**. A fiber runs until it blocks, and an event handler can only
start when nothing else is running. If `main()` loops without calling `uBit.sleep()`, no
button handler runs, however many times you press. mbsim copies this, because timing bugs
that depend on it must show up here the same way they do on the board.

- A switch between fibers happens only at a blocking call: `uBit.sleep`, `fiber_sleep`,
  `schedule()`, blocking `scroll` / `print`, `soundExpressions.play`, `release_fiber`, or a
  handler returning. Nothing is ever interrupted part-way.
- The event log (`e`) shows, for each event, when it was **queued** and when it was
  **delivered** to your handler. A gap between the two is time your code spent not
  yielding.
- If the running fiber hasn't yielded for more than 2 s, the screen says so:
  *"main() hasn't yielded for N s. Events can't be delivered until it calls uBit.sleep()."*
  The display and `q` still work, because the UI is a separate thread that never runs
  your code.

The scheduler, message bus, buttons and display animations are ports of CODAL's own
source (`lancaster-university/codal-core` and `codal-microbit-v2`), not re-inventions, so
details such as these carry over:

- Time moves in 4 ms scheduler ticks. `uBit.sleep(10)` wakes at the next tick at or after
  10 ms, and even `uBit.sleep(0)` waits for the next tick.
- Buttons are debounced exactly as CODAL does it: `DOWN` arrives about 32 ms after the
  press and `UP`/`CLICK` about 40 ms after the release. A long click is a release after
  1000 ms; `HOLD` fires at 1500 ms.
- Registering any listener for `MICROBIT_ID_BUTTON_AB` (after `uBit.init()`) changes how A
  and B behave: pressing both then gives one A+B click and no separate A or B click.
- With the default `MESSAGE_BUS_LISTENER_QUEUE_IF_BUSY`, events that arrive while a handler
  is still running wait and run afterwards, one at a time. With `DROP_IF_BUSY` they are
  lost.
- `uBit.serial.printf` is CODAL's own small formatter: only `%c %d %s %x %X %%` work.
  Anything else (`%u`, `%f`, `%ld`, `%5d`) prints `???`, here and on the board.

## Headless mode

Headless mode runs a program with scripted input and no UI, then writes what happened to
files. With a fixed seed it is exactly repeatable: the clock is virtual and jumps straight
to the next moment something is due.

```sh
./mbsim run <path> --headless --seed 7 --script input.txt --until 10000 --out run/
```

The script has one action per line, at a simulated time in ms; `#` starts a comment.

```text
# time_ms  action
500   click A          # down, then up 120 ms later
1200  longclick A      # held for 1100 ms
2000  down A
2020  down B
2140  up A
2160  up B
3000  touch LOGO
4000  click AB         # both buttons, with the same timing as the Space key
```

| File in `--out` | Contents |
| --- | --- |
| `frames.txt` | One block per display change: `t=<ms> mode=<BW\|GREY> bright=<0-255>`, then 5 rows of 5 values. The values are what each LED shows (0–255): in `BW` any non-zero pixel shows at the overall brightness; in `GREY` it is pixel × brightness ÷ 255. |
| `events.jsonl` | One JSON object per delivery of an event to a handler: `id`, `value`, `name`, `queued_ms`, `delivered_ms`, `handler`, `fiber`. An event with two listeners gives two lines. An event nobody listens for gives one line with `null` for the last three; a dropped event also has `"dropped"`. |
| `serial.txt` | Serial output |
| `sound.jsonl` | `name`, `start_ms`, `end_ms`, `async` |

| Exit code | Meaning |
| --- | --- |
| 0 | Reached `--until` (default 10000 ms), or the program finished |
| 2 | Compile error (or a bad command line or script) |
| 3 | `microbit_panic` |
| 4 | Crash |
| 5 | Watchdog: no fiber yielded for 5 s of real time |

Without `--out`, the files go to `~/.cache/mbsim/runs/`. The run ends early, with code 0,
once `main()` has returned, no fiber is left, no input is queued and nothing has happened
for a simulated second.

Because headless time only moves when the program waits, each call to `uBit.systemTime()`
advances it by one microsecond, so a loop that polls the clock still ends. A loop that
never looks at the clock and never sleeps is stopped by the watchdog.

## Assembly mode

A `.S` file contains ARM Thumb code, which an x86 laptop can't run. When a `.S` file is
present (or with `--arm`), mbsim builds the **whole program** for 32-bit ARM Linux and
runs it under QEMU user-mode emulation. Everything else works the same, including the
live display.

- Your C++ and the runtime are compiled with `arm-linux-gnueabihf-g++ -march=armv7ve+fp
  -mthumb -static`, and each `.S` goes through the same driver with `-mthumb`, so `sdiv`
  and `udiv` are accepted as they are on the Cortex-M4.
- `.cpu`, `.arch` and `.fpu` lines that name an M-profile core (such as `.cpu cortex-m4`)
  are commented out **in a copy** in the build folder, because they clash with a desktop
  ARM binary. mbsim prints one line saying what it ignored. Your file is never changed.
- If a `.S` file contains `.arm`, mbsim warns: *"The micro:bit's Cortex-M4 can only run
  Thumb code. This file won't work on the board as written."* (It still runs here.)
- The `extern "C"` declaration works unchanged: arguments arrive in r0–r3, the result
  returns in r0, and r4–r11 must be preserved, on both cores.
- `--gdb` starts the program paused and prints the `gdb-multiarch` command to attach from
  a second terminal. You can then `break` on your function, `stepi`, and
  `info registers`, as in CPUlator. Line numbers refer to your own `.S` file.

## Catching array mistakes: `--asan`

Writing past the end of an array silently corrupts other data on the board. Here it may
crash or may appear to work. `./mbsim run <path> --asan` rebuilds with AddressSanitizer and
UBSan, stops at the first bad access, and names the file and line in your code. It works
in native mode only, so not together with a `.S` file.

## Lifecycle

| Situation | What mbsim does |
| --- | --- |
| `main()` returns | The scheduler keeps running, so handlers and fibers still work; the status line shows *main() returned*. Whether the real board does the same is an open question (below). |
| `microbit_panic(code)` | Everything stops; the display loops a sad face and the three digits of the code until `r` or `q`. |
| Crash | The terminal is restored, then: *"Often an array index out of range. Rerun with --asan to find the line."* |
| `r` | The program starts again from the top, with a new seed unless `--seed` was given. |
| `q`, `Ctrl+C`, SIGTERM | The terminal is always restored. |

## What is simulated

Simulated: `uBit.init`, `sleep`, `systemTime`; `uBit.display` (image, modes, brightness,
`scroll`, `print` and their `Async` forms, `stopAnimation`, image animations);
`uBit.messageBus` (`listen`, `ignore`, all flags); `uBit.buttonA`, `buttonB`, `buttonAB`,
`uBit.logo`; `uBit.audio` (volume and sound-expression timing); `uBit.serial` (output);
`microbit_random`, `microbit_panic`; `create_fiber`, `release_fiber`, `fiber_sleep`,
`schedule`, `invoke`; `MicroBitEvent`, `ManagedString`, `MicroBitImage`; and the
`MICROBIT_*` / `DEVICE_*` constants.

Not simulated (they compile, return 0 and show a one-time note): accelerometer, compass,
radio, edge-connector pins, microphone, thermometer, light sensing, serial input.

## Known fidelity limits

| Area | Real micro:bit v2 | mbsim |
| --- | --- | --- |
| Speed | 64 MHz Cortex-M4. Heavy loops between yields take visible time. | Code between yields takes almost no time. A lag on the board won't show here. |
| Memory errors | 128 KB RAM. Out-of-bounds writes silently corrupt other data or hard-fault. | May crash, may not. Use `--asan` to catch them reliably. |
| Compiler | `arm-none-eabi-g++` with CODAL's flags | Desktop `g++` with CODAL's language flags (`-std=c++11 -fno-exceptions -fno-rtti`). Warnings differ. In native mode `long` and pointers are 64-bit here but 32-bit on the board; `int` is 32-bit on both. |
| Buttons | Physical presses, bounce, truly simultaneous presses | Keyboard presses with fixed hold times |
| Display | Multiplexed LEDs with a non-linear brightness curve | Red terminal blocks with a linear map (a value of 15 is dim but visible); light sensing not modelled |
| Sound | Real tones | Timing only; nothing is played. |
| Sound length | Set by the synthesizer's sample buffers | The sum of the segment durations in CODAL's data, including CODAL's random variation |
| Assembly | Thumb-2 on a Cortex-M4 | Thumb-2 on an emulated Cortex-A15. The common instruction subset matches; M-profile system instructions (`cpsid`, special `msr`) don't. ARM-mode code runs here but not on the board. |
| Interrupt timing | Timer and button interrupts fire at any moment | The 4 ms tick work is done whenever your code enters the scheduler or reads the time or a button, each tick with its own exact time. Results are the same; only when the host CPU does the work differs. |
| Tick phase | Sleep wake-ups and component ticks are separate timers with their own phase | One 4 ms tick, starting at time 0 |
| Fiber stacks | CODAL's own stack handling | Fixed 64 KB per fiber (512 KB with `--asan`), with a guard page (overflow is reported as a crash) |
| Start-up | `uBit.init()` takes longer: the Bluetooth build sleeps a further 100 ms before the final 10 ms | `uBit.init()` returns at 12 ms (a 10 ms sleep, rounded up to the 4 ms tick) |
| Serial | 115200 baud; a `send` longer than 19 bytes blocks about 1.7 ms per 19 bytes | A long `send` yields until the next 4 ms tick; `printf` never yields (as CODAL) |
| Random numbers | Seeded from the hardware generator | CODAL's generator, seeded from `--seed` or the clock |
| Before `uBit.init()` | Varies | Sleeps spin, events run their handlers immediately, and using the display shows a warning. Call `uBit.init()` first. |
| Other sensors and pins | Work | Stubs returning 0 |

### Open questions to check on the board

- What happens when `main()` returns?
- How long do `giggle` and `sad` actually play for, compared with mbsim's lengths?
- What does `microbit_random(0)` return? CODAL's source says `MICROBIT_INVALID_PARAMETER`
  (-1001), and that is what mbsim returns (with a warning).
- What does `soundExpressions.play("some unknown name")` do? Reading CODAL, it waits for a
  sound that never starts. mbsim warns and returns at once.

## Layout

```text
mbsim                    the command (Python 3)
include/MicroBit.h       the fake header your code includes
include/mbsim/           ManagedString, Event, constants, device classes
runtime/scheduler.cpp    fibers (ucontext), clock, 4 ms tick, idle loop
runtime/messagebus.cpp   ported from codal-core
runtime/buttons.cpp      Button and MultiButton port, logo, input queue
runtime/display.cpp      image, modes, AnimatedDisplay port, pendolino3 font
runtime/audio.cpp        sound expression data and timing
runtime/serial.cpp       serial output, CODAL's printf
runtime/device.cpp       MicroBit, random numbers, panic, ManagedString
runtime/main.cpp         entry point, event log, exit paths, crash handling
ui/terminal.cpp          renderer and keyboard thread
ui/headless.cpp          script reader and output files
```

`include/mbsim/CodalConstants.h`, `runtime/pendolino3.inc` and `runtime/sound_data.inc`
are generated from the CODAL repositories and record the commits they came from. Nothing
in them is typed by hand.

## Licence

MIT. Parts of mbsim are ported from CODAL (Lancaster University; sound expression data
British Broadcasting Corporation), which is also MIT licensed. See `LICENSE`; each ported
file names the CODAL file it came from.
