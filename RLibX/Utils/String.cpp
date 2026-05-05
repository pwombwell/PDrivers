#include "String.h"

#include "RLib/RLib.h"
#include <stdlib.h>
#include <string.h>

char* strdup(const char* s)
{
    if (s == nullptr)
        return nullptr;

    size_t len = strlen(s) + 1;
    char* dst = (char*)malloc(len);
    if (dst == nullptr)
        return nullptr;
    
    memcpy(dst, s, len);
    return dst;
}