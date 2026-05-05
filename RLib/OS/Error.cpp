#include "Error.h"
#include "Core/Constants.h"
#include "RLib/RLib.h"

namespace riscos {

static MAKE_INTERNAT_ERROR_BLOCK(HeapFail_Alloc, "OutOfMem");

MyError MyError::OOM()
{
    // FIXME: Should MessageTrans this.
    return ErrorBlock_HeapFail_Alloc;
}

} // namespace riscos::OS
