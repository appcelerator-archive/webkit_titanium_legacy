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

#ifndef V8IsolatedWorld_h
#define V8IsolatedWorld_h

#include <v8.h>

#include "V8DOMMap.h"
#include "V8Index.h"
#include "V8Proxy.h"
#include "V8Utilities.h"
#include "ScriptSourceCode.h"  // for WebCore::ScriptSourceCode

namespace WebCore {

    class V8Proxy;

    // V8IsolatedWorld
    //
    // V8IsolatedWorld represents a isolated execution environment for
    // JavaScript.  Each isolated world executes in parallel with the main
    // JavaScript world.  An isolated world has access to the same DOM data
    // structures as the main world but none of the JavaScript pointers.
    //
    // It is an error to ever share a JavaScript pointer between two isolated
    // worlds or between an isolated world and the main world.  Because
    // isolated worlds have access to the DOM, they need their own DOM wrappers
    // to avoid having pointers to the main world's DOM wrappers (which are
    // JavaScript objects).
    //
    class V8IsolatedWorld {
    public:
        // Creates an isolated world. To destroy it, call destroy().
        // This will delete the isolated world when the context it owns is GC'd.
        V8IsolatedWorld(V8Proxy* proxy, int extensionGroup);
        ~V8IsolatedWorld();

        // Call this to destroy the isolated world. It will be deleted sometime
        // after this call, once all script references to the world's context
        // have been dropped.
        void destroy();

        // Returns the isolated world associated with
        // v8::Context::GetEntered().  Because worlds are isolated, the entire
        // JavaScript call stack should be from the same isolated world.
        // Returns NULL if the entered context is from the main world.
        //
        // FIXME: Consider edge cases with DOM mutation events that might
        // violate this invariant.
        //
        static V8IsolatedWorld* getEntered()
        {
            // This is a temporary performance optimization.   Essentially,
            // GetHiddenValue is too slow for this code path.  We need to get the
            // V8 team to add a real property to v8::Context for isolated worlds.
            // Until then, we optimize the common case of not having any isolated
            // worlds at all.
            if (!isolatedWorldCount)
                return 0;
            if (!v8::Context::InContext())
                return 0;
            return reinterpret_cast<V8IsolatedWorld*>(getGlobalObject(v8::Context::GetEntered())->GetPointerFromInternalField(V8Custom::kDOMWindowEnteredIsolatedWorldIndex));
        }

        v8::Handle<v8::Context> context() { return m_context->get(); }
        PassRefPtr<SharedPersistent<v8::Context> > sharedContext() { return m_context; }

        DOMDataStore* getDOMDataStore() const { return m_domDataStore.getStore(); }

    private:
        static v8::Handle<v8::Object> getGlobalObject(v8::Handle<v8::Context> context)
        {
            return v8::Handle<v8::Object>::Cast(context->Global()->GetPrototype());
        }

        // Called by the garbage collector when our JavaScript context is about
        // to be destroyed.
        static void contextWeakReferenceCallback(v8::Persistent<v8::Value> object, void* isolated_world);

        // The v8::Context for the isolated world.  This object is keep on the
        // heap as long as |m_context| has not been garbage collected.
        RefPtr<SharedPersistent<v8::Context> > m_context;

        // The backing store for the isolated world's DOM wrappers.  This class
        // doesn't have visibility into the wrappers.  This handle simply helps
        // manage their lifetime.
        DOMDataStoreHandle m_domDataStore;

        static int isolatedWorldCount;
    };

} // namespace WebCore

#endif // V8IsolatedWorld_h
