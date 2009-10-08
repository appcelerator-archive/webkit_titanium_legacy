#ifndef WebKitTitanium_h
#define WebKitTitanium_h
#include <WebKit/WebKit.h>

#ifdef WEBKIT_EXPORTS
#define WEBKIT_API __declspec(dllexport)
#else
#define WEBKIT_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif
    WEBKIT_API typedef void(*NormalizeURLCallback)(const char*, char*, int);
    WEBKIT_API typedef void(*URLToFileURLCallback)(const char*, char*, int);
    WEBKIT_API void setNormalizeURLCallback(NormalizeURLCallback cb);
    WEBKIT_API void setURLToFileURLCallback(URLToFileURLCallback cb);
#ifdef __cplusplus
}
#endif

class IWebScriptEvaluator;
void STDMETHODCALLTYPE addScriptEvaluator(IWebScriptEvaluator *evaluator);
#endif
