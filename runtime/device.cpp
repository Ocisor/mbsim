// mbsim: ManagedString, CodalComponent, the random number generator, the MicroBit
// device class and microbit_panic.
// Ported from codal-core/source/types/ManagedString.cpp, core/CodalComponent.cpp,
// core/CodalCompat.cpp (random), codal-microbit-v2/model/MicroBit.cpp (init,
// onListenerRegisteredEvent) and source/MicroBitDevice.cpp (panic display).
// Copyright (c) 2016-2017 Lancaster University. MIT licence, see LICENSE.

#include "internal.h"

#include <unistd.h>

using namespace codal;

// ----------------------------------------------------------------------- ManagedString

static const std::shared_ptr<const std::string> &emptyStringData()
{
    static const std::shared_ptr<const std::string> e = std::make_shared<const std::string>();
    return e;
}

ManagedString ManagedString::EmptyString;

ManagedString::ManagedString() : ptr(emptyStringData()) {}

ManagedString::ManagedString(const char *str)
{
    // Sanity check. Return EmptyString for anything distasteful
    if (str == NULL || *str == 0)
        ptr = emptyStringData();
    else
        ptr = std::make_shared<const std::string>(str);
}

ManagedString::ManagedString(const int value)
{
    ptr = std::make_shared<const std::string>(std::to_string(value));
}

ManagedString::ManagedString(const char value)
{
    ptr = std::make_shared<const std::string>(1, value);
}

ManagedString::ManagedString(const char *str, const int16_t length)
{
    // Sanity check. Return EmptyString for anything distasteful
    if (str == NULL || *str == 0 || length <= 0 || (size_t)length > strlen(str))
        ptr = emptyStringData();
    else
        ptr = std::make_shared<const std::string>(str, (size_t)length);
}

ManagedString::ManagedString(const ManagedString &s) : ptr(s.ptr) {}

ManagedString::ManagedString(const ManagedString &s1, const ManagedString &s2)
{
    ptr = std::make_shared<const std::string>(*s1.ptr + *s2.ptr);
}

ManagedString::~ManagedString() {}

ManagedString &ManagedString::operator=(const ManagedString &s)
{
    ptr = s.ptr;
    return *this;
}

bool ManagedString::operator==(const ManagedString &s)
{
    return ((length() == s.length()) && (strcmp(toCharArray(), s.toCharArray()) == 0));
}

bool ManagedString::operator!=(const ManagedString &s)
{
    return !(*this == s);
}

bool ManagedString::operator<(const ManagedString &s)
{
    return (strcmp(toCharArray(), s.toCharArray()) < 0);
}

bool ManagedString::operator>(const ManagedString &s)
{
    return (strcmp(toCharArray(), s.toCharArray()) > 0);
}

ManagedString ManagedString::substring(int16_t start, int16_t length)
{
    // If the parameters are illegal, just return a reference to the empty string.
    if (start >= this->length())
        return ManagedString();

    // Compute a safe copy length;
    length = codal::min(this->length() - start, length);

    // Build a ManagedString from this.
    return ManagedString(toCharArray() + start, length);
}

ManagedString codal::operator+(const ManagedString &lhs, const ManagedString &rhs)
{
    // If the either string is empty, nothing to do!
    if (rhs.length() == 0)
        return lhs;

    if (lhs.length() == 0)
        return rhs;

    return ManagedString(lhs, rhs);
}

char ManagedString::charAt(int16_t index)
{
    return (index >= 0 && index < length()) ? (*ptr)[index] : 0;
}

// ---------------------------------------------------------------------- CodalComponent

CodalComponent *CodalComponent::components[DEVICE_COMPONENT_COUNT];

CodalComponent::CodalComponent()
{
    this->id = 0;
    this->status = 0;
    addComponent();
}

CodalComponent::CodalComponent(uint16_t id, uint16_t status)
{
    this->id = id;
    this->status = status;
    addComponent();
}

CodalComponent::~CodalComponent()
{
    status = 0;
    removeComponent();
}

void CodalComponent::addComponent()
{
    for (int i = 0; i < DEVICE_COMPONENT_COUNT; i++)
    {
        if (components[i] == NULL)
        {
            components[i] = this;
            return;
        }
    }
}

void CodalComponent::removeComponent()
{
    for (int i = 0; i < DEVICE_COMPONENT_COUNT; i++)
    {
        if (components[i] == this)
        {
            components[i] = NULL;
            return;
        }
    }
}

// ------------------------------------------------------------------------------ random

static uint32_t random_value = 0;
static MicroBitDevice *microbit_device_instance = NULL;

int codal::seed_random(uint32_t seed)
{
    random_value = seed;
    return DEVICE_OK;
}

int codal::random(int max)
{
    uint32_t m, result;

    if (max <= 0)
        return DEVICE_INVALID_PARAMETER;

    if (random_value == 0)
        seed_random(0xC0DA1);

    // Our maximum return value is actually one less than passed
    max--;

    do
    {
        m = (uint32_t)max;
        result = 0;
        do
        {
            // Cycle the LFSR (Linear Feedback Shift Register).
            // We use an optimal sequence with a period of 2^32-1, as defined by Bruce Schneier here
            // (a true legend in the field!),
            // For those interested, it's documented in his paper:
            // "Pseudo-Random Sequence Generator for 32-Bit CPUs: A fast, machine-independent
            // generator for 32-bit Microprocessors"
            // https://www.schneier.com/paper-pseudorandom-sequence.html
            uint32_t rnd = random_value;

            rnd = ((((rnd >> 31) ^ (rnd >> 6) ^ (rnd >> 4) ^ (rnd >> 2) ^ (rnd >> 1) ^ rnd) &
                    0x0000001)
                   << 31) |
                  (rnd >> 1);

            random_value = rnd;

            result = ((result << 1) | (rnd & 0x00000001));
        } while (m >>= 1);
    } while (result > (uint32_t)max);

    return result;
}

int codal::microbit_random(int max)
{
    if (max <= 0)
        mbsim::note_once("random-max", "microbit_random(%d): max must be at least 1. Returned "
                                       "MICROBIT_INVALID_PARAMETER (%d), as CODAL does.",
                         max, DEVICE_INVALID_PARAMETER);

    return microbit_device_instance ? microbit_device_instance->random(max) : 0;
}

void codal::microbit_seed_random()
{
    if (microbit_device_instance)
        microbit_device_instance->seedRandom();
}

void codal::microbit_seed_random(uint32_t seed)
{
    if (microbit_device_instance)
        microbit_device_instance->seedRandom(seed);
}

MicroBitDevice::MicroBitDevice()
{
    microbit_device_instance = this;
}

int MicroBitDevice::random(int max)
{
    return codal::random(max);
}

// On the board this reads the hardware random number generator. Here it uses the run's
// seed (--seed, or one picked from the clock), so a run can be repeated exactly.
//
// The seed is scrambled first. CODAL's generator is a shift register, and a small number
// such as --seed 7 fed straight into it yields a long run of zeros before it warms up;
// the board never sees that because its hardware seed uses all 32 bits.
void MicroBitDevice::seedRandom()
{
    static uint32_t draws = 0;
    uint32_t r = mbsim::pick_seed() + 0x9E3779B9u * draws++;

    // 32-bit finaliser from MurmurHash3 (public domain, Austin Appleby).
    r ^= r >> 16;
    r *= 0x85EBCA6Bu;
    r ^= r >> 13;
    r *= 0xC2B2AE35u;
    r ^= r >> 16;

    seedRandom(r);
}

int MicroBitDevice::seedRandom(uint32_t seed)
{
    return codal::seed_random(seed);
}

void MicroBitDevice::reset()
{
    microbit_reset();
}

void MicroBitDevice::panic(int statusCode)
{
    microbit_panic(statusCode);
}

uint32_t codal::microbit_serial_number()
{
    return 0x6D627369;  // "mbsi"
}

const char *codal::microbit_friendly_name()
{
    return "mbsim";
}

// ---------------------------------------------------------------------------- MicroBit

MicroBit::MicroBit()
    : messageBus(),
      io(),
      serial(),
      display(),
      buttonA(mbsim::LINE_A, DEVICE_ID_BUTTON_A, DEVICE_BUTTON_ALL_EVENTS),
      buttonB(mbsim::LINE_B, DEVICE_ID_BUTTON_B, DEVICE_BUTTON_ALL_EVENTS),
      buttonAB(DEVICE_ID_BUTTON_A, DEVICE_ID_BUTTON_B, DEVICE_ID_BUTTON_AB),
      logo(mbsim::LINE_LOGO, ID_PIN_LOGO, DEVICE_BUTTON_ALL_EVENTS),
      audio()
{
    // Clear our status
    status = 0;
}

int MicroBit::init()
{
    if (status & MICROBIT_INITIALIZED)
        return DEVICE_NOT_SUPPORTED;

    status |= MICROBIT_INITIALIZED;

    // Bring up fiber scheduler.
    scheduler_init(messageBus);

    for (int i = 0; i < DEVICE_COMPONENT_COUNT; i++)
    {
        if (CodalComponent::components[i])
            CodalComponent::components[i]->init();
    }

    // Seed our random number generator
    seedRandom();

    // Create an event handler to trap any handlers being created for I2C services.
    // We do this to enable initialisation of those services only when they're used,
    // which saves processor time, memeory and battery life.
    {
        mbsim::InternalScope internal;
        messageBus.listen(DEVICE_ID_MESSAGE_BUS_LISTENER, DEVICE_EVT_ANY, this, &MicroBit::onListenerRegisteredEvent);
    }

    // Deschedule for a little while, just to allow for any components that finialise
    // initialisation as a background task, and to allow the power mamanger to repsonse to
    // background events from the KL27 before any user code begins running.
    sleep(10);

    return DEVICE_OK;
}

void MicroBit::onListenerRegisteredEvent(Event evt)
{
    switch (evt.value)
    {
        case DEVICE_ID_BUTTON_AB:
            // A user has registered to receive events from the buttonAB multibutton.
            // Disable click events from being generated by ButtonA and ButtonB, and defer the
            // control of this to the multibutton handler.
            //
            // This way, buttons look independent unless a buttonAB is requested, at which
            // point button A+B clicks can be correclty handled without breaking
            // causal ordering.
            buttonA.setEventConfiguration(DEVICE_BUTTON_SIMPLE_EVENTS);
            buttonB.setEventConfiguration(DEVICE_BUTTON_SIMPLE_EVENTS);
            buttonAB.setEventConfiguration(DEVICE_BUTTON_ALL_EVENTS);
            break;

        case DEVICE_ID_COMPASS:
            mbsim::notSimulated("compass");
            break;

        case DEVICE_ID_ACCELEROMETER:
        case DEVICE_ID_GESTURE:
            mbsim::notSimulated("accelerometer");
            break;

        case DEVICE_ID_THERMOMETER:
            mbsim::notSimulated("thermometer");
            break;

        case DEVICE_ID_SYSTEM_LEVEL_DETECTOR:
        case DEVICE_ID_SYSTEM_MICROPHONE:
            mbsim::notSimulated("microphone");
            break;
    }
}

ManagedString MicroBit::getName()
{
    return ManagedString(microbit_friendly_name());
}

ManagedString MicroBit::getSerial()
{
    return ManagedString((int)microbit_serial_number());
}

int mbsim::notSimulated(const char *what)
{
    note_once(what, "%s: not simulated (calls return 0).", what);
    return 0;
}

// ------------------------------------------------------------------------------- panic

// Constants for microbit_panic, from MicroBitDevice.cpp.
// length of message: face, code digit, code digit, code digit
#define microbit_panic_MSGLEN 4
#define microbit_panic_MSG1STDIGIT 1
#define microbit_panic_DIVMAX 100
#define microbit_panic_DIVBASE 10
// Each character is scanned 48 times ("1000 ~ 25s", so ~25 ms a scan): 40 lit, 8 blank.
#define microbit_panic_SCANS 48
#define microbit_panic_CLEARSCANS 8
#define MBSIM_PANIC_SCAN_MS 25

static const uint8_t panic_face[BITMAP_FONT_WIDTH] = {0x1B, 0x1B, 0x0, 0x0E, 0x11};
static int panic_timeout = 0;

void codal::microbit_panic_timeout(int iterations)
{
    panic_timeout = iterations;
}

void mbsim::do_panic(int statusCode)
{
    {
        std::lock_guard<std::mutex> g(shared().mu);
        shared().panicCode = statusCode;
    }
    note("microbit_panic(%d): the program has stopped. On the board the display now loops a "
         "sad face and the digits %03d.", statusCode, statusCode % 1000);
    flush_frame();
    audio_flush_at_exit();
    display_register(nullptr);

    uint64_t tMs = sim_now_us() / 1000;
    const int litMs = (microbit_panic_SCANS - microbit_panic_CLEARSCANS) * MBSIM_PANIC_SCAN_MS;
    const int clearMs = microbit_panic_CLEARSCANS * MBSIM_PANIC_SCAN_MS;

    liveMode = DISPLAY_MODE_BLACK_AND_WHITE;
    liveEnabled = true;

    for (int repeat = 0; panic_timeout == 0 || repeat < panic_timeout; repeat++)
    {
        for (int msgIdx = 0; msgIdx < microbit_panic_MSGLEN; msgIdx++)
        {
            // find the the current character and its font bytes
            int chr = 0;
            if (msgIdx >= microbit_panic_MSG1STDIGIT)
            {
                // calculate divisor for this digit: 100s, 10s or units
                int div = microbit_panic_DIVMAX;
                for (int digit = msgIdx - microbit_panic_MSG1STDIGIT; digit > 0; digit--)
                    div /= microbit_panic_DIVBASE;
                chr = 0x30 + (statusCode / div) % microbit_panic_DIVBASE;
            }

            const uint8_t *fontBytes = chr ? (BitmapFont::defaultFont + BITMAP_FONT_WIDTH * (chr - BITMAP_FONT_ASCII_START)) : panic_face;

            // dim display after 5 repetitions
            int brightness = repeat >= 5 ? 25 : 255;
            uint8_t lit[25], blank[25] = {0};
            for (int row = 0; row < 5; row++)
                for (int col = 0; col < 5; col++)
                    lit[row * 5 + col] = (fontBytes[row] & (1 << (4 - col))) ? brightness : 0;

            if (opt().headless)
            {
                headless_frame(tMs, DISPLAY_MODE_BLACK_AND_WHITE, brightness, lit);
                tMs += litMs;
                headless_frame(tMs, DISPLAY_MODE_BLACK_AND_WHITE, brightness, blank);
                tMs += clearMs;
            }
            else
            {
                liveBrightness = brightness;
                for (int i = 0; i < 25; i++)
                    livePixels[i] = lit[i] ? 255 : 0;
                usleep(litMs * 1000);
                for (int i = 0; i < 25; i++)
                    livePixels[i] = 0;
                usleep(clearMs * 1000);
            }
        }

        // Headless: one pass of the message is enough to check; then report the panic.
        if (opt().headless)
            finish(EXIT_PANIC);
    }

    // microbit_panic_timeout() was set: the board would reset here.
    finish(opt().headless ? EXIT_PANIC : EXIT_RESET);
}

void codal::microbit_panic(int statusCode)
{
    mbsim::do_panic(statusCode);
}

void codal::target_panic(int statusCode)
{
    mbsim::do_panic(statusCode);
}

void codal::microbit_reset()
{
    if (mbsim::opt().headless)
    {
        mbsim::note("microbit_reset() called: on the board the program would restart. Stopping here.");
        mbsim::finish(mbsim::EXIT_OK);
    }
    mbsim::finish(mbsim::EXIT_RESET);
}

void codal::target_reset()
{
    microbit_reset();
}
