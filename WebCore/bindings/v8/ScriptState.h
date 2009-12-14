/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
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

#ifndef ScriptState_h
#define ScriptState_h

#include <v8.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>

namespace WebCore {
    class DOMWrapperWorld;
    class Node;
    class Page;
    class Frame;

    class ScriptState : public Noncopyable {
    public:
        ScriptState() { }
        ScriptState(Frame* frame);
        ScriptState(Frame* frame, v8::Handle<v8::Context> context);
        ~ScriptState();

        bool hadException() { return !m_exception.IsEmpty(); }
        void setException(v8::Local<v8::Value> exception)
        {
            m_exception = exception;
        }
        v8::Local<v8::Value> exception() { return m_exception; }

        Frame* frame() const { return m_frame; }
        v8::Local<v8::Context> context() const
        {
            return v8::Local<v8::Context>::New(m_context);
        }

    private:
        v8::Local<v8::Value> m_exception;
        Frame* m_frame;
        v8::Persistent<v8::Context> m_context;
    };

    ScriptState* scriptStateFromNode(DOMWrapperWorld*, Node*);
    ScriptState* scriptStateFromPage(DOMWrapperWorld*, Page*);

    DOMWrapperWorld* mainThreadNormalWorld();
    inline DOMWrapperWorld* debuggerWorld() { return mainThreadNormalWorld(); }
    inline DOMWrapperWorld* pluginWorld() { return mainThreadNormalWorld(); }

    // Dummy class to avoid a bunch of ifdef's in WebCore.
    class DOMWrapperWorld : public RefCounted<DOMWrapperWorld> {
    };

}

#endif // ScriptState_h
