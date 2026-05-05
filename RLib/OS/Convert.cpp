#include "swis.h"

#include "RLib/RLib.h"
#include "RLibX/Utils/Debug.h"
#include "Convert.h"

namespace riscos {
namespace OS {

MyError binaryToDecimal(int32_t value, char* buffer, size_t size, uint32_t& len)
{
    return _swix(OS_BinaryToDecimal, _INR(0,2) | _OUT(2),
                 value, buffer, size, &len);
}

MyError convertCardinal4(uint32_t value, char* buffer, uint32_t len)
{
    return _swix(OS_ConvertCardinal4, _INR(0,2),
                 value, buffer, len);
}

MyError gsTrans(const char* string, char* buffer, size_t size, uint32_t& written)
{
    return _swix(OS_GSTrans, _INR(0,2) | _OUT(2),
                 string, buffer, size, &written);
}

} } // namespace riscos::OS
