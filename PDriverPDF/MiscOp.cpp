#include "kernel.h"

#include "Core/PDriver.h"
#include "MiscOp.h"

#include "Core/Job.h"
#include "Core/MsgCode.h"
#include "Core/Workspace.h"

#include "RLib/OS/Error.h"

/* Device-specific MiscOp handling for the PDF printer driver.
 * All requests are unsupported, so return the standard bad MiscOp error.
 */

MyError miscop_decode(uint32_t reason, OS::Regs& args, CoreWS& ws)
{
    (void)reason;
    (void)args;
    return ws.messages.lookupError(ErrorBlock_PrintBadMiscOp);
}
