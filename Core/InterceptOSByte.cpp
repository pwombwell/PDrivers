#include "InterceptOSByte.h"

#include "Job.h"
#include "SpriteMonitor.h"
#include "Workspace.h"

// To pass on the vector.
#include "RLib/KernelWrapper.h"

// Is this an OS_Byte call we're not interested in, either because we're
// passing OS_Byte calls through to the real routines or because it's neither
// OS_Byte &DA (read/write bytes in VDU queue) nor OSByte &A3 (set dot pattern
// length)?

// Also intercept OS_Byte &87 (read mode / char under cursor)
// This can only return the mode number (of the sprite),
// as the sprite buffer may be too small [to read the character at the
// text cursor position] (address exceptions can occur).

// Modified by DJS to use the global (and accurate) "spriteparams" variable
// rather than per job "jobspriteparams" variable, which simply says which
// sprite belongs to each job rather than where output is currently going.

VectorReturn InterceptOSByte::intercept(OS::Regs& regs,
                                        vectortrap_f trap, void* trappw)
{
    CoreWS& ws(CoreWS::instance());

    if (ws.interceptMgr().hasPassthrough(Passthrough_Byte))
        return VectorReturn::pass();

    // Intercept but do precisely nothing if it is a counting pass.
    if (ws.isCountingPass())
        return VectorReturn::pass();

    uint8_t reason = regs[0] & 0xff;
    switch (reason) {
        case 135: // 0x87
            return handle135(regs);

        case 163: // 0xa3
            return handle163(regs);

        case 218: // 0xda
            return handle218(regs, trap, trappw);

        default:
            return VectorReturn::pass();
    }
}

VectorReturn InterceptOSByte::handle135(OS::Regs& regs)
{
    // 0x87 = read mode / char under cursor.
    // https://www.riscosopen.org/wiki/documentation/show/OS_Byte%20135

    // If output is going to the sprite buffer, look up its mode directly.
    // This is to avoid address exceptions if the sprite is small.
    const SpriteMonitor& spriteMonitor(CoreWS::instance().spriteMonitor());

    // If output going to screen, pass it on.
    if (spriteMonitor.isRedirectionToScreen())
        return VectorReturn::pass();

    // Selector id is always be stored in absolute form.
    Sprite::Id id = spriteMonitor.currentSpriteId();

    regs[1] = 0; // character not recognised
    regs[2] = (uint32_t)(uintptr_t)id.header();

    // Don't pass it on.
    return VectorReturn::claim();
}

// `interceptbyte_dotdashlength`
VectorReturn InterceptOSByte::handle163(OS::Regs& regs)
{
    // 0xa3 = set dot pattern length
    // We're only interested in r1 = &f2, r2 <= 64.
    uint32_t length = regs[2] & 0xff;

    if ((regs[1] & 0xff) != 0xf2 || length > 64)
        return VectorReturn::pass();

    CoreJobWS& job(*CoreWS::instance().currentJob());

    // Deal with 1-64 (and redundantly 0 as well) by storing length.
    job.dottedlength = length;

    // Deal with 0 by setting default dot pattern.
    if (length == 0)
        job.setDefaultDotPattern();

    // Claim call.
    return VectorReturn::claim();
}

// `interceptbyte_readwritequeuelength`
VectorReturn InterceptOSByte::handle218(OS::Regs& regs, vectortrap_f trap,
                                        void* trappw)
{
    // 0xda = read/write bytes in VDU queue

    // This is OS_Byte &DA (read/write bytes in VDU queue). First call the next
    // owner of the vector to set R2's return value, then do the main body of
    // the call on our own variables.
    uint32_t origR0 = regs[0];
    uint32_t origR1 = regs[1];

    regs[0] = 0xda; // Don't change VDU's variable.
    regs[1] = 0;
    regs[2] = 255;

    OS::KernelRegsWrapper kernelRegs(regs);
    MyError err = (*trap)(kernelRegs, trappw);
    if (err)
        return err;

    CoreJobWS& job(*CoreWS::instance().currentJob());
    uint32_t oldQueuePos, newQueuePos;

    oldQueuePos = job.wrch().queuePos();    // old +ve queue position
    oldQueuePos = 0 - oldQueuePos;          // old -ve queue position

    newQueuePos = oldQueuePos & origR1;     // new -ve queue position
    newQueuePos ^= origR0;
    newQueuePos = 0 - newQueuePos;          // new +ve queue position

    // Update our queue position.
    job.wrch().setQueuePos((uint8_t)newQueuePos);

    // Produce correct r1 to return.
    regs[1] = oldQueuePos & 0xff;

    // Claim call.
    return VectorReturn::claim();
}
