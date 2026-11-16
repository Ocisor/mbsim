// mbsim: Event, ported from codal-core/inc/types/Event.h
// Copyright (c) 2017 Lancaster University. MIT licence, see LICENSE.
#ifndef MBSIM_EVENT_H
#define MBSIM_EVENT_H

#include <stdint.h>
#include "CodalConstants.h"

// micro:bit v2 builds CODAL with 64-bit microsecond timestamps (target.json).
#ifndef CODAL_TIMESTAMP
#define CODAL_TIMESTAMP uint64_t
#endif

namespace codal
{
    enum EventLaunchMode
    {
        CREATE_ONLY,
        CREATE_AND_FIRE
    };

    #define DEVICE_EVENT_DEFAULT_LAUNCH_MODE CREATE_AND_FIRE

    /**
      * An event. As in CODAL, constructing one FIRES it unless CREATE_ONLY is given.
      */
    class Event
    {
        public:
        uint16_t source;            // ID of the component that generated the event.
        uint16_t value;             // Component specific code indicating the cause of the event.
        CODAL_TIMESTAMP timestamp;  // Time at which the event was generated. us since power on.

        Event(uint16_t source, uint16_t value, EventLaunchMode mode = DEVICE_EVENT_DEFAULT_LAUNCH_MODE);
        Event(uint16_t source, uint16_t value, CODAL_TIMESTAMP currentTimeUs, EventLaunchMode mode = DEVICE_EVENT_DEFAULT_LAUNCH_MODE);
        Event();

        /** Fires this event onto the default message bus. */
        void fire();
    };

    struct EventQueueItem
    {
        Event evt;
        EventQueueItem *next;
        EventQueueItem(Event evt);
    };
}

#endif
