// mbsim: hardware that is NOT simulated. Everything here compiles, returns 0 and
// shows a one-time "not simulated" note in the status area.
#ifndef MBSIM_STUBS_H
#define MBSIM_STUBS_H

#include <stdint.h>
#include "CodalConstants.h"
#include "ManagedString.h"

namespace mbsim
{
    /** Shows `what` once in the status area as "<what> is not simulated". Returns 0. */
    int notSimulated(const char *what);
}

namespace codal
{
    struct Sample3D
    {
        int x, y, z;
        Sample3D() : x(0), y(0), z(0) {}
    };

    class Accelerometer
    {
        public:
        int getX() { return mbsim::notSimulated("accelerometer"); }
        int getY() { return mbsim::notSimulated("accelerometer"); }
        int getZ() { return mbsim::notSimulated("accelerometer"); }
        int getPitch() { return mbsim::notSimulated("accelerometer"); }
        int getRoll() { return mbsim::notSimulated("accelerometer"); }
        float getPitchRadians() { return (float)mbsim::notSimulated("accelerometer"); }
        float getRollRadians() { return (float)mbsim::notSimulated("accelerometer"); }
        uint16_t getGesture() { return (uint16_t)mbsim::notSimulated("accelerometer"); }
        Sample3D getSample() { mbsim::notSimulated("accelerometer"); return Sample3D(); }
        int setPeriod(int) { return mbsim::notSimulated("accelerometer"); }
        int getPeriod() { return mbsim::notSimulated("accelerometer"); }
        int setRange(int) { return mbsim::notSimulated("accelerometer"); }
        int getRange() { return mbsim::notSimulated("accelerometer"); }
        int requestUpdate() { return mbsim::notSimulated("accelerometer"); }
    };
    typedef Accelerometer MicroBitAccelerometer;

    class Compass
    {
        public:
        int heading() { return mbsim::notSimulated("compass"); }
        int getX() { return mbsim::notSimulated("compass"); }
        int getY() { return mbsim::notSimulated("compass"); }
        int getZ() { return mbsim::notSimulated("compass"); }
        int getFieldStrength() { return mbsim::notSimulated("compass"); }
        int calibrate() { return mbsim::notSimulated("compass"); }
        int isCalibrated() { return mbsim::notSimulated("compass"); }
        int isCalibrating() { return mbsim::notSimulated("compass"); }
        void clearCalibration() { mbsim::notSimulated("compass"); }
        int setPeriod(int) { return mbsim::notSimulated("compass"); }
        int getPeriod() { return mbsim::notSimulated("compass"); }
        int requestUpdate() { return mbsim::notSimulated("compass"); }
    };
    typedef Compass MicroBitCompass;

    class MicroBitThermometer
    {
        public:
        int getTemperature() { return mbsim::notSimulated("thermometer"); }
        void setPeriod(int) { mbsim::notSimulated("thermometer"); }
        int getPeriod() { return mbsim::notSimulated("thermometer"); }
    };

    class MicroBitRadioDatagram
    {
        public:
        int recv(uint8_t *, int) { return mbsim::notSimulated("radio"); }
        ManagedString recv() { mbsim::notSimulated("radio"); return ManagedString(); }
        int send(uint8_t *, int) { return mbsim::notSimulated("radio"); }
        int send(ManagedString) { return mbsim::notSimulated("radio"); }
    };

    class MicroBitRadioEvent
    {
        public:
        int listen(uint16_t, uint16_t) { return mbsim::notSimulated("radio"); }
        int ignore(uint16_t, uint16_t) { return mbsim::notSimulated("radio"); }
    };

    class MicroBitRadio
    {
        public:
        MicroBitRadioDatagram datagram;
        MicroBitRadioEvent event;
        int enable() { return mbsim::notSimulated("radio"); }
        int disable() { return mbsim::notSimulated("radio"); }
        int setGroup(uint8_t) { return mbsim::notSimulated("radio"); }
        int setTransmitPower(int) { return mbsim::notSimulated("radio"); }
        int setFrequencyBand(int) { return mbsim::notSimulated("radio"); }
        int getRSSI() { return mbsim::notSimulated("radio"); }
        int dataReady() { return mbsim::notSimulated("radio"); }
    };

    enum class PullMode : uint8_t
    {
        None = 0,
        Down,
        Up
    };

    /** An edge connector pin. Not simulated: writes are ignored and reads return 0. */
    class MicroBitPin
    {
        public:
        uint16_t id;
        MicroBitPin(uint16_t id = 0) : id(id) {}
        int setDigitalValue(int) { return mbsim::notSimulated("edge connector pins"); }
        int getDigitalValue() { return mbsim::notSimulated("edge connector pins"); }
        int getDigitalValue(PullMode) { return mbsim::notSimulated("edge connector pins"); }
        int setAnalogValue(int) { return mbsim::notSimulated("edge connector pins"); }
        int getAnalogValue() { return mbsim::notSimulated("edge connector pins"); }
        int setServoValue(int, int = 0, int = 0) { return mbsim::notSimulated("edge connector pins"); }
        int setAnalogPeriod(int) { return mbsim::notSimulated("edge connector pins"); }
        int setAnalogPeriodUs(uint32_t) { return mbsim::notSimulated("edge connector pins"); }
        uint32_t getAnalogPeriodUs() { return (uint32_t)mbsim::notSimulated("edge connector pins"); }
        int getAnalogPeriod() { return mbsim::notSimulated("edge connector pins"); }
        int isTouched() { return mbsim::notSimulated("edge connector pins"); }
        int setPull(PullMode) { return mbsim::notSimulated("edge connector pins"); }
        int isInput() { return mbsim::notSimulated("edge connector pins"); }
        int isOutput() { return mbsim::notSimulated("edge connector pins"); }
        int isDigital() { return mbsim::notSimulated("edge connector pins"); }
        int isAnalog() { return mbsim::notSimulated("edge connector pins"); }
        int eventOn(int) { return mbsim::notSimulated("edge connector pins"); }
    };
    typedef MicroBitPin NRF52Pin;
    typedef MicroBitPin Pin;

    class MicroBitIO
    {
        public:
        MicroBitPin P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15, P16,
            P19, P20, speaker, microphone, runmic, logo;
        MicroBitIO()
            : P0(ID_PIN_P0), P1(ID_PIN_P1), P2(ID_PIN_P2), P3(ID_PIN_P3), P4(ID_PIN_P4),
              P5(ID_PIN_P5), P6(ID_PIN_P6), P7(ID_PIN_P7), P8(ID_PIN_P8), P9(ID_PIN_P9),
              P10(ID_PIN_P10), P11(ID_PIN_P11), P12(ID_PIN_P12), P13(ID_PIN_P13),
              P14(ID_PIN_P14), P15(ID_PIN_P15), P16(ID_PIN_P16), P19(ID_PIN_P19),
              P20(ID_PIN_P20), logo(ID_PIN_LOGO) {}
    };
}

#endif
