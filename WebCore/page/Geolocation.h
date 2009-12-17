/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
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

#ifndef Geolocation_h
#define Geolocation_h

#include "GeolocationService.h"
#include "Geoposition.h"
#include "PositionCallback.h"
#include "PositionError.h"
#include "PositionErrorCallback.h"
#include "PositionOptions.h"
#include "Timer.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Platform.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class Frame;

#if ENABLE(CLIENT_BASED_GEOLOCATION)
class GeolocationPosition;
class GeolocationError;
#endif

class Geolocation : public RefCounted<Geolocation>
#if !ENABLE(CLIENT_BASED_GEOLOCATION)
    , public GeolocationServiceClient
#endif
{
public:
    static PassRefPtr<Geolocation> create(Frame* frame) { return adoptRef(new Geolocation(frame)); }

    virtual ~Geolocation();

    void disconnectFrame();
    
    Geoposition* lastPosition();

    void getCurrentPosition(PassRefPtr<PositionCallback>, PassRefPtr<PositionErrorCallback>, PassRefPtr<PositionOptions>);
    int watchPosition(PassRefPtr<PositionCallback>, PassRefPtr<PositionErrorCallback>, PassRefPtr<PositionOptions>);
    void clearWatch(int watchId);
    
    void setIsAllowed(bool);
    bool isAllowed() const { return m_allowGeolocation == Yes; }
    bool isDenied() const { return m_allowGeolocation == No; }
    
    void setShouldClearCache(bool shouldClearCache) { m_shouldClearCache = shouldClearCache; }
    bool shouldClearCache() const { return m_shouldClearCache; }

#if ENABLE(CLIENT_BASED_GEOLOCATION)
    void setPostion(GeolocationPosition*);
    void setError(GeolocationError*);
#endif

private:
    Geolocation(Frame*);

    class GeoNotifier : public RefCounted<GeoNotifier> {
    public:
        static PassRefPtr<GeoNotifier> create(Geolocation* geolocation, PassRefPtr<PositionCallback> positionCallback, PassRefPtr<PositionErrorCallback> positionErrorCallback, PassRefPtr<PositionOptions> options) { return adoptRef(new GeoNotifier(geolocation, positionCallback, positionErrorCallback, options)); }
        
        void setFatalError(PassRefPtr<PositionError>);
        bool hasZeroTimeout() const;
        void startTimerIfNeeded();
        void timerFired(Timer<GeoNotifier>*);
        
        Geolocation* m_geolocation;
        RefPtr<PositionCallback> m_successCallback;
        RefPtr<PositionErrorCallback> m_errorCallback;
        RefPtr<PositionOptions> m_options;
        Timer<GeoNotifier> m_timer;
        RefPtr<PositionError> m_fatalError;

    private:
        GeoNotifier(Geolocation*, PassRefPtr<PositionCallback>, PassRefPtr<PositionErrorCallback>, PassRefPtr<PositionOptions>);
    };

    class Watchers {
    public:
        void set(int id, PassRefPtr<GeoNotifier>);
        void remove(int id);
        void remove(GeoNotifier*);
        void clear();
        bool isEmpty() const;
        void getNotifiersVector(Vector<RefPtr<GeoNotifier> >&) const;
    private:
        typedef HashMap<int, RefPtr<GeoNotifier> > IdToNotifierMap;
        typedef HashMap<RefPtr<GeoNotifier>, int> NotifierToIdMap;
        IdToNotifierMap m_idToNotifierMap;
        NotifierToIdMap m_notifierToIdMap;
    };

    bool hasListeners() const { return !m_oneShots.isEmpty() || !m_watchers.isEmpty(); }

    void sendError(Vector<RefPtr<GeoNotifier> >&, PositionError*);
    void sendPosition(Vector<RefPtr<GeoNotifier> >&, Geoposition*);
    
    static void stopTimer(Vector<RefPtr<GeoNotifier> >&);
    void stopTimersForOneShots();
    void stopTimersForWatchers();
    void stopTimers();
    
    void positionChanged(PassRefPtr<Geoposition>);
    void makeSuccessCallbacks();
    void handleError(PositionError*);

    void requestPermission();

    bool startUpdating(PositionOptions*);
    void stopUpdating();
    void suspend();
    void resume();

#if !ENABLE(CLIENT_BASED_GEOLOCATION)
    // GeolocationServiceClient
    virtual void geolocationServicePositionChanged(GeolocationService*);
    virtual void geolocationServiceErrorOccurred(GeolocationService*);
#endif

    PassRefPtr<GeoNotifier> startRequest(PassRefPtr<PositionCallback>, PassRefPtr<PositionErrorCallback>, PassRefPtr<PositionOptions>);

    void fatalErrorOccurred(GeoNotifier*);
    void requestTimedOut(GeoNotifier*);

    typedef HashSet<RefPtr<GeoNotifier> > GeoNotifierSet;
    
    GeoNotifierSet m_oneShots;
    Watchers m_watchers;
    Frame* m_frame;
#if !ENABLE(CLIENT_BASED_GEOLOCATION)
    OwnPtr<GeolocationService> m_service;
#endif
    RefPtr<Geoposition> m_lastPosition;
    RefPtr<Geoposition> m_currentPosition;

    enum {
        Unknown,
        InProgress,
        Yes,
        No
    } m_allowGeolocation;
    bool m_shouldClearCache;
};
    
} // namespace WebCore

#endif // Geolocation_h
