#include "PDriver.h"
#include "InterceptSprite.h"

#include "Colour.h"
#include "Constants.h"
#include "Device.h"
#include "InterceptWrch.h"
#include "Job.h"
#include "MsgCode.h"
#include "Workspace.h"

#include "cmhg.h"

#include "RLib/Constants/Sprite.h"
#include "RLib/OS/Sprite.h"

#include "swis.h"

static const uint32_t SpriteV = 0x1Fu;

bool InterceptSprite::supportedReason(uint32_t reason)
{
    switch (reason) {
    case SpriteReason_SelectSprite:
    case SpriteReason_PutSprite:
    case SpriteReason_PutSpriteUserCoords:
    case SpriteReason_PlotMask:
    case SpriteReason_PlotMaskUserCoords:
    case SpriteReason_PlotMaskScaled:
    case SpriteReason_PutSpriteScaled:
    case SpriteReason_PlotMaskTransformed:
    case SpriteReason_PutSpriteTransformed:
        return true;

    default:
        return false;
    }
}

bool InterceptSprite::badReason(uint32_t reason)
{
    switch (reason) {
    case SpriteReason_ScreenSave:
    case SpriteReason_ScreenLoad:
    case SpriteReason_GetSprite:
    case SpriteReason_GetSpriteUserCoords:
    case SpriteReason_PutSpriteGreyScaled:
        return true;

    default:
        return false;
    }
}

VectorReturn InterceptSprite::finish(MyError err, ScopedEscapeEnable& esc)
{
    MyError escapeErr = esc.disableAndCheck();
    if (escapeErr != nullptr)
        err = escapeErr;

    if (err != nullptr)
        return m_job.makePersistentError(err);

    return VectorReturn::claim();
}

// `intsprite_selectsprite`
// Deal with SelectSprite by allowing it through if R0 >= &100, and telling
// the VDU interception code to select the sprite if R0 < &100.
VectorReturn InterceptSprite::handleSelect(uint32_t reason,
                                           const Sprite::Selector& sprite)
{
    if (reason >= 0x100)
        return VectorReturn::pass();

    MyError err;
    if (m_job.hasPersistentError())
        return m_job.getPersistentError();

    if ((err = wrch_selectsprite(sprite, m_job)) != nullptr)
        return m_job.makePersistentError(err);

    return VectorReturn::claim();
}

// `intsprite_scaledput`
VectorReturn InterceptSprite::handlePut(SpritePlotBlock& block,
                                        Geometry::Point<OS::Unit> point,
                                        const Sprite::ScaleFactor* scale)
{
    if (m_job.hasPersistentError())
        return m_job.getPersistentError();

    MyError err;
    ScopedEscapeEnable esc(m_ws.m_escapeState);
    if ((err = esc.enable()) != nullptr)
        return err;

    if ((err = vdu5_flush(m_job)) != nullptr) {
        debugLog("handlePut: vdu5_flush error %08x '%s'\n",
                 err.errnum(), err.message());
        return finish(err, esc);
    }

    debugLog("handlePut: entry reason=%08x point=[%d,%d] plotAction(raw)=%08x scale=%08x colTrans=%08x\n",
             block.reason, point.x, point.y, uint32_t(block.plotAction),
             uint32_t(uintptr_t(scale)), uint32_t(uintptr_t(block.colTable)));

    // Preserving bits 4,5 of R5 ("use palette","wide table" flags),
    // reduce the GCOL action to the range 0-15, then ignore calls
    // that specify a non-overwrite action.
    // gcolAction &= OS::GCOLAction(63); // Norcroft no like (namespace?).
    block.plotAction = Sprite::PlotAction(block.plotAction & Sprite::PlotAction(63));
    debugLog("handlePut: masked plotAction=%08x low=%u useMask=%u ignoreTrans=%u wide=%u dither=%u colourMap=%u\n",
             uint32_t(block.plotAction),
             uint32_t(block.plotAction & Sprite::PlotAction_GCOLMask),
             (uint32_t(block.plotAction) & uint32_t(Sprite::PlotAction_UseMask)) != 0,
             (uint32_t(block.plotAction) & uint32_t(Sprite::PlotAction_IgnoreColTrans)) != 0,
             (uint32_t(block.plotAction) & uint32_t(Sprite::PlotAction_WideColTrans)) != 0,
             (uint32_t(block.plotAction) & uint32_t(Sprite::PlotAction_Dither)) != 0,
             (uint32_t(block.plotAction) & uint32_t(Sprite::PlotAction_ColourMap)) != 0);
    if ((block.plotAction & Sprite::PlotAction_GCOLMask) != OS::GCOLAction_Src) {
        debugLog("handlePut: skipping non-src plotAction=%08x\n", uint32_t(block.plotAction));
        return finish(nullptr, esc);
    }

    // Offset by graphics origin and current box's origin.
    point += m_job.origin;
    point -= m_job.usersoffset;

    debugLog("handlePut: calling sprite_put reason=%08x point=[%d,%d] plotAction=%08x scale=%08x colTrans=%08x\n",
             block.reason, point.x, point.y, uint32_t(block.plotAction),
             uint32_t(uintptr_t(scale)), uint32_t(uintptr_t(block.colTable)));

    err = sprite_put(block, point, scale, m_job);

    if (err)
        debugLog("handlePut: sprite_put returned error %08x '%s'\n", err.errnum(), err.message());
    else
        debugLog("handlePut: sprite_put returned ok\n");

    return finish(err, esc);
}

VectorReturn InterceptSprite::handleMask(SpritePlotBlock& block,
                                         const Point<OS::Unit> point,
                                         const Sprite::ScaleFactor* scale)
{
    if (m_job.hasPersistentError())
        return m_job.getPersistentError();

    MyError err;
    ScopedEscapeEnable esc(m_ws.m_escapeState);
    if ((err = esc.enable()) != nullptr)
        return err;

    if ((err = vdu5_flush(m_job)) != nullptr)
        return finish(err, esc);

    uint32_t rgb = 0;
    if (!gcol_lookupbg(Disabled_VDU, &rgb, m_job))
        return finish(nullptr, esc);

    if ((err = colour_setrealrgb(rgb, m_job)) != nullptr)
        return finish(err, esc);

    err = sprite_mask(block, point, scale, m_job);

    return finish(err, esc);
}


VectorReturn InterceptSprite::handleTransformed(const Sprite::Selector& sprite,
                                                const OS::Regs& regs)
{
    if (m_job.hasPersistentError())
        return m_job.getPersistentError();

    MyError err;
    ScopedEscapeEnable esc(m_ws.m_escapeState);
    if ((err = esc.enable()) != nullptr)
        return err;

    if ((err = vdu5_flush(m_job)) != nullptr)
        return finish(err, esc);

    Sprite::TransformFlag flags = regs.as<Sprite::TransformFlag>(3);
    bool hasSourceRect   = !!(flags & Sprite::TransformFlag_SourceRect);

    TransformedSprite info(regs[0] & 0xff, sprite, flags,
                           regs.as<Sprite::PlotAction>(5),
                           regs.as<const ColourTrans::Table*>(7));
    info.sourceRect = hasSourceRect ? regs.as<const Sprite::PixelRect*>(4) : nullptr;

    if (info.isMaskPlot()) {
        // SpriteOp with r0=SpriteReason_PlotMaskTransformed
        uint32_t rgb = 0;
        if (!gcol_lookupbg(Disabled_VDU, &rgb, m_job))
            return finish(nullptr, esc);

        if ((err = colour_setrealrgb(rgb, m_job)) != nullptr)
            return finish(err, esc);
    }

    // Adjust inputs by origin.
    if (info.isParallelogram()) {
        const Sprite::Coords* srcCoords(regs.as<Sprite::Coords*>(6));

        for (int i = 0; i < 4; i++) {
            Point<Draw::Unit> point((*srcCoords)[i]);
            point += Draw::fromOSUnit(m_job.origin);
            point -= Draw::fromOSUnit(m_job.usersoffset);
            info.coords.points[i] = point;
        }
    } else {
        info.matrix = *regs.as<const Draw::Matrix*>(6);

        info.matrix.e += m_job.origin.dx - m_job.usersoffset.dx;
        info.matrix.f += m_job.origin.dy - m_job.usersoffset.dy;
    }

    err = sprite_plottransformed(info, m_job);
    return finish(err, esc);
}

// `interceptsprite`
VectorReturn InterceptSprite::intercept(OS::Regs& regs, CoreWS& ws)
{
    if (ws.m_interceptMgr.hasPassthrough(Passthrough_Sprite))
        return VectorReturn::pass();

    if (ws.currentJob() == nullptr)
        return VectorReturn::pass();

    CoreJobWS& job(*ws.currentJob());

    // The assembler treats high reason >= 3, or a low reason outside the table,
    // as table entry 1.  With VectorErrors false, entry 1 is pass-through.
    uint32_t highReason = regs[0] >> 8;
    uint32_t reason = regs[0] & 0xffu;
    if (highReason >= 3)
        return VectorReturn::pass();

    // Maybe the (ARM code's) lookup table is better? Harder to maintain, though.
    if (badReason(reason)) {
        MyError err;

        if (job.hasPersistentError())
            return job.getPersistentError();

        return job.makePersistentError(ws.messages.lookupError(ErrorBlock_PrintCantThisSpriteOp));
    }
// top of print-job SpriteV interceptor, before supportedReason()
debugLog("SpriteV job r0=%08x reason=%u r1=%08x r2=%08x r3=%08x r4=%08x r5=%08x r6=%08x r7=%08x counting=%u\n",
         regs[0], regs[0] & 0xff,
         regs[1], regs[2], regs[3], regs[4],
         regs[5], regs[6], regs[7],
         ws.m_countingPass);

    if (!supportedReason(reason))
        return VectorReturn::pass();

    // Intercept, but do precisely nothing if it is a counting pass.
    if (ws.m_countingPass)
        return VectorReturn::claim();

    InterceptSprite handler(ws, job);

    Sprite::Selector sprite(regs[0],
                            regs.as<const void*>(1),
                            regs.as<const void*>(2));

    switch (reason) {
    case SpriteReason_SelectSprite:
        return handler.handleSelect(regs[0], sprite);

    case SpriteReason_PutSprite:
        // Deal with PutSprite by converting it to PutSpriteScaled.
    {
        SpritePlotBlock block(regs[0], sprite, Sprite::TransformFlag_None,
                              regs.as<Sprite::PlotAction>(5), nullptr);
        return handler.handlePut(block, job.oldpoint, nullptr);
    }

    case SpriteReason_PutSpriteUserCoords:
        // Deal with PutSpriteUserCoords by converting it to PutSpriteScaled
    {
        SpritePlotBlock block(regs[0], sprite, Sprite::TransformFlag_None,
                              regs.as<Sprite::PlotAction>(5), nullptr);
        return handler.handlePut(block,
                                 Point<OS::Unit>(regs.as<int32_t>(3),
                                                 regs.as<int32_t>(4)),
                                 nullptr);
    }

    case SpriteReason_PutSpriteScaled:
    {
        SpritePlotBlock block(regs[0], sprite, Sprite::TransformFlag_None,
                              regs.as<Sprite::PlotAction>(5),
                              regs.as<const ColourTrans::Table*>(7));
        return handler.handlePut(block,
                                 Point<OS::Unit>(regs.as<int32_t>(3),
                                                 regs.as<int32_t>(4)),
                                 regs.as<const Sprite::ScaleFactor*>(6));
    }

    case SpriteReason_PlotMask:
    {
        SpritePlotBlock block(regs[0], sprite, Sprite::TransformFlag_None,
                              Sprite::PlotAction(OS::GCOLAction_Src), nullptr);
        return handler.handleMask(block, ws.currentJob()->oldpoint, nullptr);
    }

    case SpriteReason_PlotMaskUserCoords:
    {
        SpritePlotBlock block(regs[0], sprite, Sprite::TransformFlag_None,
                              Sprite::PlotAction(OS::GCOLAction_Src), nullptr);
        return handler.handleMask(block,
                                  Point<OS::Unit>(regs.as<int32_t>(3),
                                                  regs.as<int32_t>(4)),
                                  nullptr);
    }

    case SpriteReason_PlotMaskScaled:
    {
        SpritePlotBlock block(regs[0], sprite, Sprite::TransformFlag_None,
                              Sprite::PlotAction(OS::GCOLAction_Src), nullptr);
        return handler.handleMask(block,
                                  Point<OS::Unit>(regs.as<int32_t>(3),
                                                  regs.as<int32_t>(4)),
                                  regs.as<const Sprite::ScaleFactor*>(6));
    }

    case SpriteReason_PutSpriteTransformed:
    case SpriteReason_PlotMaskTransformed:
            return handler.handleTransformed(sprite, regs);

    default: break;
    }

    return VectorReturn::pass();
}
