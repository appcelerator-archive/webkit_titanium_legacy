#include "config.h"
#include "StringHash.h"
#include <wtf/Vector.h>
#include "ScriptSourceCode.h"
#include "ScriptEvaluator.h"
#include "ScriptElement.h"
#include "CString.h"
#include "FrameLoaderClientGtk.h"
#include "InspectorClientGtk.h"
#include "TitaniumProtocols.h"
#include <webkit/webkit.h>
#include <webkit/webkittitanium.h>

namespace WebCore {
    class String;
    class ScriptSourceCode;
}

gchar userAgentBuffer[1024];
const gchar* webkit_titanium_get_user_agent()
{
    WebCore::String userAgent = WebKit::FrameLoaderClient::composeUserAgent();
    strncpy(userAgentBuffer, userAgent.utf8().data(), 1023);
    userAgentBuffer[1023] = '\0';
    return userAgentBuffer;
}

void webkit_titanium_set_normalize_url_cb(NormalizeURLCallback cb) {
    WebCore::TitaniumProtocols::NormalizeCallback = cb;
}

void webkit_titanium_set_url_to_file_url_cb(URLToFileURLCallback cb) {
    WebCore::TitaniumProtocols::URLCallback = cb;
}

void webkit_titanium_set_inspector_url(const gchar* url) {
   if (WebKit::CustomGtkWebInspectorPath)
       free(WebKit::CustomGtkWebInspectorPath);
   WebKit::CustomGtkWebInspectorPath = strdup(url);
}

class EvaluatorAdapter : public WebCore::ScriptEvaluator {
    protected:
        WebKitWebScriptEvaluator *evaluator;

    public:
        EvaluatorAdapter(WebKitWebScriptEvaluator *evaluator) 
        : evaluator(evaluator)
        {
        }

        bool matchesMimeType(const WebCore::String &mimeType) {
            return evaluator->matchesMimeType(mimeType.utf8().data());
        }

        void evaluate(const WebCore::String &mimeType, const WebCore::ScriptSourceCode& sourceCode, void *context) {
            evaluator->evaluate(mimeType.utf8().data(), sourceCode.jsSourceCode().toString().ascii(), context);

        }
};

void webkit_titanium_add_script_evaluator(WebKitWebScriptEvaluator *evaluator) {
    WebCore::ScriptElement::addScriptEvaluator(new EvaluatorAdapter(evaluator));
}

