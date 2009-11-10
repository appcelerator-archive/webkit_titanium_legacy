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

#ifndef WebInputElement_h
#define WebInputElement_h

#include "WebElement.h"

#if WEBKIT_IMPLEMENTATION
namespace WebCore { class HTMLInputElement; }
namespace WTF { template <typename T> class PassRefPtr; }
#endif

namespace WebKit {

    // Provides readonly access to some properties of a DOM input element node.
    class WebInputElement : public WebElement {
    public:
        WebInputElement() : WebElement() { }
        WebInputElement(const WebInputElement& n) : WebElement(n) { }

        WebInputElement& operator=(const WebInputElement& n) { WebElement::assign(n); return *this; }
        WEBKIT_API void assign(const WebInputElement& n) { WebElement::assign(n); }

#if WEBKIT_IMPLEMENTATION
        WebInputElement(const WTF::PassRefPtr<WebCore::HTMLInputElement>&);
        WebInputElement& operator=(const WTF::PassRefPtr<WebCore::HTMLInputElement>&);
        operator WTF::PassRefPtr<WebCore::HTMLInputElement>() const;
#endif

        void setActivatedSubmit(bool);
        void setValue(const WebString& value);
        WebString value();
        void setAutofilled(bool);
        void dispatchFormControlChangeEvent();
        void setSelectionRange(size_t, size_t);
    };

} // namespace WebKit

#endif
