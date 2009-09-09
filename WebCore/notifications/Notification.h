/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef Notification_h  
#define Notification_h

#include "ActiveDOMObject.h"
#include "AtomicStringHash.h"
#include "Event.h"
#include "EventListener.h"
#include "EventTarget.h"
#include "ExceptionCode.h"
#include "KURL.h"
#include "NotificationPresenter.h"
#include "NotificationContents.h"
#include "RegisteredEventListener.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if ENABLE(NOTIFICATIONS)
namespace WebCore {

    class WorkerContext;

    class Notification : public RefCounted<Notification>, public ActiveDOMObject, public EventTarget { 
    public:
        static Notification* create(const String& url, ScriptExecutionContext* context, ExceptionCode& ec, NotificationPresenter* provider) { return new Notification(url, context, ec, provider); }
        static Notification* create(const NotificationContents& contents, ScriptExecutionContext* context, ExceptionCode& ec, NotificationPresenter* provider) { return new Notification(contents, context, ec, provider); }
        
        virtual ~Notification();

        void show();
        void cancel();
    
        bool isHTML() { return m_isHTML; }
        KURL url() { return m_notificationURL; }
        NotificationContents& contents() { return m_contents; }

        EventListener* ondisplay() const;
        void setOndisplay(PassRefPtr<EventListener> eventListener);
        EventListener* onerror() const;
        void setOnerror(PassRefPtr<EventListener> eventListener);
        EventListener* onclose() const;
        void setOnclose(PassRefPtr<EventListener> eventListener);
    
        using RefCounted<Notification>::ref;
        using RefCounted<Notification>::deref;
    
        // Dispatching of events on the notification.  The presenter should call these when events occur.
        void dispatchDisplayEvent();
        void dispatchErrorEvent();
        void dispatchCloseEvent();

        // EventTarget interface
        virtual ScriptExecutionContext* scriptExecutionContext() const { return ActiveDOMObject::scriptExecutionContext(); }
        virtual void addEventListener(const AtomicString&, PassRefPtr<EventListener>, bool);
        virtual void removeEventListener(const AtomicString&, EventListener*, bool);
        virtual bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&);
        virtual Notification* toNotification() { return this; }

        // These methods are for onEvent style listeners.
        EventListener* getAttributeEventListener(const AtomicString&) const;
        void setAttributeEventListener(const AtomicString&, PassRefPtr<EventListener>);
        void clearAttributeEventListener(const AtomicString&);

    private:
        Notification(const String& url, ScriptExecutionContext* context, ExceptionCode& ec, NotificationPresenter* provider);
        Notification(const NotificationContents& fields, ScriptExecutionContext* context, ExceptionCode& ec, NotificationPresenter* provider);

        // EventTarget interface
        virtual void refEventTarget() { ref(); }
        virtual void derefEventTarget() { deref(); }
  
        void handleEvent(PassRefPtr<Event> event, bool useCapture);

        bool m_isHTML;
        KURL m_notificationURL;
        NotificationContents m_contents;

        bool m_isShowing;

        RegisteredEventListenerVector m_eventListeners;

        NotificationPresenter* m_presenter;
    };

} // namespace WebCore

#endif // ENABLE(NOTIFICATIONS)

#endif // Notifications_h
