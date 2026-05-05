#include "Core/PDriver.h"
#include "ScreenDump.h"

#include "Core/MsgCode.h"

#include "RLib/OS/Error.h"

MyError screendump_dump(int32_t handle)
{
    (void)handle;
    return ErrorBlock_PrintNoScreenDump;
}
