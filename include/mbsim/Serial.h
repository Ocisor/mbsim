// mbsim: the USB serial port. API ported from codal-core/inc/driver-models/Serial.h
// Copyright (c) 2017 Lancaster University. MIT licence, see LICENSE.
#ifndef MBSIM_SERIAL_H
#define MBSIM_SERIAL_H

#include <stdint.h>
#include "CodalConstants.h"
#include "Component.h"
#include "ManagedString.h"

#define CODAL_SERIAL_DEFAULT_BUFFER_SIZE 20
#define DEVICE_DEFAULT_SERIAL_MODE SYNC_SLEEP
#define MICROBIT_SERIAL_DEFAULT_BAUD_RATE 115200

namespace codal
{
    enum SerialMode
    {
        ASYNC,
        SYNC_SPINWAIT,
        SYNC_SLEEP
    };

    /**
      * Output appears in mbsim's serial pane. On the real board it is only visible in a
      * serial terminal at 115200 baud. Nothing is ever received: reads return DEVICE_NO_DATA.
      */
    class MicroBitSerial : public CodalComponent
    {
        bool txEmptyPending;
        int baudrate;

        public:
        MicroBitSerial();
        virtual void periodicCallback();

        int putc(char c);
        int getc();

        int sendChar(char c, SerialMode mode = DEVICE_DEFAULT_SERIAL_MODE);
        int send(ManagedString s, SerialMode mode = DEVICE_DEFAULT_SERIAL_MODE);
        int send(uint8_t *buffer, int bufferLen, SerialMode mode = DEVICE_DEFAULT_SERIAL_MODE);

        /**
          * CODAL's own small formatter, NOT the C library's. It understands only
          * %c %d %s %x %X and %%. Anything else (%u, %f, %ld, %5d ...) prints "???".
          */
        void printf(const char *format, ...);

        int read(SerialMode mode = DEVICE_DEFAULT_SERIAL_MODE);
        ManagedString read(int size, SerialMode mode = DEVICE_DEFAULT_SERIAL_MODE);
        int read(uint8_t *buffer, int bufferLen, SerialMode mode = DEVICE_DEFAULT_SERIAL_MODE);
        ManagedString readUntil(ManagedString delimeters, SerialMode mode = DEVICE_DEFAULT_SERIAL_MODE);

        int setBaud(int baudrate);
        int isReadable();
        int isWriteable();
        int setRxBufferSize(uint8_t size);
        int setTxBufferSize(uint8_t size);
        int getRxBufferSize();
        int getTxBufferSize();
        int clearRxBuffer();
        int clearTxBuffer();
        int rxBufferedSize();
        int txBufferedSize();
    };
}

#endif
