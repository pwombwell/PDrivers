#ifndef CORE_SPRITE_H
#define CORE_SPRITE_H

#include "InterceptManager.h"

#include "RLib/OS.h"
#include "RLib/Geometry.h"
#include "RLib/OS/Vector.h"
#include "RLibX/Sprite.h"

#include <stdint.h>

class CoreWS;
class CoreJobWS;
class ScopedEscapeEnable;
struct SpritePlotBlock;

using namespace riscos::Geometry;

class InterceptSprite : public InterceptorBase
{
public:
    static VectorReturn intercept(OS::Regs& regs, CoreWS& ws);

private:
    InterceptSprite(CoreWS& ws, CoreJobWS& job)
        : m_ws(ws), m_job(job)
    { }

    MyError badOperation();
    VectorReturn finish(MyError err, ScopedEscapeEnable& esc);

    VectorReturn handleSelect(uint32_t reason,
                              const Sprite::Selector& sprite);
    VectorReturn handlePut(SpritePlotBlock& block,
                           Geometry::Point<OS::Unit> point,
                           const Sprite::ScaleFactor* scale);

    VectorReturn handleMask(SpritePlotBlock& block,
                            Geometry::Point<OS::Unit> point,
                            const Sprite::ScaleFactor* scale);

    VectorReturn handleTransformed(const Sprite::Selector& sprite,
                                   const OS::Regs& regs);

    static bool supportedReason(uint32_t reason);
    static bool badReason(uint32_t reason);

private:
    CoreWS& m_ws;
    CoreJobWS& m_job;
};

#endif
