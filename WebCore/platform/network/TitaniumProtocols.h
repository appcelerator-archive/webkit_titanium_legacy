#ifndef TitaniumProtocols_h
#define TitaniumProtocols_h

#include "KURL.h"

#ifndef KEYVALUESTRUCT
typedef struct {
    char* key;
    char* value;
} KeyValuePair;
#define KEYVALUESTRUCT 1
#endif

typedef void(*NormalizeURLCallback)(const char*, char*, int);
typedef void(*URLToPathCallback)(const char*, char*, int);
typedef int(*CanPreprocessURLCallback)(const char*);
typedef char*(*PreprocessURLCallback)(const char* url, KeyValuePair* headers, char** mimeType);

namespace WebCore {

    class TitaniumProtocols {
        public:
        static KURL NormalizeURL(KURL url);
        static KURL URLToFileURL(KURL url);
        static bool CanPreprocess(const ResourceRequest& request);
        static String Preprocess(const ResourceRequest& request, String& mimeType);
        static NormalizeURLCallback NormalizeCallback;
        static URLToPathCallback URLCallback;
        static CanPreprocessURLCallback CanPreprocessCallback;
        static PreprocessURLCallback PreprocessCallback;
    };

} // namespace WebCore

#endif // TitaniumProtocols_h
