/*
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StorageNamespaceProxy.h"

#if ENABLE(DOM_STORAGE)

#include "SecurityOrigin.h"
#include "StorageAreaProxy.h"
#include "WebKit.h"
#include "WebKitClient.h"
#include "WebStorageNamespace.h"
#include "WebString.h"

namespace WebCore {

PassRefPtr<StorageNamespace> StorageNamespace::localStorageNamespace(const String& path, unsigned quota)
{
    return new StorageNamespaceProxy(WebKit::webKitClient()->createLocalStorageNamespace(path, quota));
}

PassRefPtr<StorageNamespace> StorageNamespace::sessionStorageNamespace()
{
    return new StorageNamespaceProxy(WebKit::webKitClient()->createSessionStorageNamespace());
}

StorageNamespaceProxy::StorageNamespaceProxy(WebKit::WebStorageNamespace* storageNamespace)
    : m_storageNamespace(storageNamespace)
{
}

StorageNamespaceProxy::~StorageNamespaceProxy()
{
}

PassRefPtr<StorageNamespace> StorageNamespaceProxy::copy()
{
    return adoptRef(new StorageNamespaceProxy(m_storageNamespace->copy()));
}

PassRefPtr<StorageArea> StorageNamespaceProxy::storageArea(PassRefPtr<SecurityOrigin> origin)
{
    return adoptRef(new StorageAreaProxy(m_storageNamespace->createStorageArea(origin->toString())));
}

void StorageNamespaceProxy::close()
{
    m_storageNamespace->close();
}

void StorageNamespaceProxy::unlock()
{
    // FIXME: Implement.
}

} // namespace WebCore

#endif // ENABLE(DOM_STORAGE)
