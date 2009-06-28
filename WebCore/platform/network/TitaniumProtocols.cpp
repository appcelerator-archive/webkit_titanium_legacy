#include "TitaniumProtocols.h"
#include <cstdio>
#include <assert.h>

typedef void(*NormalizeURLCallback)(const char*, char*, int);
typedef void(*URLToPathCallback)(const char*, char*, int);

namespace WebCore {
    NormalizeURLCallback TitaniumProtocols::NormalizeCallback = 0;
    URLToPathCallback TitaniumProtocols::URLCallback = 0;

    KURL TitaniumProtocols::NormalizeURL(KURL url) {
        assert(NormalizeCallback != 0);

        // If you are using a URL in a Titanium application that is longer
        // than 4KB, either you are from the future or you are doing something
        // artistic or just wrong.
        char* buffer = new char[4096];
        NormalizeCallback(url.string().utf8().data(), buffer, 4096);
        KURL normalizedURL = KURL(buffer);
        delete [] buffer;

        return normalizedURL;
    }

    KURL TitaniumProtocols::URLToFileURL(KURL url) {
        assert(URLCallback != 0);

        // If you are using a URL in a Titanium application that is longer
        // than 4KB, either you are from the future or you are doing something
        // artistic or just wrong.
        char* buffer = new char[4096];
        URLCallback(url.string().utf8().data(), buffer, 4096);
        KURL fileURL = KURL(buffer);
        delete [] buffer;

        return fileURL;
    }
}
