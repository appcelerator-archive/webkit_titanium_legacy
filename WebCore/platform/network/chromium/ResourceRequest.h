/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2008 Google, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef ResourceRequest_h
#define ResourceRequest_h

#include "CString.h"
#include "ResourceRequestBase.h"

namespace WebCore {

    class Frame;

    class ResourceRequest : public ResourceRequestBase {
    public:
        ResourceRequest(const String& url) 
            : ResourceRequestBase(KURL(ParsedURLString, url), UseProtocolCachePolicy)
            , m_requestorID(0)
            , m_requestorProcessID(0)
            , m_appCacheHostID(0)
        {
        }

        ResourceRequest(const KURL& url, const CString& securityInfo) 
            : ResourceRequestBase(url, UseProtocolCachePolicy)
            , m_requestorID(0)
            , m_requestorProcessID(0)
            , m_appCacheHostID(0)
            , m_securityInfo(securityInfo)
        {
        }

        ResourceRequest(const KURL& url) 
            : ResourceRequestBase(url, UseProtocolCachePolicy)
            , m_requestorID(0)
            , m_requestorProcessID(0)
            , m_appCacheHostID(0)
        {
        }

        ResourceRequest(const KURL& url, const String& referrer, ResourceRequestCachePolicy policy = UseProtocolCachePolicy) 
            : ResourceRequestBase(url, policy)
            , m_requestorID(0)
            , m_requestorProcessID(0)
            , m_appCacheHostID(0)
        {
            setHTTPReferrer(referrer);
        }
        
        ResourceRequest()
            : ResourceRequestBase(KURL(), UseProtocolCachePolicy)
            , m_requestorID(0)
            , m_requestorProcessID(0)
            , m_appCacheHostID(0)
        {
        }

        // Allows the request to be matched up with its requestor.
        int requestorID() const { return m_requestorID; }
        void setRequestorID(int requestorID) { m_requestorID = requestorID; }

        // The process id of the process from which this request originated. In
        // the case of out-of-process plugins, this allows to link back the
        // request to the plugin process (as it is processed through a render
        // view process).
        int requestorProcessID() const { return m_requestorProcessID; }
        void setRequestorProcessID(int requestorProcessID) { m_requestorProcessID = requestorProcessID; }

        // Allows the request to be matched up with its app cache host.
        int appCacheHostID() const { return m_appCacheHostID; }
        void setAppCacheHostID(int id) { m_appCacheHostID = id; }

        // Opaque buffer that describes the security state (including SSL
        // connection state) for the resource that should be reported when the
        // resource has been loaded.  This is used to simulate secure
        // connection for request (typically when showing error page, so the
        // error page has the errors of the page that actually failed).  Empty
        // string if not a secure connection.
        CString securityInfo() const { return m_securityInfo; }
        void setSecurityInfo(const CString& value) { m_securityInfo = value; }

    private:
        friend class ResourceRequestBase;

        void doUpdatePlatformRequest() {}
        void doUpdateResourceRequest() {}

        int m_requestorID;
        int m_requestorProcessID;
        int m_appCacheHostID;
        CString m_securityInfo;
    };

} // namespace WebCore

#endif
