// mbsim: CodalComponent, ported from codal-core/inc/core/CodalComponent.h
// Copyright (c) 2017 Lancaster University. MIT licence, see LICENSE.
#ifndef MBSIM_COMPONENT_H
#define MBSIM_COMPONENT_H

#include <stdint.h>
#include "CodalConstants.h"

#ifndef DEVICE_COMPONENT_COUNT
#define DEVICE_COMPONENT_COUNT 60
#endif

namespace codal
{
    /**
      * Base class of every device driver. Components with DEVICE_COMPONENT_STATUS_SYSTEM_TICK
      * set get periodicCallback() once per scheduler tick (every 4 ms on the micro:bit v2),
      * from what is interrupt context on the real board.
      */
    class CodalComponent
    {
        public:
        static CodalComponent *components[DEVICE_COMPONENT_COUNT];

        uint16_t id;      // Event Bus ID of this component
        uint16_t status;  // Component defined state.

        CodalComponent();
        CodalComponent(uint16_t id, uint16_t status);
        virtual ~CodalComponent();

        virtual int init() { return DEVICE_NOT_SUPPORTED; }
        virtual void periodicCallback() {}
        virtual void idleCallback() {}

        private:
        void addComponent();
        void removeComponent();
    };
}

#endif
