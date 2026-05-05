#include "Debug.h"

#if RLIB_DEBUG
#include <stdarg.h>
#include <stdio.h>
void debugLogv(const char* fmt, va_list ap)
{
    FILE* f = fopen("$.DEBUG", "a");
    if (f == 0)
        return;

    vfprintf(f, fmt, ap);
    fputc('\n', f);
    fflush(f);
    fclose(f);
}

void debugLog(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    debugLogv(fmt, ap);
    va_end(ap);
}
#endif
