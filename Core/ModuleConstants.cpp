#include "cmhg.h"

#include "Constants.h"
#include "InterceptManager.h"
#include "OS.h"

#include <stdint.h>

extern const uint32_t Module_Version = 465u;

extern const uint32_t PDriverMiscOp_DriverSpecific = 0x80000000u;
extern const uint32_t PDumperMiscOp_DumperSpecific =
    PDriverMiscOp_DriverSpecific + 0x100u;

extern const uint32_t PDumperReason_SetDriver = 0u;
extern const uint32_t PDumperReason_OutputDump = 1u;
extern const uint32_t PDumperReason_ColourSet = 2u;
extern const uint32_t PDumperReason_StartPage = 3u;
extern const uint32_t PDumperReason_EndPage = 4u;
extern const uint32_t PDumperReason_StartJob = 5u;
extern const uint32_t PDumperReason_AbortJob = 6u;
extern const uint32_t PDumperReason_MiscOp = 7u;

extern const uint32_t passthrough_wrch = Passthrough_Wrch;
extern const uint32_t passthrough_spr = Passthrough_Sprite;

_kernel_oserror* PDriver_BadSWI_Handler(int number,
                                        _kernel_swi_regs* r,
                                        void* pw)
{
    debugLog("bad swi:%d", number);
    (void)number;
    (void)r;
    (void)pw;
    return error_BAD_PDRIVER_SWI;
}
