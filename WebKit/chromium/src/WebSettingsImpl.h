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

#ifndef WebSettingsImpl_h
#define WebSettingsImpl_h

// TODO(jorlow): Remove this hack once WebView is free of glue.
#include "../public/WebSettings.h"

namespace WebCore {
class Settings;
}

namespace WebKit {

class WebSettingsImpl : public WebSettings {
public:
    explicit WebSettingsImpl(WebCore::Settings*);
    virtual ~WebSettingsImpl() { }

    virtual void setStandardFontFamily(const WebString&);
    virtual void setFixedFontFamily(const WebString&);
    virtual void setSerifFontFamily(const WebString&);
    virtual void setSansSerifFontFamily(const WebString&);
    virtual void setCursiveFontFamily(const WebString&);
    virtual void setFantasyFontFamily(const WebString&);
    virtual void setDefaultFontSize(int);
    virtual void setDefaultFixedFontSize(int);
    virtual void setMinimumFontSize(int);
    virtual void setMinimumLogicalFontSize(int);
    virtual void setDefaultTextEncodingName(const WebString&);
    virtual void setJavaScriptEnabled(bool);
    virtual void setWebSecurityEnabled(bool);
    virtual void setJavaScriptCanOpenWindowsAutomatically(bool);
    virtual void setLoadsImagesAutomatically(bool);
    virtual void setPluginsEnabled(bool);
    virtual void setDOMPasteAllowed(bool);
    virtual void setDeveloperExtrasEnabled(bool);
    virtual void setShrinksStandaloneImagesToFit(bool);
    virtual void setUsesEncodingDetector(bool);
    virtual void setTextAreasAreResizable(bool);
    virtual void setJavaEnabled(bool);
    virtual void setAllowScriptsToCloseWindows(bool);
    virtual void setUserStyleSheetLocation(const WebURL&);
    virtual void setUsesPageCache(bool);
    virtual void setDownloadableBinaryFontsEnabled(bool);
    virtual void setXSSAuditorEnabled(bool);
    virtual void setLocalStorageEnabled(bool);
    virtual void setEditableLinkBehaviorNeverLive();
    virtual void setFontRenderingModeNormal();
    virtual void setShouldPaintCustomScrollbars(bool);
    virtual void setDatabasesEnabled(bool);
    virtual void setAllowUniversalAccessFromFileURLs(bool);
    virtual void setTextDirectionSubmenuInclusionBehaviorNeverIncluded();
    virtual void setOfflineWebApplicationCacheEnabled(bool);
    virtual void setExperimentalWebGLEnabled(bool);

private:
    WebCore::Settings* m_settings;
};

} // namespace WebKit

#endif
