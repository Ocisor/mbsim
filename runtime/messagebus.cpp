// mbsim: the message bus.
// Ported from codal-core/source/drivers/MessageBus.cpp, core/CodalListener.cpp,
// core/MemberFunctionCallback.cpp and types/Event.cpp.
// Copyright (c) 2017 Lancaster University. MIT licence, see LICENSE.
//
// The logic is CODAL's, line for line where possible. The only additions are the calls
// into mbsim's event log (evlog_*), which record when each event was queued and when a
// handler finally ran.

#include "internal.h"

using namespace codal;

EventModel *EventModel::defaultEventBus = NULL;

static uint16_t userNotifyId = DEVICE_NOTIFY_USER_EVENT_BASE;

// ------------------------------------------------------------------------------- Event

Event::Event(uint16_t source, uint16_t value, EventLaunchMode mode)
{
    this->source = source;
    this->value = value;
    this->timestamp = mbsim::sim_now_us();

    if (mode != CREATE_ONLY)
        this->fire();
}

Event::Event(uint16_t source, uint16_t value, CODAL_TIMESTAMP currentTimeUs, EventLaunchMode mode)
{
    this->source = source;
    this->value = value;
    this->timestamp = currentTimeUs;

    if (mode != CREATE_ONLY)
        this->fire();
}

Event::Event()
{
    this->source = 0;
    this->value = 0;
    this->timestamp = mbsim::sim_now_us();
}

void Event::fire()
{
    if (EventModel::defaultEventBus)
        EventModel::defaultEventBus->send(*this);
}

EventQueueItem::EventQueueItem(Event evt)
{
    this->evt = evt;
    this->next = NULL;
}

// ---------------------------------------------------------------------------- Listener

void Listener::common(uint16_t id, uint16_t value, uint16_t flags)
{
    this->id = id;
    this->value = value;
    this->cb = NULL;
    this->cb_arg = NULL;
    this->flags = flags;
    this->next = NULL;
    this->evt_queue = NULL;
    this->mbsim_internal = mbsim::internalDepth > 0;
}

Listener::Listener(uint16_t id, uint16_t value, void (*handler)(Event), uint16_t flags)
{
    common(id, value, flags);
    this->cb = handler;
}

Listener::Listener(uint16_t id, uint16_t value, void (*handler)(Event, void *), void *arg, uint16_t flags)
{
    common(id, value, flags | MESSAGE_BUS_LISTENER_PARAMETERISED);
    this->cb_param = handler;
    this->cb_arg = arg;
}

Listener::~Listener()
{
    if (this->flags & MESSAGE_BUS_LISTENER_METHOD)
        delete cb_method;

    while (evt_queue)
    {
        EventQueueItem *item = evt_queue;
        evt_queue = item->next;
        delete item;
    }
}

void Listener::queue(Event e)
{
    int queueDepth;

    EventQueueItem *p = evt_queue;

    if (evt_queue == NULL)
        evt_queue = new EventQueueItem(e);
    else
    {
        queueDepth = 1;

        while (p->next != NULL)
        {
            p = p->next;
            queueDepth++;
        }

        if (queueDepth < MESSAGE_BUS_LISTENER_MAX_QUEUE_DEPTH)
            p->next = new EventQueueItem(e);
        else if (!mbsim_internal)
            mbsim::evlog_undelivered(e, "dropped: this handler's queue is full");
    }
}

void MemberFunctionCallback::fire(Event e)
{
    invoke(object, method, e);
}

bool MemberFunctionCallback::operator==(const MemberFunctionCallback &mfc)
{
    return (object == mfc.object && (memcmp(method, mfc.method, sizeof(method)) == 0));
}

// -------------------------------------------------------------------------- MessageBus

MessageBus::MessageBus()
{
    this->listeners = NULL;
    this->evt_queue_head = NULL;
    this->evt_queue_tail = NULL;
    this->queueLength = 0;

    // ANY listeners for scheduler events MUST be immediate, or else they will not be registered.
    {
        mbsim::InternalScope internal;
        listen(DEVICE_ID_SCHEDULER, DEVICE_SCHEDULER_EVT_IDLE, this, &MessageBus::idle, MESSAGE_BUS_LISTENER_IMMEDIATE);
    }

    if (EventModel::defaultEventBus == NULL)
        EventModel::defaultEventBus = this;
}

// Invokes one listener for the event stored in it. Runs in the handler's own fiber
// (or directly, for non-blocking listeners).
static void async_callback(void *param)
{
    Listener *listener = (Listener *)param;

    // OK, now we need to decide how to behave depending on our configuration.
    // If this a fiber f already active within this listener then check our
    // configuration to determine the correct course of action.
    if (listener->flags & MESSAGE_BUS_LISTENER_BUSY)
    {
        // Drop this event, if that's how we've been configured.
        if (listener->flags & MESSAGE_BUS_LISTENER_DROP_IF_BUSY)
        {
            if (!listener->mbsim_internal)
                mbsim::evlog_undelivered(listener->evt, "dropped: handler busy (DROP_IF_BUSY)");
            return;
        }

        // Queue this event up for later, if that's how we've been configured.
        if (listener->flags & MESSAGE_BUS_LISTENER_QUEUE_IF_BUSY)
        {
            listener->queue(listener->evt);
            return;
        }
    }

    // Record that we have a fiber going into this listener...
    listener->flags |= MESSAGE_BUS_LISTENER_BUSY;

    while (1)
    {
        if (!listener->mbsim_internal)
            mbsim::evlog_delivered(listener->evt, listener);

        // Firstly, check for a method callback into an object.
        if (listener->flags & MESSAGE_BUS_LISTENER_METHOD)
            listener->cb_method->fire(listener->evt);

        // Now a parameterised C function
        else if (listener->flags & MESSAGE_BUS_LISTENER_PARAMETERISED)
            listener->cb_param(listener->evt, listener->cb_arg);

        // We must have a plain C function
        else
            listener->cb(listener->evt);

        // If there are more events to process, dequeue the next one and process it.
        if ((listener->flags & MESSAGE_BUS_LISTENER_QUEUE_IF_BUSY) && listener->evt_queue)
        {
            EventQueueItem *item = listener->evt_queue;

            listener->evt = item->evt;
            listener->evt_queue = listener->evt_queue->next;
            delete item;

            // We spin the scheduler here, to preven any particular event handler from
            // continuously holding onto resources.
            schedule();
        }
        else
            break;
    }

    // The fiber of exiting... clear our state.
    listener->flags &= ~MESSAGE_BUS_LISTENER_BUSY;
}

// mbsim: does any listener registered by the user's program match this event?
static bool has_user_listener(Listener *l, const Event &evt)
{
    for (; l != NULL; l = l->next)
    {
        if (l->mbsim_internal || (l->flags & MESSAGE_BUS_LISTENER_DELETING))
            continue;
        if ((l->id == evt.source || l->id == DEVICE_ID_ANY) && (l->value == evt.value || l->value == DEVICE_EVT_ANY))
            return true;
    }
    return false;
}

void MessageBus::queueEvent(Event &evt)
{
    int processingComplete;

    EventQueueItem *prev = evt_queue_tail;

    // Now process all handler regsitered as URGENT.
    // These pre-empt the queue, and are useful for fast, high priority services.
    processingComplete = this->process(evt, true);

    // If we've already processed all event handlers, we're all done.
    // No need to queue the event.
    if (processingComplete)
    {
        if (!mbsim::is_internal_event(evt.source) && !has_user_listener(listeners, evt))
            mbsim::evlog_undelivered(evt, NULL);
        return;
    }

    // If we need to queue, but there is no space, then there's nothg we can do.
    if (queueLength >= MESSAGE_BUS_LISTENER_MAX_QUEUE_DEPTH)
    {
        // Note that this can lead to strange lockups, where we await an event that never arrives.
        if (!mbsim::is_internal_event(evt.source))
            mbsim::evlog_undelivered(evt, "dropped: the message bus queue is full");
        return;
    }

    // Otherwise, we need to queue this event for later processing...
    // We queue this event at the tail of the queue at the point where we entered queueEvent()
    // This is important as the processing above *may* have generated further events, and
    // we want to maintain ordering of events.
    EventQueueItem *item = new EventQueueItem(evt);

    // The queue was empty when we entered this function, so queue our event at the start of the queue.
    if (prev == NULL)
    {
        item->next = evt_queue_head;
        evt_queue_head = item;
    }
    else
    {
        item->next = prev->next;
        prev->next = item;
    }

    if (item->next == NULL)
        evt_queue_tail = item;

    queueLength++;
}

EventQueueItem *MessageBus::dequeueEvent()
{
    EventQueueItem *item = NULL;

    if (evt_queue_head != NULL)
    {
        item = evt_queue_head;
        evt_queue_head = item->next;

        if (evt_queue_head == NULL)
            evt_queue_tail = NULL;

        queueLength--;
    }

    return item;
}

int MessageBus::deleteMarkedListeners()
{
    Listener *l, *p;
    int removed = 0;

    l = listeners;
    p = NULL;

    // Walk this list of event handlers. Delete any that match the given listener.
    while (l != NULL)
    {
        if ((l->flags & MESSAGE_BUS_LISTENER_DELETING) && !(l->flags & MESSAGE_BUS_LISTENER_BUSY))
        {
            if (p == NULL)
                listeners = l->next;
            else
                p->next = l->next;

            // delete the listener.
            Listener *t = l;
            l = l->next;

            // If notification of deletion has been requested, invoke the listener deletion callback.
            if (listener_deletion_callback)
                listener_deletion_callback(t);

            delete t;
            removed++;

            continue;
        }

        p = l;
        l = l->next;
    }

    return removed;
}

// Runs when the scheduler has nothing else to do.
void MessageBus::idle(Event)
{
    // Clear out any listeners marked for deletion
    this->deleteMarkedListeners();

    EventQueueItem *item = this->dequeueEvent();

    // Whilst there are events to process and we have no useful other work to do, pull
    // them off the queue and process them.
    while (item)
    {
        if (!mbsim::is_internal_event(item->evt.source) && !has_user_listener(listeners, item->evt))
            mbsim::evlog_undelivered(item->evt, NULL);

        // send the event to all standard event listeners.
        this->process(item->evt);

        // Free the queue item.
        delete item;

        // If we have created some useful work to do, we stop processing.
        // This helps to minimise the number of blocked fibers we create at any point in
        // time, therefore also reducing the RAM footprint.
        if (!scheduler_runqueue_empty())
            break;

        // Pull the next event to process, if there is one.
        item = this->dequeueEvent();
    }
}

int MessageBus::send(Event evt)
{
    // We simply queue processing of the event until we're scheduled in normal thread context.
    // We do this to avoid the possibility of executing event handler code in IRQ context,
    // which may bring hidden race conditions to kids code. Queuing all events ensures
    // causal ordering (total ordering in fact).
    this->queueEvent(evt);
    return DEVICE_OK;
}

int MessageBus::process(Event &evt, bool urgent)
{
    Listener *l;
    int complete = 1;
    bool listenerUrgent;

    l = listeners;
    while (l != NULL)
    {
        if ((l->id == evt.source || l->id == DEVICE_ID_ANY) && (l->value == evt.value || l->value == DEVICE_EVT_ANY))
        {
            // If we're running under the fiber scheduler, then derive the THREADING_MODE
            // for the callback based on the metadata in the listener itself.
            if (fiber_scheduler_running())
                listenerUrgent = (l->flags & MESSAGE_BUS_LISTENER_IMMEDIATE) == MESSAGE_BUS_LISTENER_IMMEDIATE;
            else
                listenerUrgent = true;

            // If we should process this event hander in this pass, then activate the listener.
            if (listenerUrgent == urgent && !(l->flags & MESSAGE_BUS_LISTENER_DELETING))
            {
                l->evt = evt;

                // OK, if this handler has regisitered itself as non-blocking, we just
                // execute it directly... This is normally only done for trusted system
                // components. Otherwise, we invoke it in a 'fork on block' context, that
                // will automatically create a fiber should the event handler attempt a
                // blocking operation.
                if (l->flags & MESSAGE_BUS_LISTENER_NONBLOCKING || !fiber_scheduler_running())
                    async_callback(l);
                else
                    invoke(async_callback, l);
            }
            else
                complete = 0;
        }

        l = l->next;
    }

    return complete;
}

int MessageBus::add(Listener *newListener)
{
    Listener *l, *p;
    int methodCallback;

    // handler can't be NULL!
    if (newListener == NULL)
        return DEVICE_INVALID_PARAMETER;

    l = listeners;

    // Firstly, we treat a listener as an idempotent operation. Ensure we don't already
    // have this handler registered in a that will already capture these events. If we
    // do, silently ignore.

    // We always check the ID, VALUE and CB_METHOD fields.
    // If we have a callback to a method, check the cb_method class. Otherwise, the cb
    // function point is sufficient.
    while (l != NULL)
    {
        methodCallback = (newListener->flags & MESSAGE_BUS_LISTENER_METHOD) && (l->flags & MESSAGE_BUS_LISTENER_METHOD);

        if (l->id == newListener->id && l->value == newListener->value && (methodCallback ? *l->cb_method == *newListener->cb_method : l->cb == newListener->cb) && newListener->cb_arg == l->cb_arg)
        {
            // We have a perfect match for this event listener already registered.
            // If it's marked for deletion, we simply resurrect the listener, and we're done.
            // Either way, we return an error code, as the *new* listener should be released...
            if (l->flags & MESSAGE_BUS_LISTENER_DELETING)
                l->flags &= ~MESSAGE_BUS_LISTENER_DELETING;

            return DEVICE_NOT_SUPPORTED;
        }

        l = l->next;
    }

    // We have a valid, new event handler. Add it to the list.
    // if listeners is null - we can automatically add this listener to the list at the beginning...
    if (listeners == NULL)
    {
        listeners = newListener;
        Event(DEVICE_ID_MESSAGE_BUS_LISTENER, newListener->id);

        return DEVICE_OK;
    }

    // We maintain an ordered list of listeners.
    // The chain is held stictly in increasing order of ID (first level), then value code
    // (second level). Find the correct point in the chain for this event.
    // Adding a listener is a rare occurance, so we just walk the list...

    p = listeners;
    l = listeners;

    while (l != NULL && l->id < newListener->id)
    {
        p = l;
        l = l->next;
    }

    while (l != NULL && l->id == newListener->id && l->value < newListener->value)
    {
        p = l;
        l = l->next;
    }

    // add at front of list
    if (p == listeners && (newListener->id < p->id || (p->id == newListener->id && p->value > newListener->value)))
    {
        newListener->next = p;

        // this new listener is now the front!
        listeners = newListener;
    }

    // add after p
    else
    {
        newListener->next = p->next;
        p->next = newListener;
    }

    Event(DEVICE_ID_MESSAGE_BUS_LISTENER, newListener->id);
    return DEVICE_OK;
}

int MessageBus::remove(Listener *listener)
{
    Listener *l;
    int removed = 0;

    // handler can't be NULL!
    if (listener == NULL)
        return DEVICE_INVALID_PARAMETER;

    l = listeners;

    // Walk this list of event handlers. Delete any that match the given listener.
    while (l != NULL)
    {
        if ((listener->flags & MESSAGE_BUS_LISTENER_METHOD) == (l->flags & MESSAGE_BUS_LISTENER_METHOD))
        {
            if (((listener->flags & MESSAGE_BUS_LISTENER_METHOD) && (*l->cb_method == *listener->cb_method)) ||
                ((!(listener->flags & MESSAGE_BUS_LISTENER_METHOD) && l->cb == listener->cb)))
            {
                if ((listener->id == DEVICE_ID_ANY || listener->id == l->id) && (listener->value == DEVICE_EVT_ANY || listener->value == l->value))
                {
                    // Found a match. mark this to be removed from the list.
                    l->flags |= MESSAGE_BUS_LISTENER_DELETING;
                    removed++;

                    // Raise an event to indicate the removal
                    Event(DEVICE_ID_MESSAGE_BUS_IGNORED, l->id);
                }
            }
        }

        l = l->next;
    }

    if (removed > 0)
        return DEVICE_OK;
    else
        return DEVICE_INVALID_PARAMETER;
}

Listener *MessageBus::elementAt(int n)
{
    Listener *l = listeners;

    while (n > 0)
    {
        if (l == NULL)
            return NULL;

        n--;
        l = l->next;
    }

    return l;
}

namespace codal
{
    uint16_t allocateNotifyEvent()
    {
        return userNotifyId++;
    }
}

MessageBus::~MessageBus()
{
    ignore(DEVICE_ID_SCHEDULER, DEVICE_EVT_ANY, this, &MessageBus::idle);
}
