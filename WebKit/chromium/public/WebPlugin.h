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

#ifndef WebPlugin_h
#define WebPlugin_h

#include "WebCanvas.h"

struct NPObject;

namespace WebKit {

class WebDataSource;
class WebFrame;
class WebInputEvent;
class WebPluginContainer;
class WebURL;
class WebURLResponse;
struct WebCursorInfo;
struct WebPluginParams;
struct WebRect;
struct WebURLError;
template <typename T> class WebVector;

class WebPlugin {
public:
    virtual bool initialize(WebPluginContainer*) = 0;
    virtual void destroy() = 0;

    virtual NPObject* scriptableObject() = 0;

    virtual void paint(WebCanvas*, const WebRect&) = 0;

    // Coordinates are relative to the containing window.
    virtual void updateGeometry(
        const WebRect& frameRect, const WebRect& clipRect,
        const WebVector<WebRect>& cutOutsRects, bool isVisible) = 0;

    virtual void updateFocus(bool) = 0;
    virtual void updateVisibility(bool) = 0;

    virtual bool acceptsInputEvents() = 0;
    virtual bool handleInputEvent(const WebInputEvent&, WebCursorInfo&) = 0;

    virtual void didReceiveResponse(const WebURLResponse&) = 0;
    virtual void didReceiveData(const char* data, int dataLength) = 0;
    virtual void didFinishLoading() = 0;
    virtual void didFailLoading(const WebURLError&) = 0;

    // Called in response to WebPluginContainer::loadFrameRequest
    virtual void didFinishLoadingFrameRequest(
        const WebURL&, void* notifyData) = 0;
    virtual void didFailLoadingFrameRequest(
        const WebURL&, void* notifyData, const WebURLError&) = 0;

protected:
    ~WebPlugin() { }
};

} // namespace WebKit

#endif
