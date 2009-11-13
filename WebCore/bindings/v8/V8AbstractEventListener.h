/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8AbstractEventListener_h
#define V8AbstractEventListener_h

#include "EventListener.h"
#include "OwnHandle.h"
#include "SharedPersistent.h"
#include <v8.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

    class Event;
    class Frame;
    class V8Proxy;

    // Shared by listener objects and V8Proxy so that V8Proxy can
    // silence listeners when needed.
    class V8ListenerGuard : public RefCounted<V8ListenerGuard> {
      public:
        static PassRefPtr<V8ListenerGuard> create()
        {
            return adoptRef(new V8ListenerGuard);
        }

        bool isDisconnected() const { return m_disconnected; }

        void disconnectListeners()
        {
            m_disconnected = true;
        }

      private:
        V8ListenerGuard()
            : m_disconnected(false) { }

        bool m_disconnected;
    };

    // There are two kinds of event listeners: HTML or non-HMTL. onload,
    // onfocus, etc (attributes) are always HTML event handler type; Event
    // listeners added by Window.addEventListener or
    // EventTargetNode::addEventListener are non-HTML type.
    //
    // Why does this matter?
    // WebKit does not allow duplicated HTML event handlers of the same type,
    // but ALLOWs duplicated non-HTML event handlers.
    class V8AbstractEventListener : public EventListener {
    public:
        virtual ~V8AbstractEventListener();

        static const V8AbstractEventListener* cast(const EventListener* listener)
        {
            return listener->type() == JSEventListenerType
                ? static_cast<const V8AbstractEventListener*>(listener)
                : 0;
        }

        static V8AbstractEventListener* cast(EventListener* listener)
        {
            return const_cast<V8AbstractEventListener*>(cast(const_cast<const EventListener*>(listener)));
        }

        // Implementation of EventListener interface.

        virtual bool operator==(const EventListener& other) { return this == &other; }

        virtual void handleEvent(ScriptExecutionContext*, Event*);

        // Returns the owner frame of the listener.
        Frame* frame() { return m_frame; }

        virtual bool isLazy() const { return false; }

        // Returns the listener object, either a function or an object.
        v8::Local<v8::Object> getListenerObject()
        {
            prepareListenerObject();
            return v8::Local<v8::Object>::New(m_listener);
        }

        v8::Local<v8::Object> getExistingListenerObject()
        {
            return v8::Local<v8::Object>::New(m_listener);
        }

        bool hasExistingListenerObject()
        {
            return !m_listener.IsEmpty();
        }

        // Dispose listener object and clear the handle.
        void disposeListenerObject();

        // Detach the listener from its owner frame.
        void disconnectFrame() { m_frame = 0; }

        virtual bool disconnected() const { return m_guard && m_guard->isDisconnected(); }

    protected:
        V8AbstractEventListener(Frame*, PassRefPtr<V8ListenerGuard>, bool isAttribute);

        virtual void prepareListenerObject() { }

        void setListenerObject(v8::Handle<v8::Object> listener);

        void invokeEventHandler(v8::Handle<v8::Context>, Event*, v8::Handle<v8::Value> jsEvent);

        // Get the receiver object to use for event listener call.
        v8::Local<v8::Object> getReceiverObject(Event*);

        int lineNumber() const { return m_lineNumber; }

    private:
        // Implementation of EventListener function.
        virtual bool virtualisAttribute() const { return m_isAttribute; }

        virtual v8::Local<v8::Value> callListenerFunction(v8::Handle<v8::Value> jsevent, Event*) = 0;

        v8::Persistent<v8::Object> m_listener;

        // Indicates if the above handle is weak.
        bool m_isWeak;

        // Indicates if this is an HTML type listener.
        bool m_isAttribute;

        // Frame to which the event listener is attached to. An event listener must be destroyed before its owner frame is
        // deleted. See fast/dom/replaceChild.html
        // FIXME: this could hold m_frame live until the event listener is deleted.
        Frame* m_frame;
        RefPtr<SharedPersistent<v8::Context> > m_context;
        RefPtr<V8ListenerGuard> m_guard;

        // Position in the HTML source for HTML event listeners.
        int m_lineNumber;
        int m_columnNumber;
    };

} // namespace WebCore

#endif // V8AbstractEventListener_h
