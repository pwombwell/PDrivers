#include "PDriver.h"
#include "InterceptManager.h"

#include "Colour.h"
#include "Device.h"
#include "InterceptColTrans.h"
#include "InterceptDraw.h"
#include "InterceptSprite.h"
#include "InterceptWrch.h"
#include "Job.h"
#include "MsgCode.h"
#include "Workspace.h"

#include "cmhg.h"

#include "RLib/Constants/Service.h"
#include "RLib/OS/Vector.h"

#include "swis.h"

#include <string.h>

static bool canInterceptOutput(const CoreWS& ws)
{
    CoreJobWS* job = ws.currentJob();
    if (!job)
        return false;

    const SpriteMonitor& spriteMonitor = ws.spriteMonitor();

    return spriteMonitor.isRedirectionToScreen() || spriteMonitor.isRedirectionToJob(*job);
}


void broadcast_vdu5_changed(CoreWS& ws, uint32_t code)
{
    JobManager& jobMgr(ws.jobMgr());

    CoreJobWS* job = jobMgr.firstJob();

    while (job) {
        vdu5_changed(*job, code);

        job = jobMgr.nextJob(job);
    }
}

// Interceptor ----------------------------------------------------------------
// `initintercept`
void InterceptManager::init()
{
    m_intercepting    = Intercept_None;
    m_shouldIntercept = Intercept_All;
    m_passthrough     = Passthrough_None;
    m_wimpReportFlag  = false;

    m_ws.spriteMonitor().setToScreen();
}

MyError InterceptManager::adjust()
{
    return change(m_shouldIntercept, false);
}

MyError InterceptManager::change(Intercept wanted)
{
    m_shouldIntercept = wanted;

    return change(wanted, false);
}

MyError InterceptManager::change(Intercept wanted, bool recursed)
{
    MyError err;
    Intercept active = m_intercepting;

    Intercept requested = wanted;
    if (wanted && (m_wimpReportFlag || !canInterceptOutput(m_ws)))
        wanted = Intercept_None;

    Intercept changed = wanted ^ active;
    if (changed == Intercept_None)
        return nullptr;

    CoreWS& ws(CoreWS::instance());

    if (!!(changed & Intercept_Wrch)) {
        if (!!(wanted & Intercept_Wrch)) {
            err = ws.claimVector(OS::Vector_WrchV, wrch_vector);
            if (err == nullptr)
                broadcast_vdu5_changed(m_ws, 0);
        } else {
            err = ws.releaseVector(OS::Vector_WrchV, wrch_vector);
        }
        if (err != nullptr)
            goto fail;
        active ^= Intercept_Wrch;
    }

    if (!!(changed & Intercept_ColTrans)) {
        if (!!(wanted & Intercept_ColTrans)) {
            err = ws.claimVector(OS::Vector_ColourV, colour_vector);
        } else {
            err = ws.releaseVector(OS::Vector_ColourV, colour_vector);
        }
        if (err != nullptr)
            goto fail;
        active ^= Intercept_ColTrans;
    }

    if (!!(changed & Intercept_Draw)) {
        if (!!(wanted & Intercept_Draw)) {
            err = ws.claimVector(OS::Vector_DrawV, draw_vector);
        } else {
            err = ws.releaseVector(OS::Vector_DrawV, draw_vector);
        }
        if (err != nullptr)
            goto fail;
        active ^= Intercept_Draw;
    }

    if (!!(changed & Intercept_Sprite)) {
        if (!!(wanted & Intercept_Sprite)) {
            err = ws.claimVector(OS::Vector_SpriteV, sprite_vector);
        } else {
            err = ws.releaseVector(OS::Vector_SpriteV, sprite_vector);
        }
        if (err != nullptr)
            goto fail;
        active ^= Intercept_Sprite;
    }

    if (!!(changed & Intercept_Byte)) {
        if (!!(wanted & Intercept_Byte)) {
            err = ws.claimVector(OS::Vector_ByteV, byte_vector);
        } else {
            err = ws.releaseVector(OS::Vector_ByteV, byte_vector);
        }
        if (err != nullptr)
            goto fail;
        active ^= Intercept_Byte;
    }

    if (!!(changed & Intercept_Font)) {
        // Undocumented, but Intercept.s sends r2 dependent on intercept_font,
        // and the font manager is more explicit: R2 = 0 or -1 (off or on).
        // Asm doesn't check for error after service call.
        int32_t r2 = !!(wanted & Intercept_Font) ? -1 : 0;
        _swix(OS_ServiceCall, _INR(1,2), Service_Print, r2);
        active ^= Intercept_Font;
    }

    if (!!(changed & Intercept_JPEG)) {
        err = XJPEG_PDriverIntercept(!!(wanted & Intercept_JPEG) ? 1 : 0);
        if (err != nullptr)
            goto fail;
        active ^= Intercept_JPEG;
    }

    m_intercepting = active;
    return nullptr;

fail:
    m_intercepting = active;

    if (!recursed) {
        // Try to clear intercepts if this is not a recursive call. Note err
        // is set, so it is preserved.
        m_shouldIntercept = Intercept_None;

        (void)change(Intercept_None, true);
    }

    return err;
}
