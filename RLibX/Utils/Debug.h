#ifndef RLIB_UTILS_DEBUG_H
#define RLIB_UTILS_DEBUG_H

#include <stdarg.h>

#define RLIB_DEBUG 1

void debugLog(const char* text, ...);
void debugLogv(const char* fmt, va_list ap);

#endif
