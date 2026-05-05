#include "CharInput.h"

#include "swis.h"

namespace riscos {
namespace OS {

bool readEscapeState()
{
    uint32_t flags;
    if (_swix(OS_ReadEscapeState, _RETURN(_FLAGS), &flags) != nullptr)
        return false;

    return (flags & _C) != 0;
}

} } // namespace riscos::OS
