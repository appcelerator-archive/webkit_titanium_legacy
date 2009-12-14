/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorBackend_h
#define InspectorBackend_h

#include "Console.h"
#include "InspectorController.h"
#include "PlatformString.h"

#include <wtf/RefCounted.h>

namespace WebCore {

class CachedResource;
class Database;
class InspectorDOMAgent;
class InspectorFrontend;
class JavaScriptCallFrame;
class Node;
class Storage;

class InspectorBackend : public RefCounted<InspectorBackend>
{
public:
    static PassRefPtr<InspectorBackend> create(InspectorController* inspectorController)
    {
        return adoptRef(new InspectorBackend(inspectorController));
    }

    ~InspectorBackend();

    InspectorController* inspectorController() { return m_inspectorController; }
    void disconnectController() { m_inspectorController = 0; }

    void clearMessages(bool clearUI);
    void storeLastActivePanel(const String& panelName);

    void toggleNodeSearch();
    bool searchingForNode();

    void enableResourceTracking(bool always);
    void disableResourceTracking(bool always);
    bool resourceTrackingEnabled() const;

    void startTimelineProfiler();
    void stopTimelineProfiler();

#if ENABLE(JAVASCRIPT_DEBUGGER)
    bool debuggerEnabled() const;
    void enableDebugger(bool always);
    void disableDebugger(bool always);

    void addBreakpoint(const String& sourceID, unsigned lineNumber, const String& condition);
    void updateBreakpoint(const String& sourceID, unsigned lineNumber, const String& condition);
    void removeBreakpoint(const String& sourceID, unsigned lineNumber);

    void pauseInDebugger();
    void resumeDebugger();

    bool pauseOnExceptions();
    void setPauseOnExceptions(bool pause);

    void stepOverStatementInDebugger();
    void stepIntoStatementInDebugger();
    void stepOutOfFunctionInDebugger();

    bool profilerEnabled();
    void enableProfiler(bool always);
    void disableProfiler(bool always);

    void startProfiling();
    void stopProfiling();

    void getProfileHeaders(long callId);
    void getProfile(long callId, unsigned uid);

    JavaScriptCallFrame* currentCallFrame() const;
#endif

    void dispatchOnInjectedScript(long callId, const String& methodName, const String& arguments, bool async);
    void getChildNodes(long callId, long nodeId);
    void setAttribute(long callId, long elementId, const String& name, const String& value);
    void removeAttribute(long callId, long elementId, const String& name);
    void setTextNodeValue(long callId, long nodeId, const String& value);
    void getEventListenersForNode(long callId, long nodeId);
    void copyNode(long nodeId);
    void removeNode(long callId, long nodeId);
    void highlightDOMNode(long nodeId);
    void hideDOMNodeHighlight();

    void getCookies(long callId);
    void deleteCookie(const String& cookieName, const String& domain);

    // Generic code called from custom implementations.
    void releaseWrapperObjectGroup(const String& objectGroup);
    void didEvaluateForTestInFrontend(long callId, const String& jsonResult);

#if ENABLE(DATABASE)
    void getDatabaseTableNames(long callId, long databaseId);
#endif

#if ENABLE(DOM_STORAGE)
    void getDOMStorageEntries(long callId, long storageId);
    void setDOMStorageItem(long callId, long storageId, const String& key, const String& value);
    void removeDOMStorageItem(long callId, long storageId, const String& key);
#endif

private:
    InspectorBackend(InspectorController* inspectorController);
    InspectorDOMAgent* inspectorDOMAgent();
    InspectorFrontend* inspectorFrontend();
    Node* nodeForId(long nodeId);

    InspectorController* m_inspectorController;
};

} // namespace WebCore

#endif // !defined(InspectorBackend_h)
