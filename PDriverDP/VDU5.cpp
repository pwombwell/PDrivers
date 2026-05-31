#include "Core/PDriver.h"
#include "Core/VDU5.h"

#include "JobWS.h"
#include "Private.h"

#include "Core/Constants.h"
#include "Core/InterceptManager.h"
#include "Core/Job.h"
#include "Core/Workspace.h"

static const uint8_t set_text_width_string[] = {4, 23, 17, 7, 6};

MyError vdu5_char(uint8_t c, const Point<OS::Unit>& p, CoreJobWS& coreJob)
{
    JobWS& job(toJobWS(coreJob));

    if (job.disabled & Disabled_NullClip)
        return nullptr;

    ScopedPassthrough scopedPT(CoreWS::instance().m_interceptMgr, Passthrough_Sprite);

    MyError err;
    
    if ((err = vdu_counted_string(set_text_width_string)) != nullptr)
        return err;

    // -> our OS units
    int32_t size_x = XScale(job.vdu5().charSize().width.raw(), job);
    // -> our pixels
    size_x >>= bufferpix_l2size;
    if ((err = vdu_pair((uint32_t)size_x)) != nullptr)
        return err;

    int32_t size_y = YScale(job.vdu5().charSize().height.raw(), job);
    size_y >>= bufferpix_l2size;
    if ((err = vdu_pair((uint32_t)size_y)) != nullptr)
        return err;

    if ((err = vdu_pair(0)) != nullptr)
        return err;

    int32_t x_scaled = XScale(Draw::fromOSUnit(p.x).raw(), job);
    x_scaled -= (int32_t)job.output().currentOffset().dx.raw();
    int32_t x_pos = x_scaled >> 8;

    int32_t y_scaled = YScale(Draw::fromOSUnit(p.y).raw(), job);
    y_scaled -= (int32_t)job.output().currentOffset().dy.raw();
    int32_t y_pos = y_scaled >> 8;
    y_pos -= (1 << bufferpix_l2size);

    if ((err = vdu_pair(25 + (4 << 8))) != nullptr)
        return err;
    if ((err = vdu_pair((uint32_t)x_pos)) != nullptr)
        return err;
    if ((err = vdu_pair((uint32_t)y_pos)) != nullptr)
        return err;

    return vdu_char(c);
}

MyError vdu5_delete(const Point<OS::Unit>& p, CoreJobWS& coreJob)
{
    MyError err = vdu5_char((uint8_t)' ', p, coreJob);
    if (err)
        return err;

    ScopedPassthrough scopedPT(CoreWS::instance().m_interceptMgr, Passthrough_Sprite);

    // FIXME: wrong.
    return vdu_char(127);
}

MyError vdu5_flush(CoreJobWS&)
{
    return nullptr;
}

void vdu5_changed(CoreJobWS&, uint32_t) {
}
