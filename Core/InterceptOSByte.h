#ifndef CORE_INTERCEPT_OSBYTE_H
#define CORE_INTERCEPT_OSBYTE_H

#include "InterceptManager.h"

#include "RLib/OS.h"
#include "RLib/OS/Vector.h"

#include <stdint.h>

class CoreWS;
class CoreJobWS;

struct _kernel_oserror;
struct _kernel_swi_regs;

// Defined in cmhg.h, but that drags in swis.h and kernel.h.
typedef _kernel_oserror *(*vectortrap_f)(_kernel_swi_regs *regs,void *trappw);

class InterceptOSByte : public InterceptorBase
{
public:
    // Entry point from cmhg.
    VectorReturn intercept(OS::Regs& regs, vectortrap_f trap, void* trappw);

private:
    // 0x87 = read mode / char under cursor
    VectorReturn handle135(OS::Regs& regs);

    // 0xa3 = set dot pattern length
    VectorReturn handle163(OS::Regs& regs);

    // 0xda = read/write bytes in VDU queue
    VectorReturn handle218(OS::Regs& regs, vectortrap_f trap, void* trappw);
};

#endif
