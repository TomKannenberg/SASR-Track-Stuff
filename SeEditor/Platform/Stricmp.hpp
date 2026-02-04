#pragma once

#include <cstring>

#if defined(_WIN32)
    #include <string.h>
    inline int SeStricmp(char const* a, char const* b)
    {
        return ::_stricmp(a, b);
    }
#else
    #include <strings.h>
    inline int SeStricmp(char const* a, char const* b)
    {
        return ::strcasecmp(a, b);
    }
#endif

