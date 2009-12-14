/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GeolocationController.h"

#if ENABLE(CLIENT_BASED_GEOLOCATION)

#include "GeolocationControllerClient.h"

namespace WebCore {

GeolocationController::GeolocationController(Page* page, GeolocationControllerClient* client)
    : m_page(page)
    , m_client(client)
{
}

GeolocationController::~GeolocationController()
{
}

void GeolocationController::addObserver(Geolocation* observer)
{
    ASSERT(!m_observers.contains(observer));

    bool wasEmpty = m_observers.isEmpty();
    m_observers.add(observer);
    if (wasEmpty)
        m_client->startUpdating();
}

void GeolocationController::removeObserver(Geolocation* observer)
{
    ASSERT(m_observers.contains(observer));

    m_observers.remove(observer);
    if (m_observers.isEmpty())
        m_client->stopUpdating();
}

void GeolocationController::positionChanged(GeolocationPosition* position)
{
    HashSet<RefPtr<Geolocation> >::const_iterator end = m_observers.end();
    for (HashSet<RefPtr<Geolocation> >::const_iterator it = m_observers.begin(); it != end; ++it)
        (*it)->setPosition(position);
}

void GeolocationController::errorOccurred(GeolocationError* error)
{
    HashSet<RefPtr<Geolocation> >::const_iterator end = m_observers.end();
    for (HashSet<RefPtr<Geolocation> >::const_iterator it = m_observers.begin(); it != end; ++it)
        (*it)->setError(error);
}

GeolocationPosition* GeolocationController::lastPosition()
{
    return m_client->lastPosition();
}

} // namespace WebCore

#endif // ENABLE(CLIENT_BASED_GEOLOCATION)
