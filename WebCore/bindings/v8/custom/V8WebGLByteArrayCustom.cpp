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

#include "config.h"

#if ENABLE(3D_CANVAS)

#include "WebGLArrayBuffer.h"
#include "WebGLByteArray.h"

#include "V8Binding.h"
#include "V8WebGLArrayBuffer.h"
#include "V8WebGLArrayCustom.h"
#include "V8WebGLByteArray.h"
#include "V8CustomBinding.h"
#include "V8Proxy.h"

namespace WebCore {

CALLBACK_FUNC_DECL(WebGLByteArrayConstructor)
{
    INC_STATS("DOM.WebGLByteArray.Contructor");

    return constructWebGLArray<WebGLByteArray>(args, V8ClassIndex::ToInt(V8ClassIndex::WEBGLBYTEARRAY));
}

// Get the specified value from the byte buffer and return it wrapped as a JavaScript Number object to V8. Accesses outside the valid byte buffer range return "undefined".
INDEXED_PROPERTY_GETTER(WebGLByteArray)
{
    INC_STATS("DOM.WebGLByteArray.IndexedPropertyGetter");
    WebGLByteArray* byteBuffer = V8DOMWrapper::convertToNativeObject<WebGLByteArray>(V8ClassIndex::WEBGLBYTEARRAY, info.Holder());

    if ((index < 0) || (index >= byteBuffer->length()))
        return v8::Undefined();
    signed char result;
    if (!byteBuffer->get(index, result))
        return v8::Undefined();
    return v8::Number::New(result);
}

// Set the specified value in the byte buffer. Accesses outside the valid byte buffer range are silently ignored.
INDEXED_PROPERTY_SETTER(WebGLByteArray)
{
    INC_STATS("DOM.WebGLByteArray.IndexedPropertySetter");
    WebGLByteArray* array = V8DOMWrapper::convertToNativeObject<WebGLByteArray>(V8ClassIndex::WEBGLBYTEARRAY, info.Holder());

    if ((index >= 0) && (index < array->length())) {
        if (!value->IsNumber())
            return throwError("Could not convert value argument to a number");
        array->set(index, value->NumberValue());
    }
    return value;
}

CALLBACK_FUNC_DECL(WebGLByteArrayGet)
{
    INC_STATS("DOM.WebGLByteArray.get()");
    return getWebGLArrayElement<WebGLByteArray, signed char>(args, V8ClassIndex::WEBGLBYTEARRAY);
}

CALLBACK_FUNC_DECL(WebGLByteArraySet)
{
    INC_STATS("DOM.WebGLByteArray.set()");
    return setWebGLArray<WebGLByteArray, V8WebGLByteArray>(args, V8ClassIndex::WEBGLBYTEARRAY);
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
