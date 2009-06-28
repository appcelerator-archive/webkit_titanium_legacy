#ifndef TitaniumProtocols_h
#define TitaniumProtocols_h

#include "KURL.h"
#include "CString.h"
typedef void(*NormalizeURLCallback)(const char*, char*, int);
typedef void(*URLToPathCallback)(const char*, char*, int);

namespace WebCore {

    class TitaniumProtocols {
    public:
        static KURL NormalizeURL(KURL url);
        static KURL URLToFileURL(KURL url);
        static NormalizeURLCallback NormalizeCallback;
        static URLToPathCallback URLCallback;
    };

} // namespace WebCore

#endif // TitaniumProtocols_h
