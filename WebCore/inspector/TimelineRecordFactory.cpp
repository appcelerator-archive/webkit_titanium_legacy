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
#include "TimelineRecordFactory.h"

#if ENABLE(INSPECTOR)

#include "Event.h"
#include "InspectorFrontend.h"
#include "IntRect.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptArray.h"
#include "ScriptObject.h"

namespace WebCore {

ScriptObject TimelineRecordFactory::createGenericRecord(InspectorFrontend* frontend, double startTime)
{
    ScriptObject record = frontend->newScriptObject();
    record.set("startTime", startTime);
    return record;
}

ScriptObject TimelineRecordFactory::createEventDispatchRecord(InspectorFrontend* frontend, double startTime, const Event& event)
{
    ScriptObject record = createGenericRecord(frontend, startTime);
    ScriptObject data = frontend->newScriptObject();
    data.set("type", event.type().string());
    record.set("data", data);
    return record;
}

ScriptObject TimelineRecordFactory::createGenericTimerRecord(InspectorFrontend* frontend, double startTime, int timerId)
{
    ScriptObject record = createGenericRecord(frontend, startTime);
    ScriptObject data = frontend->newScriptObject();
    data.set("timerId", timerId);
    record.set("data", data);
    return record;
}

ScriptObject TimelineRecordFactory::createTimerInstallRecord(InspectorFrontend* frontend, double startTime, int timerId, int timeout, bool singleShot)
{
    ScriptObject record = createGenericRecord(frontend, startTime);
    ScriptObject data = frontend->newScriptObject();
    data.set("timerId", timerId);
    data.set("timeout", timeout);
    data.set("singleShot", singleShot);
    record.set("data", data);
    return record;
}

ScriptObject TimelineRecordFactory::createXHRReadyStateChangeRecord(InspectorFrontend* frontend, double startTime, const String& url, int readyState)
{
    ScriptObject record = createGenericRecord(frontend, startTime);
    ScriptObject data = frontend->newScriptObject();
    data.set("url", url);
    data.set("readyState", readyState);
    record.set("data", data);
    return record;
}

ScriptObject TimelineRecordFactory::createXHRLoadRecord(InspectorFrontend* frontend, double startTime, const String& url)
{
    ScriptObject record = createGenericRecord(frontend, startTime);
    ScriptObject data = frontend->newScriptObject();
    data.set("url", url);
    record.set("data", data);
    return record;
}

ScriptObject TimelineRecordFactory::createEvaluateScriptRecord(InspectorFrontend* frontend, double startTime, const String& url, double lineNumber) 
{
    ScriptObject record = createGenericRecord(frontend, startTime);
    ScriptObject data = frontend->newScriptObject();
    data.set("url", url);
    data.set("lineNumber", lineNumber);
    record.set("data", data);
    return record;
}

ScriptObject TimelineRecordFactory::createMarkTimelineRecord(InspectorFrontend* frontend, double startTime, const String& message) 
{
    ScriptObject record = createGenericRecord(frontend, startTime);
    ScriptObject data = frontend->newScriptObject();
    data.set("message", message);
    record.set("data", data);
    return record;
}


ScriptObject TimelineRecordFactory::createResourceSendRequestRecord(InspectorFrontend* frontend, double startTime,
    unsigned long identifier, bool isMainResource, const ResourceRequest& request)
{
    ScriptObject record = createGenericRecord(frontend, startTime);
    ScriptObject data = frontend->newScriptObject();
    data.set("identifier", identifier);
    data.set("url", request.url().string());
    data.set("requestMethod", request.httpMethod());
    data.set("isMainResource", isMainResource);
    record.set("data", data);
    return record;
}

ScriptObject TimelineRecordFactory::createResourceReceiveResponseRecord(InspectorFrontend* frontend, double startTime,
    unsigned long identifier, const ResourceResponse& response)
{
    ScriptObject record = createGenericRecord(frontend, startTime);
    ScriptObject data = frontend->newScriptObject();
    data.set("identifier", identifier);
    data.set("statusCode", response.httpStatusCode());
    data.set("mimeType", response.mimeType());
    data.set("expectedContentLength", response.expectedContentLength());
    record.set("data", data);
    return record;
}

ScriptObject TimelineRecordFactory::createResourceFinishRecord(InspectorFrontend* frontend, double startTime,
    unsigned long identifier, bool didFail)
{
    ScriptObject record = createGenericRecord(frontend, startTime);
    ScriptObject data = frontend->newScriptObject();
    data.set("identifier", identifier);
    data.set("didFail", didFail);
    record.set("data", data);
    return record;
}

ScriptObject TimelineRecordFactory::createPaintRecord(InspectorFrontend* frontend, double startTime, const IntRect& rect)
{
    ScriptObject record = createGenericRecord(frontend, startTime);
    ScriptObject data = frontend->newScriptObject();
    data.set("x", rect.x());
    data.set("y", rect.y());
    data.set("width", rect.width());
    data.set("height", rect.height());
    record.set("data", data);
    return record;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
