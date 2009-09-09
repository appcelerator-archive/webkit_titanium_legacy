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

#ifndef InspectorTimelineAgent_h
#define InspectorTimelineAgent_h

#include "ScriptObject.h"
#include "ScriptArray.h"

#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {
    class Event;
    class InspectorFrontend;
    class TimelineItem;

    class InspectorTimelineAgent {
    public:
        InspectorTimelineAgent(InspectorFrontend* frontend);
        ~InspectorTimelineAgent();

        void reset();

        // Methods called from WebCore.
        void willDispatchDOMEvent(const Event&);
        void didDispatchDOMEvent();
        void willLayout();
        void didLayout();
        void willRecalculateStyle();
        void didRecalculateStyle();
        void willPaint();
        void didPaint();
        void didWriteHTML();
        void willWriteHTML();
    private:
        double sessionTimeInMilliseconds();

        static double currentTimeInMilliseconds();

        void didCompleteCurrentRecord();

        double m_sessionStartTime;
        InspectorFrontend* m_frontend;
        OwnPtr<TimelineItem> m_currentTimelineItem;
    };

} // namespace WebCore

#endif // !defined(InspectorTimelineAgent_h)
