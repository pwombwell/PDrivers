#include "kernel.h"
#include "Core/PDriver.h"

#include "Colour.h"
#include "JobWS.h"
#include "PDriverDP.h"
#include "Private.h"

#include "Common/ColTrans.h"

#include "Core/Colour.h"
#include "Core/Constants.h"
#include "Core/InterceptManager.h"
#include "Core/OS.h"
#include "Core/Workspace.h"

#include "RLib/Constants/Sprite.h"

#include <swis.h>

static const Sprite::ScaleFactor kUnitScale = {1, 1, 1, 1};

static Sprite::ScaleFactor spriteResolveScaleBlock(const Sprite::ScaleFactor* scale)
{
    if (scale == nullptr || scale == (const Sprite::ScaleFactor*)(intptr_t)-1)
        return kUnitScale;

    return *scale;
}

static void spriteScaleCoords(JobWS& job,
                              int32_t& x,
                              int32_t& y)
{
    x = (XScale(x << 8, job) - (int32_t)job.output().currentOffset().dx.raw()) >> 8;
    y = (YScale(y << 8, job) - (int32_t)job.output().currentOffset().dy.raw()) >> 8;
}


static int32_t spriteDivideOrIdentity(int32_t dividend,
                                      int32_t divisor)
{
    if (divisor == 0)
        return dividend;

    return Divide(dividend, divisor);
}



static void spriteBuildScaledDestination(JobWS& job,
                                         uint32_t widthPixels,
                                         uint32_t heightPixels,
                                         int32_t x,
                                         int32_t y,
                                         const Sprite::ScaleFactor& scale,
                                         Sprite::Coords& destCoords)
{
    uint32_t eigMask = (1u << job.screenVars.currxeig) - 1u;

    int32_t right = x + spriteDivideOrIdentity((int32_t)widthPixels *
                                               (int32_t)scale.multiplierX *
                                               (((int32_t)scale.divisorX & (int32_t)eigMask) == 0 ?
                                                   1 :
                                                   (1 << job.screenVars.currxeig)),
                                               (((int32_t)scale.divisorX & (int32_t)eigMask) == 0) ?
                                                   ((int32_t)scale.divisorX >> job.screenVars.currxeig) :
                                                   (int32_t)scale.divisorX);

    eigMask = (1u << job.screenVars.curryeig) - 1u;
    int32_t top = y + spriteDivideOrIdentity((int32_t)heightPixels *
                                             (int32_t)scale.multiplierY *
                                             (((int32_t)scale.divisorY & (int32_t)eigMask) == 0 ?
                                                 1 :
                                                 (1 << job.screenVars.curryeig)),
                                             (((int32_t)scale.divisorY & (int32_t)eigMask) == 0) ?
                                                 ((int32_t)scale.divisorY >> job.screenVars.curryeig) :
                                                 (int32_t)scale.divisorY);

    spriteScaleCoords(job, right, top);
    right >>= bufferpix_l2size;
    Draw::Unit unitRight = Draw::Unit::fromRaw(right << bufferpix_l2size);

    top >>= bufferpix_l2size;
    Draw::Unit unitTop = Draw::Unit::fromRaw(top << bufferpix_l2size);


    spriteScaleCoords(job, x, y);
    x >>= bufferpix_l2size;
    Draw::Unit unitX = Draw::Unit::fromRaw(x << bufferpix_l2size);

    y >>= bufferpix_l2size;
    Draw::Unit unitY = Draw::Unit::fromRaw(y << bufferpix_l2size);

    destCoords[0] = Point<Draw::Unit>(unitX,     unitTop);
    destCoords[1] = Point<Draw::Unit>(unitRight, unitTop);
    destCoords[2] = Point<Draw::Unit>(unitRight, unitY);
    destCoords[3] = Point<Draw::Unit>(unitX,     unitY);
}

static MyError spriteNormaliseSelector(const Sprite::Selector& sprite,
                                       uint32_t reason,
                                       uint32_t& normalisedReason,
                                       Sprite::Area*& areaOut,
                                       const void*& nameOrPtrOut)
{
    normalisedReason = reason;
    areaOut = sprite.area().ptr();
    if (sprite.areaValue() == Sprite::AreaValue_UserPtr)
        nameOrPtrOut = sprite.id().header();
    else
        nameOrPtrOut = sprite.id().name();

    if (sprite.areaValue() != Sprite::AreaValue_System)
        return nullptr;

    void* systemArea = nullptr;
    MyError err = XOS_ReadDynamicArea(3, &systemArea);
    if (err != nullptr)
        return err;

    areaOut = (Sprite::Area*)systemArea;
    nameOrPtrOut = sprite.id().name();
    normalisedReason = (reason & 0xffu) | Sprite::AreaValue_UserName;
    return nullptr;
}

static MyError spritePrepareTranslationTable(const Sprite::Info& spriteInfo,
                                             Sprite::PlotAction plotAction,
                                             const ColourTrans::Table* requestedTable,
                                             const ColourTrans::Table*& tableOut,
                                             ColourTrans::Table32K& spriteTrans,
                                             JobWS& job)
{
    MyError err = nullptr;

#if Medusa
    uint32_t log2bpp = 0;
    err = OS::xreadModeVariable(spriteInfo.mode(),
                                OS::ModeVar_Log2BPP,
                                log2bpp);
    if (err != nullptr)
        return err;

    // Only fake colourtrans tables for 16bpp and 32bpp.
    if ((log2bpp == 4u || log2bpp == 5u) && job.config().stripType() <= 2u) {
        if ((err = jpeg_ctrans_handle(job.colourTrans32K, job)) != nullptr)
            return err;

        tableOut = &job.colourTrans32K;
        return nullptr;
    }
#else
    (void)spriteInfo;
    (void)job;
#endif

    if (requestedTable != nullptr &&
        requestedTable != (const ColourTrans::Table*)(intptr_t)-1) {
        tableOut = requestedTable;
        return nullptr;
    }

    err = _swix(ColourTrans_SelectTable,
                _INR(0, 5),
                -1,
                -1,
                -1,
                -1,
                &spriteTrans,
                plotAction);
    if (err != nullptr)
        return err;

    tableOut = &spriteTrans;
    return nullptr;
}

static MyError spritePassTransformed(const TransformedSprite& block, JobWS& job)
{
    uint32_t normalisedReason = 0;
    Sprite::Area* area = nullptr;
    const void* nameOrPtr = nullptr;
    MyError err = spriteNormaliseSelector(block.sprite,
                                          block.reason,
                                          normalisedReason,
                                          area,
                                          nameOrPtr);
    if (err != nullptr)
        return err;

    InterceptManager& interceptMgr(CoreWS::instance().interceptMgr());

    ScopedPassthrough scopedPT(interceptMgr, Passthrough_Sprite);

    const void* transform = block.isParallelogram() ?
                            (const void*)&block.coords :
                            (const void*)&block.matrix;

    return _swix(OS_SpriteOp, _INR(0, 7),
                 normalisedReason, area, nameOrPtr, block.flags,
                 block.sourceRect, block.plotAction, transform, block.colTable);
}

// ew.
static void adjustMatrix(JobWS& job, Draw::Matrix& matrix)
{
    matrix.a.fromRaw(XScale(matrix.a.raw(), job));
    matrix.b.fromRaw(YScale(matrix.b.raw(), job));
    matrix.c.fromRaw(XScale(matrix.c.raw(), job));
    matrix.d.fromRaw(YScale(matrix.d.raw(), job));

    matrix.e.fromRaw(XScale(matrix.e.raw(), job));
    matrix.f.fromRaw(YScale(matrix.f.raw(), job));

    matrix.e -= job.output().currentOffset().dx;
    matrix.f -= job.output().currentOffset().dy;
}

static void adjustCoords(JobWS& job, Sprite::Coords& coords)
{
    for (int i = 0; i < 4; ++i) {
        Scale(coords[i], job);

        coords[i] -= job.output().currentOffset();
    }
}

// `sprite_put_altentry`
static MyError spritePutCommon(const SpritePlotBlock& block,
                               uint32_t transformedReason,
                               const Point<OS::Unit>& point,
                               const Sprite::ScaleFactor* scale,
                               JobWS& job)
{
    if ((job.disabled & Disabled_NullClip) != 0)
        return nullptr;

    Sprite::Info spriteInfo;
    MyError err = spriteInfo.get(block.sprite);
    if (err != nullptr)
        return err;

    const ColourTrans::Table* effectiveTable = block.colTable;
    err = spritePrepareTranslationTable(spriteInfo,
                                        block.plotAction,
                                        block.colTable,
                                        effectiveTable,
                                        job.spriteTrans,
                                        job);
    if (err != nullptr)
        return err;

    Sprite::Coords parallelogram;
    spriteBuildScaledDestination(job,
                                 spriteInfo.width(),
                                 spriteInfo.height(),
                                 point.x.raw(),
                                 point.y.raw(),
                                 spriteResolveScaleBlock(scale),
                                 parallelogram);

    TransformedSprite transformed(transformedReason, block.sprite,
                                  Sprite::TransformFlag_Parallelogram,
                                  block.plotAction, effectiveTable);
    transformed.coords = parallelogram;

    return spritePassTransformed(transformed, job);
}

MyError sprite_plottransformed(TransformedSprite& block, CoreJobWS& coreJob)
{
    JobWS& job(toJobWS(coreJob));

    if (!!(job.disabled & Disabled_NullClip))
        return nullptr;

    Sprite::Info spriteInfo;
    MyError err = spriteInfo.get(block.sprite);
    if (err != nullptr)
        return err;

    const ColourTrans::Table* effectiveTable = block.colTable;

    err = spritePrepareTranslationTable(spriteInfo,
                                        block.plotAction,
                                        block.colTable,
                                        effectiveTable,
                                        job.spriteTrans,
                                        job);
    if (err != nullptr)
        return err;

    if (block.isParallelogram())
        adjustCoords(job, block.coords);
    else
        adjustMatrix(job, block.matrix);

    return spritePassTransformed(block, job);
}

MyError sprite_put(SpritePlotBlock& block,
                   const Point<OS::Unit>& point,
                   const Sprite::ScaleFactor* scale,
                   CoreJobWS& coreJob)
{
    return spritePutCommon(block,
                           (block.reason & ~0xffu) |
                               SpriteReason_PutSpriteTransformed,
                           point, scale, toJobWS(coreJob));
}

MyError sprite_mask(SpritePlotBlock& block,
                    const Point<OS::Unit>& point,
                    const Sprite::ScaleFactor* scale,
                    CoreJobWS& coreJob)
{
    JobWS& job(toJobWS(coreJob));

    uint32_t rgb;
    if (!gcol_lookupbg(Disabled_VDU, rgb, job))
        return nullptr;

    MyError err = setBackgroundColour(rgb, job);
    if (err != nullptr)
        return err;

    block.plotAction = Sprite::PlotAction(OS::GCOLAction_Src);

    return spritePutCommon(block,
                           (block.reason & ~0xffu) |
                                SpriteReason_PlotMaskTransformed,
                           point, scale, toJobWS(coreJob));
}
