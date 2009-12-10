/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#include "WebPluginHalterClient.h"

#include "DOMCoreClasses.h"
#include "WebView.h"
#include <WebCore/Node.h>
#include <wtf/Assertions.h>

using namespace WebCore;

WebPluginHalterClient::WebPluginHalterClient(WebView* webView)
    : m_webView(webView)
{
    ASSERT_ARG(webView, webView);
}

bool WebPluginHalterClient::shouldHaltPlugin(Node* n, bool isWindowed, const String& pluginName) const
{
    ASSERT_ARG(n, n);

    IWebPluginHalterDelegate* d;
    if (FAILED(m_webView->pluginHalterDelegate(&d)) || !d)
        return false;

    COMPtr<IDOMNode> domNode(AdoptCOM, DOMNode::createInstance(n));

    BOOL shouldHalt;
    if (FAILED(d->shouldHaltPlugin(m_webView, domNode.get(), isWindowed, BString(pluginName), &shouldHalt)))
        return false;

    return shouldHalt;
}

bool WebPluginHalterClient::enabled() const
{
    IWebPluginHalterDelegate* d;
    if (FAILED(m_webView->pluginHalterDelegate(&d)) || !d)
        return false;

    return true;
}
