#include "Core/PDriver.h"
#include "ScreenDump.h"

#include "Core/Job.h"
#include "Core/MsgCode.h"
#include "Core/Workspace.h"

#include "RLib/OS/Error.h"

MyError screendump_dump(int32_t handle) {
    (void)handle;

    return CoreWS::instance().messages.lookupError(ErrorBlock_PrintNoScreenDump, "PDriverDP");
}
