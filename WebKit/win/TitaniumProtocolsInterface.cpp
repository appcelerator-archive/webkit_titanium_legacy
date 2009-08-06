#include "WebKit.h"
#include "TitaniumProtocols.h"
#include "TitaniumProtocolsInterface.h"

void setNormalizeURLCallback(NormalizeURLCallback cb) {
    WebCore::TitaniumProtocols::NormalizeCallback = cb;
}

void setURLToFileURLCallback(URLToFileURLCallback cb) {
    WebCore::TitaniumProtocols::URLCallback = cb;
}
