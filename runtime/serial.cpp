// mbsim: the serial port. Output only; it goes to the serial pane and a log file.
// printf() and the transmit path are ported from codal-core/source/driver-models/Serial.cpp
// Copyright (c) 2017 Lancaster University. MIT licence, see LICENSE.

#include "internal.h"

using namespace codal;

MicroBitSerial::MicroBitSerial()
{
    this->id = DEVICE_ID_SERIAL;
    this->status |= DEVICE_COMPONENT_STATUS_SYSTEM_TICK;
    this->txEmptyPending = false;
    this->baudrate = MICROBIT_SERIAL_DEFAULT_BAUD_RATE;
}

void MicroBitSerial::periodicCallback()
{
    // Serial::dataTransmitted(): the transmit buffer has drained; unblock any waiting fiber.
    // On the board this happens ~1.7 ms after the buffer fills; here, at the next tick.
    if (txEmptyPending)
    {
        txEmptyPending = false;
        Event(DEVICE_ID_NOTIFY, CODAL_SERIAL_EVT_TX_EMPTY);
    }
}

int MicroBitSerial::putc(char c)
{
    mbsim::serial_out(&c, 1);
    return 1;
}

int MicroBitSerial::getc()
{
    return DEVICE_NO_DATA;
}

int MicroBitSerial::sendChar(char c, SerialMode mode)
{
    return send((uint8_t *)&c, 1, mode);
}

int MicroBitSerial::send(ManagedString s, SerialMode mode)
{
    return send((uint8_t *)s.toCharArray(), s.length(), mode);
}

int MicroBitSerial::send(uint8_t *buffer, int bufferLen, SerialMode mode)
{
    if (bufferLen <= 0 || buffer == NULL)
        return DEVICE_INVALID_PARAMETER;

    // Serial::setTxInterrupt(): bytes are copied into a 20 byte ring buffer (19 usable).
    // When it fills, a SYNC_SLEEP sender blocks until it has drained - a yield point.
    const int usable = CODAL_SERIAL_DEFAULT_BUFFER_SIZE - 1;
    int copiedBytes = 0;
    int inBuffer = 0;

    while (copiedBytes < bufferLen)
    {
        if (inBuffer == usable)
        {
            if (mode == SYNC_SLEEP)
            {
                txEmptyPending = true;
                if (fiber_wait_for_event(DEVICE_ID_NOTIFY, CODAL_SERIAL_EVT_TX_EMPTY) == DEVICE_NOT_SUPPORTED)
                    txEmptyPending = false;
            }

            if (mode == ASYNC)
                break;

            inBuffer = 0;
        }

        mbsim::serial_out((const char *)&buffer[copiedBytes], 1);
        inBuffer++;
        copiedBytes++;
    }

    return copiedBytes;
}

void MicroBitSerial::printf(const char *format, ...)
{
    va_list arg;
    va_start(arg, format);

    const char *end = format;

    char buff[20];
    while (*end)
    {
        char current = *end++;
        if (current == '%')
        {
            // CODAL reads every argument as one 32-bit word. A 64-bit desktop passes
            // pointers in 64 bits, so mbsim reads %s separately; nothing else differs.
            char conv = *end;
            if (conv == 0)
                break;
            end++;

            uint32_t val = 0;
            const char *str = NULL;
            if (conv == 's')
                str = va_arg(arg, const char *);
            else if (conv != '%')
                val = va_arg(arg, uint32_t);

            char *buffPtr = buff;
            char c = 0;
            bool firstDigitFound = false;
            bool lowerCase = false;
            switch (conv)
            {
            case 'c':
                putc((char)val);
                break;
            case 'd':
                memset(buff, 0, 20);
                snprintf(buff, sizeof(buff), "%d", (int)val);  // itoa()
                while ((c = *buffPtr++) != 0)
                    putc(c);
                break;

            case 's':
                while (str && (c = *str++) != 0)
                    putc(c);
                break;

            case '%':
                putc('%');
                break;

            case 'x':
                lowerCase = true;
                // fall through
            case 'X':
                for (uint8_t i = 8; i > 0; i--)
                {
                    uint8_t digit = ((uint8_t)(val >> ((i - 1) * 4)) & 0x0f) + (uint8_t)'0';
                    if (digit > '9')
                    {
                        if (lowerCase)
                            digit += 39;
                        else
                            digit += 7;
                    }
                    if (digit != '0')
                    {
                        putc((char)digit);
                        firstDigitFound = true;
                    }
                    else if (firstDigitFound || i == 1)
                        putc((char)digit);
                }
                break;
            case 'p':
            default:
                mbsim::note_once("serial-printf",
                                 "uBit.serial.printf only understands %%c %%d %%s %%x %%X %%%%. \"%%%c\" prints "
                                 "\"???\", here and on the board.", conv);
                putc('?');
                putc('?');
                putc('?');
                break;
            }
        }
        else
            putc(current);
    }

    va_end(arg);
}

int MicroBitSerial::read(SerialMode)
{
    mbsim::notSimulated("serial input");
    return DEVICE_NO_DATA;
}

ManagedString MicroBitSerial::read(int, SerialMode)
{
    mbsim::notSimulated("serial input");
    return ManagedString();
}

int MicroBitSerial::read(uint8_t *, int, SerialMode)
{
    mbsim::notSimulated("serial input");
    return DEVICE_NO_DATA;
}

ManagedString MicroBitSerial::readUntil(ManagedString, SerialMode)
{
    mbsim::notSimulated("serial input");
    return ManagedString();
}

int MicroBitSerial::setBaud(int baudrate)
{
    this->baudrate = baudrate;
    return DEVICE_OK;
}

int MicroBitSerial::isReadable() { return 0; }
int MicroBitSerial::isWriteable() { return 1; }
int MicroBitSerial::setRxBufferSize(uint8_t) { return DEVICE_OK; }
int MicroBitSerial::setTxBufferSize(uint8_t) { return DEVICE_OK; }
int MicroBitSerial::getRxBufferSize() { return CODAL_SERIAL_DEFAULT_BUFFER_SIZE; }
int MicroBitSerial::getTxBufferSize() { return CODAL_SERIAL_DEFAULT_BUFFER_SIZE; }
int MicroBitSerial::clearRxBuffer() { return DEVICE_OK; }
int MicroBitSerial::clearTxBuffer() { return DEVICE_OK; }
int MicroBitSerial::rxBufferedSize() { return 0; }
int MicroBitSerial::txBufferedSize() { return 0; }
