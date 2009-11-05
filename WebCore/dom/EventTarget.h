/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#ifndef EventTarget_h
#define EventTarget_h

#include "AtomicStringHash.h"
#include "EventNames.h"
#include "RegisteredEventListener.h"
#include <wtf/Forward.h>

namespace WebCore {

    class AbstractWorker;
    class AtomicString;
    class DedicatedWorkerContext;
    class DOMApplicationCache;
    class DOMWindow;
    class Event;
    class EventListener;
    class EventSource;
    class MessagePort;
    class Node;
    class Notification;
    class SVGElementInstance;
    class ScriptExecutionContext;
    class SharedWorker;
    class SharedWorkerContext;
    class WebSocket;
    class Worker;
    class XMLHttpRequest;
    class XMLHttpRequestUpload;

    typedef int ExceptionCode;

    struct FiringEventIterator {
        FiringEventIterator(const AtomicString& eventType, size_t& iterator, size_t& end)
            : eventType(eventType)
            , iterator(iterator)
            , end(end)
        {
        }

        const AtomicString& eventType;
        size_t& iterator;
        size_t& end;
    };
    typedef Vector<FiringEventIterator, 1> FiringEventIteratorVector;

    typedef Vector<RegisteredEventListener, 1> EventListenerVector;
    typedef HashMap<AtomicString, EventListenerVector*> EventListenerMap;

    struct EventTargetData : Noncopyable {
        ~EventTargetData();

        EventListenerMap eventListenerMap;
        FiringEventIteratorVector firingEventIterators;
    };

    class EventTarget {
    public:
        void ref() { refEventTarget(); }
        void deref() { derefEventTarget(); }

        virtual EventSource* toEventSource();
        virtual MessagePort* toMessagePort();
        virtual Node* toNode();
        virtual DOMWindow* toDOMWindow();
        virtual XMLHttpRequest* toXMLHttpRequest();
        virtual XMLHttpRequestUpload* toXMLHttpRequestUpload();
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        virtual DOMApplicationCache* toDOMApplicationCache();
#endif
#if ENABLE(SVG)
        virtual SVGElementInstance* toSVGElementInstance();
#endif
#if ENABLE(WORKERS)
        virtual Worker* toWorker();
        virtual DedicatedWorkerContext* toDedicatedWorkerContext();
#endif
#if ENABLE(SHARED_WORKERS)
        virtual SharedWorker* toSharedWorker();
        virtual SharedWorkerContext* toSharedWorkerContext();
#endif
#if ENABLE(WEB_SOCKETS)
        virtual WebSocket* toWebSocket();
#endif

#if ENABLE(NOTIFICATIONS)
        virtual Notification* toNotification();
#endif

        virtual ScriptExecutionContext* scriptExecutionContext() const = 0;

        virtual bool addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
        virtual bool removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
        virtual void removeAllEventListeners();
        virtual bool dispatchEvent(PassRefPtr<Event>);
        bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&); // DOM API

        // Used for legacy "onEvent" attribute APIs.
        bool setAttributeEventListener(const AtomicString& eventType, PassRefPtr<EventListener>);
        bool clearAttributeEventListener(const AtomicString& eventType);
        EventListener* getAttributeEventListener(const AtomicString& eventType);

        bool hasEventListeners();
        bool hasEventListeners(const AtomicString& eventType);
        const EventListenerVector& getEventListeners(const AtomicString& eventType);

        bool fireEventListeners(Event*);
        bool isFiringEventListeners();

#if USE(JSC)
        void markEventListeners(JSC::MarkStack&);
        void invalidateEventListeners();
#endif

    protected:
        virtual ~EventTarget();
        
        virtual EventTargetData* eventTargetData() = 0;
        virtual EventTargetData* ensureEventTargetData() = 0;

    private:
        virtual void refEventTarget() = 0;
        virtual void derefEventTarget() = 0;
    };

    #define DEFINE_ATTRIBUTE_EVENT_LISTENER(attribute) \
        EventListener* on##attribute() { return getAttributeEventListener(eventNames().attribute##Event); } \
        void setOn##attribute(PassRefPtr<EventListener> listener) { setAttributeEventListener(eventNames().attribute##Event, listener); } \

    #define DEFINE_VIRTUAL_ATTRIBUTE_EVENT_LISTENER(attribute) \
        virtual EventListener* on##attribute() { return getAttributeEventListener(eventNames().attribute##Event); } \
        virtual void setOn##attribute(PassRefPtr<EventListener> listener) { setAttributeEventListener(eventNames().attribute##Event, listener); } \

    #define DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(attribute) \
        EventListener* on##attribute() { return document()->getWindowAttributeEventListener(eventNames().attribute##Event); } \
        void setOn##attribute(PassRefPtr<EventListener> listener) { document()->setWindowAttributeEventListener(eventNames().attribute##Event, listener); } \

    #define DEFINE_MAPPED_ATTRIBUTE_EVENT_LISTENER(attribute, eventName) \
        EventListener* on##attribute() { return getAttributeEventListener(eventNames().eventName##Event); } \
        void setOn##attribute(PassRefPtr<EventListener> listener) { setAttributeEventListener(eventNames().eventName##Event, listener); } \

    #define DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(recipient, attribute) \
        EventListener* on##attribute() { return recipient ? recipient->getAttributeEventListener(eventNames().attribute##Event) : 0; } \
        void setOn##attribute(PassRefPtr<EventListener> listener) { if (recipient) recipient->setAttributeEventListener(eventNames().attribute##Event, listener); } \

#ifndef NDEBUG
    void forbidEventDispatch();
    void allowEventDispatch();
    bool eventDispatchForbidden();
#else
    inline void forbidEventDispatch() { }
    inline void allowEventDispatch() { }
#endif

#if USE(JSC)
    inline void EventTarget::markEventListeners(JSC::MarkStack& markStack)
    {
        EventTargetData* d = eventTargetData();
        if (!d)
            return;

        EventListenerMap::iterator end = d->eventListenerMap.end();
        for (EventListenerMap::iterator it = d->eventListenerMap.begin(); it != end; ++it) {
            EventListenerVector& entry = *it->second;
            for (size_t i = 0; i < entry.size(); ++i)
                entry[i].listener->markJSFunction(markStack);
        }
    }

    inline void EventTarget::invalidateEventListeners()
    {
        EventTargetData* d = eventTargetData();
        if (!d)
            return;

        deleteAllValues(d->eventListenerMap);
        d->eventListenerMap.clear();
    }
#endif

    inline bool EventTarget::isFiringEventListeners()
    {
        EventTargetData* d = eventTargetData();
        if (!d)
            return false;
        return d->firingEventIterators.size() != 0;
    }

    inline bool EventTarget::hasEventListeners()
    {
        EventTargetData* d = eventTargetData();
        if (!d)
            return false;
        return !d->eventListenerMap.isEmpty();
    }

    inline bool EventTarget::hasEventListeners(const AtomicString& eventType)
    {
        EventTargetData* d = eventTargetData();
        if (!d)
            return false;
        return d->eventListenerMap.contains(eventType);
    }

} // namespace WebCore

#endif // EventTarget_h
