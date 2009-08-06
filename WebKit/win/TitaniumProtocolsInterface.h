#ifndef TitaniumProtocolsInterface_h
#define TitaniumProtocolsInterface_h

#include "WebKit.h"
#include <WebCore/BString.h>
#include "TitaniumProtocols.h"

typedef void(*NormalizeURLCallback)(const char*, char*, int);
typedef void(*URLToFileURLCallback)(const char*, char*, int);

#ifdef __cplusplus
extern "C" {
#endif

    __declspec(dllexport) void setNormalizeURLCallback(NormalizeURLCallback cb);
    __declspec(dllexport) void setURLToFileURLCallback(URLToFileURLCallback cb);

#ifdef __cplusplus
}
#endif

#endif
