/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"
#include "InjectedScript.h"

#if ENABLE(INSPECTOR)

#include "PlatformString.h"
#include "SerializedScriptValue.h"
#include "ScriptFunctionCall.h"

namespace WebCore {

InjectedScript::InjectedScript(ScriptObject injectedScriptObject)
    : m_injectedScriptObject(injectedScriptObject)
{
}

void InjectedScript::dispatch(long callId, const String& methodName, const String& arguments, bool async, RefPtr<SerializedScriptValue>* result, bool* hadException) 
{
    ASSERT(!hasNoValue());
    ScriptFunctionCall function(m_injectedScriptObject, "dispatch");
    function.appendArgument(methodName);
    function.appendArgument(arguments);
    if (async)
        function.appendArgument(callId);
    *hadException = false;
    ScriptValue resultValue = function.call(*hadException);
    if (!*hadException)
        *result = resultValue.serialize(m_injectedScriptObject.scriptState());
}

#if ENABLE(JAVASCRIPT_DEBUGGER)
PassRefPtr<SerializedScriptValue> InjectedScript::callFrames()
{
    ASSERT(!hasNoValue());
    ScriptFunctionCall function(m_injectedScriptObject, "callFrames");
    ScriptValue callFramesValue = function.call();
    return callFramesValue.serialize(m_injectedScriptObject.scriptState());
}
#endif

PassRefPtr<SerializedScriptValue> InjectedScript::wrapForConsole(ScriptValue value)
{
    ASSERT(!hasNoValue());
    ScriptFunctionCall wrapFunction(m_injectedScriptObject, "wrapObject");
    wrapFunction.appendArgument(value);
    wrapFunction.appendArgument("console");
    ScriptValue r = wrapFunction.call();
    return r.serialize(m_injectedScriptObject.scriptState());
}

void InjectedScript::releaseWrapperObjectGroup(const String& objectGroup)
{
    ASSERT(!hasNoValue());
    ScriptFunctionCall releaseFunction(m_injectedScriptObject, "releaseWrapperObjectGroup");
    releaseFunction.appendArgument(objectGroup);
    releaseFunction.call();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
