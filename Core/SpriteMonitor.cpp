#include "SpriteMonitor.h"

#include "Workspace.h"
#include "Job.h"

#include "RLib/Constants/Sprite.h"
#include "RLib/KernelWrapper.h"

#include "cmhg.h"

SpriteMonitor::~SpriteMonitor()
{
    stop();
}

bool SpriteMonitor::isRedirectionToJob(CoreJobWS& job) const
{
    if (m_context.isUnset())
        return false;

    return m_context == job.jobspriteparams;
}

// SpriteV interception routine to deal with redirection of output to sprites,
// etc. Unlike the other interception routines, this is around all the time,
// not just when a print job is active.
// `monitoroutput` in Header.s
VectorReturn SpriteMonitor::monitor(OS::Regs& regs,
                                    vectortrap_f trap, void* trappw)
{
    uint32_t reason = regs[0];

    // We are only interested in sprite ops 60 and 61, regardless of bits 8, 9.
    // "this cheap test next aliases at 124"
#if 1
    if ((reason & 0x3e) != 0x3c)
        return VectorReturn::pass();
#else
    reason &= 0xff;
    if (reason != SpriteReason_SwitchOutputToSprite &&
        reason != SpriteReason_SwitchOutputToMask)
    {
        return VectorReturn::pass();
    }
#endif

    CoreWS& ws(CoreWS::instance());
    bool isSwitchingToScreen = regs[2] == 0;

    // Also, don't interfere if there's an error box open
    if (ws.m_interceptMgr.hasWimpReportError())
        return VectorReturn::pass();

    // Store original context on stack for restoration in `monitoroutput_cont`
    Sprite::VDUContext original(regs[0], regs[1], regs[2], regs[3]);

    CoreJobWS* job = ws.currentJob();
    if (job && isSwitchingToScreen) {
        const Sprite::VDUContext& jobSpriteParams = job->jobspriteparams;
        regs[0] = jobSpriteParams.rawWord(0);
        regs[1] = jobSpriteParams.rawWord(1);
        regs[2] = jobSpriteParams.rawWord(2);
        regs[3] = jobSpriteParams.rawWord(3);
    }

    // And now pass on, but return to us afterwards.
    MyError err = (*trap)(OS::KernelRegsWrapper(regs), trappw);
    if (err)
        return err;

    // `monitoroutput_cont` - return from vector.

    // Store original values in global 'spriteparams'.
    m_context = original;

    return ws.m_interceptMgr.adjust();
}

MyError SpriteMonitor::start()
{
    CoreWS& ws = CoreWS::instance();
    return ws.claimVector(OS::Vector_SpriteV, monitor_sprite_vector);
}

MyError SpriteMonitor::stop()
{
    CoreWS& ws = CoreWS::instance();
    return ws.releaseVector(OS::Vector_SpriteV, monitor_sprite_vector);
}

// cmhg entry point.
_kernel_oserror* monitor_sprite_vector_handler(_kernel_swi_regs* r, void* pw,
                                               vectortrap_f trap, void* trappw)
{
    OS::KernelRegsWrapper regs(r);
    CoreWS& ws = CoreWS::instance();

    VectorReturn ret = ws.spriteMonitor().monitor(regs, trap, trappw);

    return ret.toCMHGTrap();
}
