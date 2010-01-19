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
 
#ifndef TimelineRecordFactory_h
#define TimelineRecordFactory_h

#include "PlatformString.h"

namespace WebCore {

    class Event;
    class InspectorFrontend;
    class IntRect;
    class ResourceRequest;
    class ResourceResponse;
    class ScriptObject;

    class TimelineRecordFactory {
    public:
        static ScriptObject createGenericRecord(InspectorFrontend*, double startTime);

        static ScriptObject createEventDispatchData(InspectorFrontend*, const Event&);

        static ScriptObject createGenericTimerData(InspectorFrontend*, int timerId);

        static ScriptObject createTimerInstallData(InspectorFrontend*, int timerId, int timeout, bool singleShot);

        static ScriptObject createXHRReadyStateChangeData(InspectorFrontend*, const String& url, int readyState);

        static ScriptObject createXHRLoadData(InspectorFrontend*, const String& url);
        
        static ScriptObject createEvaluateScriptData(InspectorFrontend*, const String&, double lineNumber);
        
        static ScriptObject createMarkTimelineData(InspectorFrontend*, const String&);

        static ScriptObject createResourceSendRequestData(InspectorFrontend*, unsigned long identifier,
            bool isMainResource, const ResourceRequest&);

        static ScriptObject createResourceReceiveResponseData(InspectorFrontend*, unsigned long identifier, const ResourceResponse&);

        static ScriptObject createResourceFinishData(InspectorFrontend*, unsigned long identifier, bool didFail);

        static ScriptObject createPaintData(InspectorFrontend*, const IntRect&);

        static ScriptObject createParseHTMLData(InspectorFrontend*, unsigned int length, unsigned int startLine);

    private:
        TimelineRecordFactory() { }
    };

} // namespace WebCore

#endif // !defined(TimelineRecordFactory_h)
