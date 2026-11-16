// mbsim: the fake MicroBit.h.
//
// Your program includes this exactly as it includes the real one:
//
//     #include "MicroBit.h"
//     MicroBit uBit;
//     int main() { uBit.init(); ... }
//
// The classes below have the same names, members, defaults and return codes as CODAL
// (lancaster-university/codal-core and codal-microbit-v2), but run on your desktop.
// mbsim gets behaviour close, not identical: always build with real CODAL and test on
// the board before relying on the result.
//
// Class layout ported from codal-microbit-v2/model/MicroBit.h
// Copyright (c) 2016 Lancaster University, UK. MIT licence, see LICENSE.
#ifndef MICROBIT_H
#define MICROBIT_H

// The real header pulls these in too, so programs that rely on them still build.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mbsim/CodalConstants.h"
#include "mbsim/ManagedString.h"
#include "mbsim/Event.h"
#include "mbsim/Component.h"
#include "mbsim/Fiber.h"
#include "mbsim/MessageBus.h"
#include "mbsim/Image.h"
#include "mbsim/Display.h"
#include "mbsim/Button.h"
#include "mbsim/Audio.h"
#include "mbsim/Serial.h"
#include "mbsim/Stubs.h"

#define MICROBIT_INITIALIZED 0x01

namespace codal
{
    // ---- helpers from CodalCompat.h / CodalDevice.h / MicroBitDevice.h ------------------
    inline int min(int a, int b) { return (a < b ? a : b); }
    inline int max(int a, int b) { return (a > b ? a : b); }
    inline void *memclr(void *a, size_t b) { return memset(a, 0, b); }

    /** @return a number in [0, max), or DEVICE_INVALID_PARAMETER if max <= 0. */
    int random(int max);
    int seed_random(uint32_t seed);

    /** @return a number in [0, max), or MICROBIT_INVALID_PARAMETER if max <= 0. */
    int microbit_random(int max);
    void microbit_seed_random();
    void microbit_seed_random(uint32_t seed);

    /** Stops everything and shows a sad face followed by the three digits of statusCode. */
    [[noreturn]] void microbit_panic(int statusCode);
    void microbit_panic_timeout(int iterations);
    [[noreturn]] void microbit_reset();
    uint32_t microbit_serial_number();
    const char *microbit_friendly_name();
    [[noreturn]] void target_panic(int statusCode);
    [[noreturn]] void target_reset();

    class MicroBitDevice
    {
        public:
        MicroBitDevice();
        int random(int max);
        void seedRandom();
        int seedRandom(uint32_t seed);
        [[noreturn]] void reset();
        [[noreturn]] void panic(int statusCode);
    };

    /**
      * The micro:bit. Declare exactly one, globally, and call init() first.
      */
    class MicroBit : public MicroBitDevice, public CodalComponent
    {
        private:
        void onListenerRegisteredEvent(Event evt);

        public:
        MessageBus messageBus;
        MicroBitIO io;
        MicroBitSerial serial;
        MicroBitDisplay display;
        Button buttonA;
        Button buttonB;
        MultiButton buttonAB;
        TouchButton logo;
        MicroBitRadio radio;
        MicroBitThermometer thermometer;
        Accelerometer accelerometer;
        Compass compass;
        MicroBitAudio audio;

        MicroBit();

        /**
          * Starts the scheduler and the device drivers, then sleeps for 10 ms.
          * @return DEVICE_OK, or DEVICE_NOT_SUPPORTED if init() was already called.
          */
        int init();

        static ManagedString getName();
        static ManagedString getSerial();

        /**
          * Blocks the calling fiber for at least `milliseconds`, letting other fibers and
          * event handlers run. The granularity is the 4 ms scheduler tick.
          */
        void sleep(uint32_t milliseconds);

        /** @return milliseconds since the program started. */
        unsigned long systemTime();
    };

    inline void MicroBit::sleep(uint32_t milliseconds)
    {
        fiber_sleep(milliseconds);
    }

    inline unsigned long MicroBit::systemTime()
    {
        return (unsigned long)system_timer_current_time();
    }
}

using namespace codal;

// Names kept from the micro:bit v1 runtime, as in MicroBitCompat.h
typedef codal::Event MicroBitEvent;
typedef codal::Listener MicroBitListener;
typedef codal::Image MicroBitImage;
typedef codal::SerialMode MicroBitSerialMode;
typedef codal::CodalComponent MicroBitComponent;
typedef codal::EventLaunchMode MicroBitEventLaunchMode;
typedef codal::BitmapFont MicroBitFont;
typedef codal::Button MicroBitButton;
typedef codal::MultiButton MicroBitMultiButton;
typedef codal::MessageBus MicroBitMessageBus;

#endif
