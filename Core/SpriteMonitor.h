#ifndef SPRITE_MONITOR_H
#define SPRITE_MONITOR_H

#include "RLib/OS.h"
#include "RLib/OS/Vector.h"
#include "RLibX/Sprite.h"

class CoreWS;
class CoreJobWS;

struct _kernel_oserror;
struct _kernel_swi_regs;

// Defined in cmhg.h, but that drags in swis.h and kernel.h.
typedef _kernel_oserror *(*vectortrap_f)(_kernel_swi_regs *regs,void *trappw);

class SpriteMonitor {
public:
    SpriteMonitor() { }
    ~SpriteMonitor();

    bool isRedirectionToScreen() const { return m_context.isUnset(); }

    // Check if the current redirection has been set by the given job.
    bool isRedirectionToJob(CoreJobWS& job) const;

    // Sprite::Id is always stored in absolute form.
    Sprite::Id currentSpriteId() const { return m_context.spriteId(); }

    // To reset the state, after an appropriate service call.
    void setToScreen() { m_context.setToScreen(); }

    MyError start();
    MyError stop();

    // Entry point from cmhg.
    VectorReturn monitor(OS::Regs& regs, vectortrap_f trap, void* trappw);

private:
    // Context is always correct, while the interception is on.
    // Currently that's the lifetime of the module, but we probably ought
    // to make it while any job is active.
    Sprite::VDUContext m_context;   // `spriteparams`
};

#endif
