// mbsim: Listener, EventModel and MessageBus, ported from
//   codal-core/inc/core/CodalListener.h, MemberFunctionCallback.h, EventModel.h
//   codal-core/inc/drivers/MessageBus.h
// Copyright (c) 2017 Lancaster University. MIT licence, see LICENSE.
#ifndef MBSIM_MESSAGE_BUS_H
#define MBSIM_MESSAGE_BUS_H

#include <stdint.h>
#include <string.h>
#include "CodalConstants.h"
#include "Event.h"
#include "Component.h"

namespace codal
{
    class MemberFunctionCallback
    {
        private:
        void *object;
        uint32_t method[4];
        void (*invoke)(void *object, uint32_t *method, Event e);
        template <typename T> static void methodCall(void *object, uint32_t *method, Event e);

        public:
        template <typename T> MemberFunctionCallback(T *object, void (T::*method)(Event e));
        bool operator==(const MemberFunctionCallback &mfc);
        void fire(Event e);
    };

    template <typename T>
    MemberFunctionCallback::MemberFunctionCallback(T *object, void (T::*method)(Event e))
    {
        this->object = object;
        memset(this->method, 0, sizeof(this->method));
        memcpy(this->method, &method, sizeof(method));
        invoke = &MemberFunctionCallback::methodCall<T>;
    }

    template <typename T>
    void MemberFunctionCallback::methodCall(void *object, uint32_t *method, Event e)
    {
        T *o = (T *)object;
        void (T::*m)(Event);
        memcpy(&m, method, sizeof(m));
        (o->*m)(e);
    }

    struct Listener
    {
        uint16_t id;     // The ID of the component that this listener is interested in.
        uint16_t value;  // The event value that this listener is interested in.
        uint16_t flags;  // Status and configuration options codes for this listener.

        union
        {
            void (*cb)(Event);
            void (*cb_param)(Event, void *);
            MemberFunctionCallback *cb_method;
        };

        void *cb_arg;  // Optional argument to be passed to the caller.

        Event evt;
        EventQueueItem *evt_queue;

        Listener *next;

        bool mbsim_internal;  // mbsim only: registered by the runtime, hidden from the event log.

        Listener(uint16_t id, uint16_t value, void (*handler)(Event), uint16_t flags = EVENT_LISTENER_DEFAULT_FLAGS);
        Listener(uint16_t id, uint16_t value, void (*handler)(Event, void *), void *arg, uint16_t flags = EVENT_LISTENER_DEFAULT_FLAGS);

        template <typename T>
        Listener(uint16_t id, uint16_t value, T *object, void (T::*method)(Event), uint16_t flags = EVENT_LISTENER_DEFAULT_FLAGS);

        ~Listener();

        /** Queues an event for later processing by this (busy) listener. */
        void queue(Event e);

        private:
        void common(uint16_t id, uint16_t value, uint16_t flags);
    };

    template <typename T>
    Listener::Listener(uint16_t id, uint16_t value, T *object, void (T::*method)(Event), uint16_t flags)
    {
        common(id, value, flags | MESSAGE_BUS_LISTENER_METHOD);
        this->cb_method = new MemberFunctionCallback(object, method);
    }

    class EventModel
    {
        protected:
        void (*listener_deletion_callback)(Listener *);

        public:
        static EventModel *defaultEventBus;

        EventModel() : listener_deletion_callback(NULL) {}
        virtual ~EventModel() {}

        virtual int send(Event) { return DEVICE_NOT_SUPPORTED; }
        virtual int add(Listener *) { return DEVICE_NOT_SUPPORTED; }
        virtual int remove(Listener *) { return DEVICE_NOT_SUPPORTED; }
        virtual Listener *elementAt(int) { return NULL; }

        int setListenerDeletionCallback(void (*listener_deletion_callback)(Listener *))
        {
            this->listener_deletion_callback = listener_deletion_callback;
            return DEVICE_OK;
        }

        /**
          * Register a listener function.
          *
          * @param id     The source of messages to listen for, e.g. MICROBIT_ID_BUTTON_A.
          *               MICROBIT_ID_ANY matches every source.
          * @param value  The value of messages to listen for, e.g. MICROBIT_BUTTON_EVT_CLICK.
          *               MICROBIT_EVT_ANY matches every value.
          * @param handler The function to call when the event is delivered.
          * @param flags  MESSAGE_BUS_LISTENER_QUEUE_IF_BUSY (the default), _DROP_IF_BUSY, ...
          *
          * @return DEVICE_OK, DEVICE_INVALID_PARAMETER if the handler is NULL, or
          *         DEVICE_NOT_SUPPORTED if this handler is already registered.
          */
        int listen(int id, int value, void (*handler)(Event), uint16_t flags = EVENT_LISTENER_DEFAULT_FLAGS)
        {
            if (handler == NULL)
                return DEVICE_INVALID_PARAMETER;

            Listener *listener = new Listener(id, value, handler, flags);

            if (add(listener) == DEVICE_OK)
                return DEVICE_OK;

            delete listener;
            return DEVICE_NOT_SUPPORTED;
        }

        int listen(int id, int value, void (*handler)(Event, void *), void *arg, uint16_t flags = EVENT_LISTENER_DEFAULT_FLAGS)
        {
            if (handler == NULL)
                return DEVICE_INVALID_PARAMETER;

            Listener *listener = new Listener(id, value, handler, arg, flags);

            if (add(listener) == DEVICE_OK)
                return DEVICE_OK;

            delete listener;
            return DEVICE_NOT_SUPPORTED;
        }

        template <typename T>
        int listen(uint16_t id, uint16_t value, T *object, void (T::*handler)(Event), uint16_t flags = EVENT_LISTENER_DEFAULT_FLAGS);

        int listen(CodalComponent &component, int value, void (*handler)(Event), uint16_t flags = EVENT_LISTENER_DEFAULT_FLAGS)
        {
            return listen(component.id, value, handler, flags);
        }

        int listen(CodalComponent &component, int value, void (*handler)(Event, void *), void *arg, uint16_t flags = EVENT_LISTENER_DEFAULT_FLAGS)
        {
            return listen(component.id, value, handler, arg, flags);
        }

        template <typename T>
        int listen(CodalComponent &component, uint16_t value, T *object, void (T::*handler)(Event), uint16_t flags = EVENT_LISTENER_DEFAULT_FLAGS)
        {
            return listen(component.id, value, object, handler, flags);
        }

        /** Unregister a listener function. Listeners are identified by id, value and handler. */
        int ignore(int id, int value, void (*handler)(Event))
        {
            if (handler == NULL)
                return DEVICE_INVALID_PARAMETER;

            Listener listener(id, value, handler);
            remove(&listener);

            return DEVICE_OK;
        }

        int ignore(int id, int value, void (*handler)(Event, void *))
        {
            if (handler == NULL)
                return DEVICE_INVALID_PARAMETER;

            Listener listener(id, value, handler, NULL);
            remove(&listener);

            return DEVICE_OK;
        }

        template <typename T>
        int ignore(uint16_t id, uint16_t value, T *object, void (T::*handler)(Event));

        int ignore(CodalComponent &component, int value, void (*handler)(Event))
        {
            return ignore(component.id, value, handler);
        }

        int ignore(CodalComponent &component, int value, void (*handler)(Event, void *))
        {
            return ignore(component.id, value, handler);
        }

        template <typename T>
        int ignore(CodalComponent &component, uint16_t value, T *object, void (T::*handler)(Event))
        {
            return ignore(component.id, value, object, handler);
        }
    };

    template <typename T>
    int EventModel::listen(uint16_t id, uint16_t value, T *object, void (T::*handler)(Event), uint16_t flags)
    {
        if (object == NULL || handler == NULL)
            return DEVICE_INVALID_PARAMETER;

        Listener *listener = new Listener(id, value, object, handler, flags);

        if (add(listener) == DEVICE_OK)
            return DEVICE_OK;

        delete listener;
        return DEVICE_NOT_SUPPORTED;
    }

    template <typename T>
    int EventModel::ignore(uint16_t id, uint16_t value, T *object, void (T::*handler)(Event))
    {
        if (handler == NULL)
            return DEVICE_INVALID_PARAMETER;

        Listener listener(id, value, object, handler);
        remove(&listener);

        return DEVICE_OK;
    }

    class MessageBus : public EventModel, public CodalComponent
    {
        public:
        MessageBus();
        ~MessageBus();

        virtual int send(Event evt);
        int process(Event &evt, bool urgent = false);
        virtual Listener *elementAt(int n);
        virtual int add(Listener *newListener);
        virtual int remove(Listener *newListener);

        private:
        Listener *listeners;
        EventQueueItem *evt_queue_head;
        EventQueueItem *evt_queue_tail;
        uint16_t queueLength;

        int deleteMarkedListeners();
        void queueEvent(Event &evt);
        EventQueueItem *dequeueEvent();
        void idle(Event);
    };

    uint16_t allocateNotifyEvent();
}

#endif
