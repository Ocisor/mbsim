// mbsim: buttons A and B, the A+B multi-button and the touch logo.
// Ported from codal-core/source/drivers/Button.cpp, MultiButton.cpp and
// driver-models/AbstractButton.cpp.
// Copyright (c) 2017 Lancaster University. MIT licence, see LICENSE.
//
// The keyboard (or the headless script) only produces raw "pin went down / pin went up"
// changes with a time stamp. Everything after that - debouncing, and deciding which of
// DOWN, UP, CLICK, LONG_CLICK and HOLD to fire - is CODAL's own logic, run once per
// 4 ms tick exactly as the board runs it from its timer interrupt.

#include "internal.h"

#include <algorithm>

using namespace codal;

// ------------------------------------------------------------------------- input lines

namespace
{
    struct Action
    {
        uint64_t t;
        int line;
        bool down;
    };

    std::mutex inputMu;
    // Function-local so they exist whenever they are first used (see internal.h).
    std::vector<Action> &pending()
    {
        static std::vector<Action> v;
        return v;
    }
    std::vector<Action> &uiPending()
    {
        static std::vector<Action> v;
        return v;
    }
    bool raw[mbsim::LINE_COUNT];
    bool uiRaw[mbsim::LINE_COUNT];
    std::atomic<int> pendingCount{0};

    Button *allButtons[8];
    int buttonCount = 0;
    int unsettled = 0;  // buttons whose debounce counter is not at rest
}

void mbsim::input_push(uint64_t tUs, int line, bool down)
{
    std::lock_guard<std::mutex> g(inputMu);
    Action a{tUs, line, down};
    // Keep both lists sorted by time; equal times stay in the order they were pushed.
    auto pos = std::upper_bound(pending().begin(), pending().end(), a,
                                [](const Action &x, const Action &y) { return x.t < y.t; });
    pending().insert(pos, a);
    auto upos = std::upper_bound(uiPending().begin(), uiPending().end(), a,
                                 [](const Action &x, const Action &y) { return x.t < y.t; });
    uiPending().insert(upos, a);
    pendingCount = (int)pending().size();
}

void mbsim::input_apply(uint64_t tickUs)
{
    if (pendingCount.load() == 0)
        return;
    std::lock_guard<std::mutex> g(inputMu);
    size_t n = 0;
    while (n < pending().size() && pending()[n].t <= tickUs)
    {
        raw[pending()[n].line] = pending()[n].down;
        n++;
    }
    pending().erase(pending().begin(), pending().begin() + n);
    pendingCount = (int)pending().size();
}

bool mbsim::line_active(int line)
{
    return raw[line];
}

bool mbsim::input_idle()
{
    if (pendingCount.load() != 0)
        return false;
    for (int i = 0; i < LINE_COUNT; i++)
        if (raw[i])
            return false;
    return unsettled == 0;
}

bool mbsim::ui_line_state(int line, uint64_t nowUs)
{
    std::lock_guard<std::mutex> g(inputMu);
    size_t n = 0;
    while (n < uiPending().size() && uiPending()[n].t <= nowUs)
    {
        uiRaw[uiPending()[n].line] = uiPending()[n].down;
        n++;
    }
    uiPending().erase(uiPending().begin(), uiPending().begin() + n);
    return uiRaw[line];
}

// ---------------------------------------------------------------------- AbstractButton

AbstractButton::AbstractButton()
{
    clickCount = 0;
    enable();
}

int AbstractButton::wasPressed()
{
    uint16_t c = clickCount;
    clickCount = 0;

    return c;
}

void AbstractButton::enable()
{
    status |= DEVICE_COMPONENT_RUNNING;
}

void AbstractButton::disable()
{
    status &= ~DEVICE_COMPONENT_RUNNING;
}

AbstractButton::~AbstractButton()
{
}

// ------------------------------------------------------------------------------ Button

Button::Button(int mbsimLine, uint16_t id, ButtonEventConfiguration eventConfiguration)
{
    this->id = id;
    this->eventConfiguration = eventConfiguration;
    this->downStartTime = 0;
    this->sigma = 0;
    this->mbsimLine = mbsimLine;

    // For backward compatibility, always perform demand activation.
    this->status |= DEVICE_COMPONENT_STATUS_SYSTEM_TICK;

    if (buttonCount < (int)(sizeof(allButtons) / sizeof(allButtons[0])))
        allButtons[buttonCount++] = this;
}

void Button::setEventConfiguration(ButtonEventConfiguration config)
{
    this->eventConfiguration = config;
}

int Button::buttonActive()
{
    return mbsim::line_active(mbsimLine) ? 1 : 0;
}

void Button::periodicCallback()
{
    // If this button is disabled, do nothing.
    if (!(status & DEVICE_COMPONENT_RUNNING))
        return;

    uint8_t sigmaBefore = sigma;

    //
    // If the pin is pulled low (touched), increment our cumulative counter.
    // otherwise, decrement it. We're essentially building a lazy follower here.
    // This makes the output debounced for buttons, and desensitizes touch sensors
    // (particularly in environments where there is mains noise!)
    //
    if (buttonActive())
    {
        if (sigma < DEVICE_BUTTON_SIGMA_MAX)
            sigma++;
    }
    else
    {
        if (sigma > DEVICE_BUTTON_SIGMA_MIN)
            sigma--;
    }

    // mbsim bookkeeping only: lets headless mode know when every button has settled.
    if (sigmaBefore == 0 && sigma != 0)
        unsettled++;
    if (sigmaBefore != 0 && sigma == 0)
        unsettled--;

    // Check to see if we have off->on state change.
    if (sigma > DEVICE_BUTTON_SIGMA_THRESH_HI && !(status & DEVICE_BUTTON_STATE))
    {
        // Record we have a state change, and raise an event.
        status |= DEVICE_BUTTON_STATE;
        Event evt(id, DEVICE_BUTTON_EVT_DOWN);
        clickCount++;

        // Record the time the button was pressed.
        downStartTime = system_timer_current_time();
    }

    // Check to see if we have on->off state change.
    if (sigma < DEVICE_BUTTON_SIGMA_THRESH_LO && (status & DEVICE_BUTTON_STATE))
    {
        status &= ~DEVICE_BUTTON_STATE;
        status &= ~DEVICE_BUTTON_STATE_HOLD_TRIGGERED;
        Event evt(id, DEVICE_BUTTON_EVT_UP);

        if (eventConfiguration == DEVICE_BUTTON_ALL_EVENTS)
        {
            // determine if this is a long click or a normal click and send event
            if ((system_timer_current_time() - downStartTime) >= DEVICE_BUTTON_LONG_CLICK_TIME)
                Event evt(id, DEVICE_BUTTON_EVT_LONG_CLICK);
            else
                Event evt(id, DEVICE_BUTTON_EVT_CLICK);
        }
    }

    // if button is pressed and the hold triggered event state is not triggered AND we are
    // greater than the button debounce value
    if ((status & DEVICE_BUTTON_STATE) && !(status & DEVICE_BUTTON_STATE_HOLD_TRIGGERED) && (system_timer_current_time() - downStartTime) >= DEVICE_BUTTON_HOLD_TIME)
    {
        // set the hold triggered event flag
        status |= DEVICE_BUTTON_STATE_HOLD_TRIGGERED;

        // fire hold event
        Event evt(id, DEVICE_BUTTON_EVT_HOLD);
    }
}

int Button::isPressed()
{
    // On the board the button state is kept up to date by the timer interrupt, even
    // while user code is busy. Catch up on any ticks that are due before answering.
    mbsim::catch_up();
    return status & DEVICE_BUTTON_STATE ? 1 : 0;
}

Button::~Button()
{
    for (int i = 0; i < buttonCount; i++)
        if (allButtons[i] == this)
            allButtons[i] = allButtons[--buttonCount];
}

// ------------------------------------------------------------------------- MultiButton

MultiButton::MultiButton(uint16_t button1, uint16_t button2, uint16_t id)
{
    this->id = id;
    this->button1 = button1;
    this->button2 = button2;
    this->eventConfiguration = DEVICE_BUTTON_SIMPLE_EVENTS;

    if (EventModel::defaultEventBus)
    {
        mbsim::InternalScope internal;
        EventModel::defaultEventBus->listen(button1, DEVICE_EVT_ANY, this, &MultiButton::onButtonEvent, MESSAGE_BUS_LISTENER_IMMEDIATE);
        EventModel::defaultEventBus->listen(button2, DEVICE_EVT_ANY, this, &MultiButton::onButtonEvent, MESSAGE_BUS_LISTENER_IMMEDIATE);
    }
}

uint16_t MultiButton::otherSubButton(uint16_t b)
{
    return (b == button1 ? button2 : button1);
}

int MultiButton::isSubButtonPressed(uint16_t button)
{
    if (button == button1)
        return status & MULTI_BUTTON_STATE_1;

    if (button == button2)
        return status & MULTI_BUTTON_STATE_2;

    return 0;
}

int MultiButton::isSubButtonHeld(uint16_t button)
{
    if (button == button1)
        return status & MULTI_BUTTON_HOLD_TRIGGERED_1;

    if (button == button2)
        return status & MULTI_BUTTON_HOLD_TRIGGERED_2;

    return 0;
}

int MultiButton::isSubButtonSupressed(uint16_t button)
{
    if (button == button1)
        return status & MULTI_BUTTON_SUPRESSED_1;

    if (button == button2)
        return status & MULTI_BUTTON_SUPRESSED_2;

    return 0;
}

void MultiButton::setButtonState(uint16_t button, int value)
{
    if (button == button1)
    {
        if (value)
            status |= MULTI_BUTTON_STATE_1;
        else
            status &= ~MULTI_BUTTON_STATE_1;
    }

    if (button == button2)
    {
        if (value)
            status |= MULTI_BUTTON_STATE_2;
        else
            status &= ~MULTI_BUTTON_STATE_2;
    }
}

void MultiButton::setHoldState(uint16_t button, int value)
{
    if (button == button1)
    {
        if (value)
            status |= MULTI_BUTTON_HOLD_TRIGGERED_1;
        else
            status &= ~MULTI_BUTTON_HOLD_TRIGGERED_1;
    }

    if (button == button2)
    {
        if (value)
            status |= MULTI_BUTTON_HOLD_TRIGGERED_2;
        else
            status &= ~MULTI_BUTTON_HOLD_TRIGGERED_2;
    }
}

void MultiButton::setSupressedState(uint16_t button, int value)
{
    if (button == button1)
    {
        if (value)
            status |= MULTI_BUTTON_SUPRESSED_1;
        else
            status &= ~MULTI_BUTTON_SUPRESSED_1;
    }

    if (button == button2)
    {
        if (value)
            status |= MULTI_BUTTON_SUPRESSED_2;
        else
            status &= ~MULTI_BUTTON_SUPRESSED_2;
    }
}

void MultiButton::setEventConfiguration(ButtonEventConfiguration config)
{
    this->eventConfiguration = config;
}

void MultiButton::onButtonEvent(Event evt)
{
    int button = evt.source;
    int otherButton = otherSubButton(button);

    switch (evt.value)
    {
        case DEVICE_BUTTON_EVT_DOWN:
            setButtonState(button, 1);
            if (isSubButtonPressed(otherButton))
            {
                Event e(id, DEVICE_BUTTON_EVT_DOWN);
                clickCount++;
            }

            break;

        case DEVICE_BUTTON_EVT_HOLD:
            setHoldState(button, 1);
            if (isSubButtonHeld(otherButton))
                Event e(id, DEVICE_BUTTON_EVT_HOLD);

            break;

        case DEVICE_BUTTON_EVT_UP:
            if (isSubButtonPressed(otherButton))
            {
                Event e(id, DEVICE_BUTTON_EVT_UP);

                if (isSubButtonHeld(button) && isSubButtonHeld(otherButton))
                    Event e(id, DEVICE_BUTTON_EVT_LONG_CLICK);
                else
                    Event e(id, DEVICE_BUTTON_EVT_CLICK);

                setSupressedState(otherButton, 1);
            }
            else if (!isSubButtonSupressed(button) && eventConfiguration == DEVICE_BUTTON_ALL_EVENTS)
            {
                if (isSubButtonHeld(button))
                    Event e(button, DEVICE_BUTTON_EVT_LONG_CLICK);
                else
                    Event e(button, DEVICE_BUTTON_EVT_CLICK);
            }

            setButtonState(button, 0);
            setHoldState(button, 0);
            setSupressedState(button, 0);

            break;
    }
}

int MultiButton::isPressed()
{
    mbsim::catch_up();
    return ((status & MULTI_BUTTON_STATE_1) && (status & MULTI_BUTTON_STATE_2));
}
