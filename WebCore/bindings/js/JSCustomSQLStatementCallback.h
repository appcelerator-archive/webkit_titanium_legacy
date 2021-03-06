/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef JSCustomSQLStatementCallback_h
#define JSCustomSQLStatementCallback_h

#if ENABLE(DATABASE)

#include "JSCallbackData.h"
#include "SQLStatementCallback.h"
#include <wtf/Forward.h>

namespace WebCore {

class SQLResultSet;

class JSCustomSQLStatementCallback : public SQLStatementCallback {
public:
    static PassRefPtr<JSCustomSQLStatementCallback> create(JSC::JSObject* callback, JSDOMGlobalObject* globalObject)
    {
        return adoptRef(new JSCustomSQLStatementCallback(callback, globalObject));
    }

    virtual ~JSCustomSQLStatementCallback();

    virtual void handleEvent(ScriptExecutionContext*, SQLTransaction*, SQLResultSet*, bool& raisedException);

private:
    JSCustomSQLStatementCallback(JSC::JSObject* callback, JSDOMGlobalObject*);

    JSCallbackData* m_data;
    RefPtr<DOMWrapperWorld> m_isolatedWorld;
};

}

#endif // ENABLE(DATABASE)

#endif // JSCustomSQLStatementCallback_h
