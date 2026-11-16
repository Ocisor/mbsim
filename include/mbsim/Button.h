// mbsim: Button and MultiButton, ported from
//   codal-core/inc/driver-models/AbstractButton.h
//   codal-core/inc/drivers/Button.h, MultiButton.h, TouchButton.h
// Copyright (c) 2017 Lancaster University. MIT licence, see LICENSE.
#ifndef MBSIM_BUTTON_H
#define MBSIM_BUTTON_H

#include "CodalConstants.h"
#include "Component.h"
#include "Event.h"

namespace codal
{
    enum ButtonEventConfiguration
    {
        DEVICE_BUTTON_SIMPLE_EVENTS,
        DEVICE_BUTTON_ALL_EVENTS
    };

    class AbstractButton : public CodalComponent
    {
        public:
        uint16_t clickCount;

        AbstractButton();
        virtual ~AbstractButton();

        /** @return 1 if this button is currently pressed, 0 otherwise. */
        virtual int isPressed() = 0;

        /** @return the number of presses since the last call, then resets the count. */
        int wasPressed();

        void enable();
        void disable();
    };

    /**
      * A debounced push button (also used for the touch logo).
      * In mbsim the raw pin level comes from the keyboard or the headless script.
      */
    class Button : public AbstractButton
    {
        unsigned long downStartTime;
        uint8_t sigma;
        ButtonEventConfiguration eventConfiguration;
        int mbsimLine;  // which simulated input line this button reads

        int buttonActive();

        public:
        Button(int mbsimLine, uint16_t id, ButtonEventConfiguration eventConfiguration = DEVICE_BUTTON_ALL_EVENTS);
        ~Button();

        void setEventConfiguration(ButtonEventConfiguration config);
        virtual int isPressed();
        virtual void periodicCallback();
    };

    typedef Button TouchButton;

    class MultiButton : public AbstractButton
    {
        uint16_t button1;
        uint16_t button2;
        ButtonEventConfiguration eventConfiguration;

        uint16_t otherSubButton(uint16_t b);
        int isSubButtonPressed(uint16_t button);
        int isSubButtonHeld(uint16_t button);
        int isSubButtonSupressed(uint16_t button);
        void setButtonState(uint16_t button, int value);
        void setHoldState(uint16_t button, int value);
        void setSupressedState(uint16_t button, int value);
        void onButtonEvent(Event evt);

        public:
        MultiButton(uint16_t button1, uint16_t button2, uint16_t id);

        virtual int isPressed();
        void setEventConfiguration(ButtonEventConfiguration config);
    };
}

#endif
