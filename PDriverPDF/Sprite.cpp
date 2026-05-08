#include "Core/PDriver.h"

#include "Sprite.h"
#include "Colour.h"
#include "GlobalWS.h"
#include "ImageBuilder.h"
#include "JobWS.h"
#include "Output.h"
#include "PDriverPDF.h"
#include "Private.h"

#include "Common/ColTrans.h"
#include "Common/Sprite.h"

#include "Core/Colour.h"
#include "Core/InterceptColTrans.h"
#include "Core/Constants.h"
#include "Core/Job.h"
#include "Core/OS.h"
#include "Core/Workspace.h"

#include <stddef.h>


static MyError sprite_translateby(int32_t dx, int32_t dy, Output& output)
{
    if ((dx | dy) == 0)
        return nullptr;

    MyError err = output.numSpace(1);
    if (err)
        return err;

    if ((err = output.numSpace(0)) != nullptr)
        return err;
    if ((err = output.numSpace(0)) != nullptr)
        return err;
    if ((err = output.numSpace(1)) != nullptr)
        return err;
    if ((err = output.numSpace(dx)) != nullptr)
        return err;
    if ((err = output.numSpace(dy)) != nullptr)
        return err;
    return output.str("cm\n");
}

static MyError sprite_do_image(PDF::ImageRecord& imageRecord, Output& output)
{
    MyError err;
    if ((err = output.str("/Im")) != nullptr)
        return err;
    if ((err = output.num(imageRecord.objectId().value())) != nullptr)
        return err;
    return output.str(" Do\n");
}

static int32_t sprite_scaled_extent(Sprite::Pixels pixels,
                                    int32_t multiplier,
                                    int32_t divisor,
                                    uint32_t eigFactor)
{
    int64_t value = (int64_t)pixels * multiplier * (1 << eigFactor);
    if (divisor != 0)
        value /= divisor;
    return (int32_t)value;
}

MyError sprite_mask_commonoutput(const Sprite::Selector& spriteIn,
                                 const Sprite::Info& spriteInfo,
                                 const Geometry::Size<uint32_t>& clip,
                                 JobWS& job)
{
    Output& output(job.output());

    if (!spriteInfo.hasMask())
        return output.str("0 0 1 1 re\nf\n");

    MyError err;
    Sprite::ResolvedSelector sprite;
    if ((err = spriteIn.resolve(sprite)) != nullptr)
        return err;

    const Sprite::HeaderView hdr = sprite.header();
    if (hdr.maskPtr() == nullptr)
        return output.str("0 0 1 1 re\nf\n");

    Sprite::PixelRect sourceRect(job.sourceclip_x,
                                 job.sourceclip_y,
                                 job.sourceclip_x + (Sprite::Pixel)clip.width,
                                 job.sourceclip_y + (Sprite::Pixel)clip.height);
    ColourMapping maskMapping(nullptr);
    PDF::ImageBuilder builder(sprite, spriteInfo, sourceRect,
                              maskMapping, false, job);
    if ((err = builder.init()) != nullptr)
        return err;

    PDF::ImageRecord* imageRecord;
    if ((err = builder.registerMaskImage(imageRecord)) != nullptr)
        return err;

    return sprite_do_image(*imageRecord, output);
}


MyError sprite_commonoutput(const Sprite::Selector& spriteIn,
                            const Sprite::Info& spriteInfo,
                            const Geometry::Size<uint32_t>& clip,
                            const ColourMapping& mapping,
                            bool useMask,
                            uint32_t log2bpc,
                            JobWS& job)
{
    MyError err;
    (void)log2bpc;

    Sprite::ResolvedSelector sprite;
    if ((err = spriteIn.resolve(sprite)) != nullptr)
        return err;

    Sprite::PixelRect sourceRect(job.sourceclip_x,
                                 job.sourceclip_y,
                                 job.sourceclip_x + (Sprite::Pixel)clip.width,
                                 job.sourceclip_y + (Sprite::Pixel)clip.height);
    PDF::ImageBuilder builder(sprite, spriteInfo, sourceRect,
                              mapping, useMask, job);
    if ((err = builder.init()) != nullptr)
        return err;

    PDF::ImageRecord* imageRecord;
    if ((err = builder.registerColourImage(imageRecord)) != nullptr)
        return err;

    return sprite_do_image(*imageRecord, job.output());
}

MyError sprite_put(const Sprite::Selector& sprite,
                   const Point<OS::Unit>& point,
                   Sprite::PlotAction plotAction,
                   const Sprite::ScaleFactor *scale,
                   const ColourTrans::Table* table,
                   JobWS& job)
{
    MyError err;
    CoreWS& ws = CoreWS::instance();

    Output& output(job.output());
    bool useMask = !!(plotAction & Sprite::PlotAction_UseMask);
    const Sprite::ScaleFactor *scale_factors = nullptr;
    Sprite::Info spriteInfo;

    ColourMapping mapping(table);

    job.sourceclip_x = 0;
    job.sourceclip_y = 0;

    ScopedPassthrough passthrough(ws.interceptMgr(), Passthrough_Sprite);
    (void)passthrough;

#if PSCoordSpeedUps
    if ((err = ensure_OScoords(output, job)) != nullptr)
        goto sprite_put_cleanup;
#endif

    if ((err = output_gsave(job)) != nullptr)
        goto sprite_put_cleanup;

    if ((err = sprite_translateby(point.x.raw(), point.y.raw(), output)) != nullptr)
        goto sprite_put_cleanup;

    scale_factors = scale;

    // Check whether the user has asked to use the sprite's palette, rather
    // than the translation table.  Adjust if so.
    // https://www.riscosopen.org/wiki/documentation/show/OS_SpriteOp%20Scaled%2FTransformed%20Plot%20Flags
    if ((err = sprite_checkR5bit4(sprite, plotAction, mapping)) != nullptr)
        goto sprite_put_cleanup;

    if ((err = spriteInfo.get(sprite)) != nullptr)
        goto sprite_put_cleanup;

    if (spriteInfo.width() == 0 || spriteInfo.height() == 0) {
        err = output_grestore(job);
        goto sprite_put_cleanup;
    }

    if (!spriteInfo.hasMask())
        useMask = false;

    if ((plotAction & Sprite::PlotAction_GCOLMask) != OS::GCOLAction_Src) {
        err = output_grestore(job);
        goto sprite_put_cleanup;
    }

#if Medusa
    uint32_t log2bpp;
    if ((err = OS::xreadModeVariable(spriteInfo.mode(), OS::ModeVar_Log2BPP, log2bpp)) != nullptr)
        goto sprite_put_cleanup;

    if (log2bpp == 4 || log2bpp == 5) {
        // If output is destined for a monochrome printer, fudge the sprite mode.
        if (!job.info.isColour()) {
            Sprite::ModeWord modeWord(spriteInfo.mode());
    
            if ((err = mapping.makeTable(modeWord)) != nullptr)
                goto sprite_put_cleanup;

            // Given the sprite is 16bpp or 32bpp, the spriteMode must be a 
            // RO3.5 or RO5 format ModeWord.
    
            // Massage the sprite mode word into a new RO3.5 ModeWord that
            // has a similar DPI, but which is 8bpp for greyscaling purposes.
            Sprite::ModeWord greyMode = medusa_same_dpi_g256_mode(modeWord);

            spriteInfo.setMode(greyMode);
        }
    }
#endif

    {
        int32_t scale_x = 1;
        int32_t scale_y = 1;
        int32_t scale_x_div = 1;
        int32_t scale_y_div = 1;
        if (scale_factors != nullptr &&
            scale_factors != (const Sprite::ScaleFactor *)-1)
        {
            scale_x = (int32_t)scale_factors->multiplierX;
            scale_y = (int32_t)scale_factors->multiplierY;
            scale_x_div = (int32_t)scale_factors->divisorX;
            scale_y_div = (int32_t)scale_factors->divisorY;
        }

        int32_t draw_w = sprite_scaled_extent((Sprite::Pixels)spriteInfo.width(),
                                              scale_x,
                                              scale_x_div,
                                              job.screenVars.currxeig);
        int32_t draw_h = sprite_scaled_extent((Sprite::Pixels)spriteInfo.height(),
                                              scale_y,
                                              scale_y_div,
                                              job.screenVars.curryeig);

        if ((err = output.numSpace(draw_w)) != nullptr)
            goto sprite_put_cleanup;
        if ((err = output.numSpace(0)) != nullptr)
            goto sprite_put_cleanup;
        if ((err = output.numSpace(0)) != nullptr)
            goto sprite_put_cleanup;
        if ((err = output.numSpace(-draw_h)) != nullptr)
            goto sprite_put_cleanup;
        if ((err = output.numSpace(0)) != nullptr)
            goto sprite_put_cleanup;
        if ((err = output.numSpace(draw_h)) != nullptr)
            goto sprite_put_cleanup;
        if ((err = output.str("cm\n")) != nullptr)
            goto sprite_put_cleanup;
    }

    if ((err = sprite_commonoutput(sprite, spriteInfo, spriteInfo.size(),
                                   mapping, useMask, 0,
                                   job)) != nullptr)
    {
        goto sprite_put_cleanup;
    }

    err = output_grestore(job);

sprite_put_cleanup:
#if Medusa
    // colourTrans32K should be auto-destroyed.
#endif

    return err;
}

static MyError sprite_mask(const Sprite::Selector& sprite,
                           const Point<OS::Unit>& point,
                           Sprite::PlotAction gcolAction,
                           const Sprite::ScaleFactor *scale,
                           const void *trans_table,
                           JobWS& job)
{
    MyError err;
    CoreWS& ws = CoreWS::instance();

    Output& output(job.output());

    Sprite::Info spriteInfo;

    job.sourceclip_x = 0;
    job.sourceclip_y = 0;

    ScopedPassthrough passthrough(ws.interceptMgr(), Passthrough_Sprite);
    (void)passthrough;
    (void)gcolAction;
    (void)trans_table;

#if PSCoordSpeedUps
    if ((err = ensure_OScoords(output, job)) != nullptr)
        goto sprite_mask_cleanup;
    if ((err = colour_ensure(job)) != nullptr)
        goto sprite_mask_cleanup;
#endif
    if ((err = output_gsave(job)) != nullptr)
        goto sprite_mask_cleanup;

    if ((err = sprite_translateby(point.x.raw(), point.y.raw(), output)) != nullptr)
        goto sprite_mask_cleanup;

    if ((err = spriteInfo.get(sprite)) != nullptr)
        goto sprite_mask_cleanup;

    if (spriteInfo.width() == 0 || spriteInfo.height() == 0) {
        err = output_grestore(job);
        goto sprite_mask_cleanup;
    }

    {
        int32_t scale_x = 1;
        int32_t scale_y = 1;
        int32_t scale_x_div = 1;
        int32_t scale_y_div = 1;
        if (scale != nullptr &&
            scale != (const Sprite::ScaleFactor *)-1)
        {
            scale_x = (int32_t)scale->multiplierX;
            scale_y = (int32_t)scale->multiplierY;
            scale_x_div = (int32_t)scale->divisorX;
            scale_y_div = (int32_t)scale->divisorY;
        }

        int32_t draw_w = sprite_scaled_extent((Sprite::Pixels)spriteInfo.width(),
                                              scale_x,
                                              scale_x_div,
                                              job.screenVars.currxeig);
        int32_t draw_h = sprite_scaled_extent((Sprite::Pixels)spriteInfo.height(),
                                              scale_y,
                                              scale_y_div,
                                              job.screenVars.curryeig);

        if ((err = output.numSpace(draw_w)) != nullptr)
            goto sprite_mask_cleanup;
        if ((err = output.numSpace(0)) != nullptr)
            goto sprite_mask_cleanup;
        if ((err = output.numSpace(0)) != nullptr)
            goto sprite_mask_cleanup;
        if ((err = output.numSpace(-draw_h)) != nullptr)
            goto sprite_mask_cleanup;
        if ((err = output.numSpace(0)) != nullptr)
            goto sprite_mask_cleanup;
        if ((err = output.numSpace(draw_h)) != nullptr)
            goto sprite_mask_cleanup;
        if ((err = output.str("cm\n")) != nullptr)
            goto sprite_mask_cleanup;
    }

    if ((err = sprite_mask_commonoutput(sprite, spriteInfo, spriteInfo.size(), job)) != nullptr)
        goto sprite_mask_cleanup;

    err = output_grestore(job);

sprite_mask_cleanup:
    return err;
}

MyError sprite_put(SpritePlotBlock& block,
                   const Point<OS::Unit>& point,
                   const Sprite::ScaleFactor* scale,
                   CoreJobWS& coreJob)
{
    return sprite_put(block.sprite, point, block.plotAction,
                      scale, block.colTable, toJobWS(coreJob));
}

MyError sprite_mask(SpritePlotBlock& block,
                    const Point<OS::Unit>& point,
                    const Sprite::ScaleFactor* scale,
                    CoreJobWS& coreJob)
{
    return sprite_mask(block.sprite, point,
                       Sprite::PlotAction(OS::GCOLAction_Src),
                       scale, nullptr, toJobWS(coreJob));
}
