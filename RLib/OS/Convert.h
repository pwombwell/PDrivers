#ifndef OS_CONVERT_H
#define OS_CONVERT_H

#include "RLib/RLib.h"

namespace riscos {
namespace OS {

// Convert value to string. len is in/out. PRM Is unclear if it's
// null-terminated. Max value is "-2147483648\0".
MyError binaryToDecimal(int32_t value, char* buffer, size_t size, uint32_t& written);

// Convert unsigned value to null-terminated string. Max value is "4294967295\0".
MyError convertCardinal4(uint32_t value, char* buffer, uint32_t size);

MyError gsTrans(const char* string, char* buffer, size_t size, uint32_t& written);

} } // namespace riscos::OS

#endif
