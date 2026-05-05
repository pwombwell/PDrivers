#include "Core/PDriver.h"

#include "GlobalWS.h"
#include "JobWS.h"
#include "Output.h"
#include "PDriverPS.h"
#include "Private.h"
#include "Sprite.h"

#include "Common/ColTrans.h"
#include "Common/Sprite.h"

#include "Core/Constants.h"
#include "Core/InterceptManager.h"
#include "Core/Job.h"
#include "Core/OS.h"
#include "Core/Workspace.h"

#include "RLib/Geometry.h"
#include "RLib/OS.h"
#include "RLib/Constants/Sprite.h"

#include <stddef.h>

// Now setup the special scaling required to ensure correct aspect ratio and size.
// Output:1 <bits/pixel> 1 1 SS <bits/pixel> SM
static MyError outputParallelogramScaling(Sprite::ModeWord spriteMode,
                                          Output& output)
{
    MyError err;

    uint32_t log2bpp;
    if ((err = OS::xreadModeVariable(spriteMode, OS::ModeVar_Log2BPP, log2bpp)) != nullptr)
        return err;

    uint32_t bits_per_pixel = 1u << log2bpp;

    if ((err = output.str("1 ")) != nullptr)
        return err;
    if ((err = output.num((int32_t)bits_per_pixel)) != nullptr)
        return err;
    if ((err = output.str(" 1 1 SS\n")) != nullptr)
        return err;
    if ((err = output.num((int32_t)bits_per_pixel)) != nullptr)
        return err;
    if ((err = output.str(" SM\n")) != nullptr)
        return err;

    return nullptr;
}

static MyError transsprite_outputscaling(Sprite::ModeWord spriteMode,
                                         const Sprite::PixelRect& sourceRect,
                                         JobWS& job)
{
    MyError err;
    Output& output(job.output());

    uint32_t xeigF, yeigF;
    if ((err = OS::xreadModeVariable(spriteMode, OS::ModeVar_XEigFactor, xeigF)) != nullptr)
        return err;
    if ((err = OS::xreadModeVariable(spriteMode, OS::ModeVar_YEigFactor, yeigF)) != nullptr)
        return err;

    uint32_t x_eig = 1u << xeigF;
    uint32_t y_eig = 1u << yeigF;

    uint32_t log2bpp;
    if ((err = OS::xreadModeVariable(spriteMode, OS::ModeVar_Log2BPP, log2bpp)) != nullptr)
        return err;

    uint32_t bits_per_pixel = 1u << log2bpp;

    if ((err = output.numSpace((int32_t)x_eig)) != nullptr)
        return err;
    if ((err = output.numSpace((int32_t)bits_per_pixel)) != nullptr)
        return err;
    if ((err = output.num((int32_t)y_eig)) != nullptr)
        return err;
    if ((err = output.str(" 1 SS\n")) != nullptr)
        return err;
    if ((err = output.num((int32_t)bits_per_pixel)) != nullptr)
        return err;
    if ((err = output.str(" SM\n")) != nullptr)
        return err;

    if (sourceRect.x0 != 0 || sourceRect.y0 != 0) {
        if ((err = output.numSpace(sourceRect.x0)) != nullptr)
            return err;
        if ((err = output.numSpace((int32_t)bits_per_pixel)) != nullptr)
            return err;
        if ((err = output.str("mul ")) != nullptr)
            return err;
        if ((err = output.numSpace(sourceRect.y0)) != nullptr)
            return err;
        if ((err = output.str("T\n")) != nullptr)
            return err;
    }

    return nullptr;
}

static MyError handleParallelogram(Output& output, TransformedSprite& block,
                                   Sprite::Info& spriteInfo,
                                   const Sprite::PixelRect& srcRect,
                                   Sprite::Pixels parWidth,
                                   Sprite::Pixels parHeight,
                                   JobWS& job)
{
    MyError err;
    bool useMask = !!(block.plotAction & Sprite::PlotAction_UseMask);

    for (int i = 0; i < 4; ++i) {
        if ((err = output.numSpace(block.coords[i].x)) != nullptr)
            return err;
        if ((err = output.numSpace(block.coords[i].y)) != nullptr)
            return err;
    }
    // PAR uses the signed source extents.  `srcRect` is only the normalised
    // positive rectangle used for reading image data.
    if ((err = output.numSpace(parWidth)) != nullptr)
        return err;
    if ((err = output.numSpace(parHeight)) != nullptr)
        return err;
    if ((err = output.str("PAR\n")) != nullptr)
        return err;

    ColourMapping mapping(block.colTable);

    if (!block.isMaskPlot()) {
        if ((err = sprite_checkR5bit4(block.sprite, block.plotAction, mapping)) != nullptr)
            return err;

        if (!spriteInfo.hasMask())
            useMask = false;

        if ((block.plotAction & Sprite::PlotAction_GCOLMask) != OS::GCOLAction_Src)
            return nullptr;
    }

    OS::Mode spriteMode(spriteInfo.mode());

#if Medusa
    if (!block.isMaskPlot()) {
        uint32_t log2bpp;
        if ((err = OS::xreadModeVariable(spriteMode, OS::ModeVar_Log2BPP, log2bpp)) != nullptr)
            return err;

        if (log2bpp == 4u || log2bpp == 5u) {
            if (job.info.isColour()) {
                if (log2bpp == 5u) {
                    err = output.str("1 32 1 1 SS  32 SM\n");
                    if (!err) {
                        err = sprite_output32bpp(block.sprite, spriteInfo,
                                                 srcRect, useMask, job);
                    }
                } else {
                    err = output.str("1 16 1 1 SS  16 SM\n");
                    if (!err) {
                        err = sprite_output16bpp(block.sprite, spriteInfo,
                                                 srcRect, useMask, job);
                    }
                }

                return err;
            }

            Sprite::ModeWord modeWord(spriteInfo.mode());
            if ((err = mapping.generated.makeTable(modeWord)) != nullptr)
                return err;

            Sprite::ModeWord greyMode = medusa_same_dpi_g256_mode(modeWord);

            spriteInfo.setMode(greyMode);
        }
    }
#endif

    if ((err = outputParallelogramScaling(spriteMode, output)) != nullptr)
        return err;

    uint32_t log2bpc;
    if ((err = OS::xreadModeVariable(spriteMode, OS::ModeVar_Log2BPC, log2bpc)) != nullptr)
        return err;

    if (block.isMaskPlot()) {
        err = sprite_mask_commonoutput(block.sprite, spriteInfo, srcRect, log2bpc, job);
    } else {
        Sprite::PlotAction action = Sprite::PlotAction(block.plotAction);
        if (useMask)
            action = Sprite::PlotAction(action | Sprite::PlotAction_UseMask);

        err = sprite_commonoutput(block.sprite, spriteInfo, srcRect,
                                  mapping, action, log2bpc, job);
    }

    return err;
}

static MyError handleMatrix(Output& output, Sprite::Info& spriteInfo,
                            const Sprite::PixelRect& srcRect,
                            TransformedSprite& block, JobWS& job)
{
    MyError err;
    bool useMask = !!(block.plotAction & Sprite::PlotAction_UseMask);

    if ((err = output.numSpace(block.matrix.a.raw())) != nullptr)
        return err;
    if ((err = output.numSpace(block.matrix.b.raw())) != nullptr)
        return err;
    if ((err = output.numSpace(block.matrix.c.raw())) != nullptr)
        return err;
    if ((err = output.numSpace(block.matrix.d.raw())) != nullptr)
        return err;
    if ((err = output.numSpace(block.matrix.e.raw())) != nullptr)
        return err;
    if ((err = output.numSpace(block.matrix.f.raw())) != nullptr)
        return err;

    if ((err = output.str("SDM\n")) != nullptr)
        return err;

    ColourMapping mapping(block.colTable);

    if (!block.isMaskPlot()) {
        if ((err = sprite_checkR5bit4(block.sprite, block.plotAction, mapping)) != nullptr)
            return err;

        if (!spriteInfo.hasMask())
            useMask = false;
    }

    if ((block.plotAction & Sprite::PlotAction_GCOLMask) != OS::GCOLAction_Src)
        return nullptr;

    OS::Mode spriteMode(spriteInfo.mode());
#if Medusa

    if (!block.isMaskPlot()) {
        uint32_t log2bpp;
        if ((err = OS::xreadModeVariable(spriteMode, OS::ModeVar_Log2BPP, log2bpp)) != nullptr)
            return err;

        if (log2bpp == 4u || log2bpp == 5u) {
            if (job.info.isColour()) {
                if ((err = transsprite_outputscaling(spriteMode, srcRect, job)) != nullptr)
                    return err;

                if (log2bpp == 5) {
                    err = sprite_output32bpp(block.sprite, spriteInfo,
                                             srcRect, useMask, job);
                } else {
                    err = sprite_output16bpp(block.sprite, spriteInfo,
                                             srcRect, useMask, job);
                }

                return nullptr;
            }

            Sprite::ModeWord modeWord(spriteInfo.mode());
            if ((err = mapping.generated.makeTable(modeWord)) != nullptr)
                return err;

            Sprite::ModeWord greyMode = medusa_same_dpi_g256_mode(modeWord);

            spriteInfo.setMode(greyMode);
        }
    }
#endif

    if ((err = transsprite_outputscaling(spriteMode, srcRect, job)) != nullptr)
        return err;

    uint32_t log2bpc;
    if ((err = OS::xreadModeVariable(spriteMode, OS::ModeVar_Log2BPC, log2bpc)) != nullptr)
        return err;

    if (block.isMaskPlot()) {
        err = sprite_mask_commonoutput(block.sprite, spriteInfo, srcRect, log2bpc, job);
    } else {
        Sprite::PlotAction action = Sprite::PlotAction(block.plotAction);

        if (useMask)
            action = Sprite::PlotAction(action | Sprite::PlotAction_UseMask);

        err = sprite_commonoutput(block.sprite, spriteInfo, srcRect,
                                  mapping, action, log2bpc, job);
    }

    return err;
}

MyError sprite_plottransformed(TransformedSprite& block, CoreJobWS& coreJob)
{
    MyError err;
    JobWS& job(toJobWS(coreJob));

    Output& output(job.output());

    ScopedPassthrough scopedPT(CoreWS::instance().m_interceptMgr, Passthrough_Sprite);

    Sprite::Info spriteInfo;
    if ((err = spriteInfo.get(block.sprite)) != nullptr)
        return err;

    const Sprite::Pixels spriteWidth = spriteInfo.width();
    const Sprite::Pixels spriteHeight = spriteInfo.height();

    Sprite::PixelRect srcRect(0, 0, spriteWidth, spriteHeight);
    Sprite::Pixels parWidth = spriteWidth;
    Sprite::Pixels parHeight = -spriteHeight;   // no source rect: ARM default flip

    if (block.sourceRect) {
        Sprite::PixelRect clipped(*block.sourceRect);

        if (clipped.x0 < 0)
            clipped.x0 = 0;
        if (clipped.x0 >= spriteWidth)
            clipped.x0 = spriteWidth - 1;

        if (clipped.y0 < 0)
            clipped.y0 = 0;
        if (clipped.y0 >= spriteHeight)
            clipped.y0 = spriteHeight - 1;

        if (clipped.x1 < -1)
            clipped.x1 = -1;
        if (clipped.x1 > spriteWidth)
            clipped.x1 = spriteWidth;

        if (clipped.y1 < -1)
            clipped.y1 = -1;
        if (clipped.y1 > spriteHeight)
            clipped.y1 = spriteHeight;

        parWidth = clipped.x1 - clipped.x0;
        if (parWidth < 0)
            clipped.x0 = clipped.x1 + 1;

        parHeight = clipped.y1 - clipped.y0;
        if (parHeight < 0)
            clipped.y0 = clipped.y1 + 1;

        srcRect.x0 = clipped.x0;
        srcRect.y0 = clipped.y0;
        srcRect.x1 = clipped.x0 + (parWidth < 0 ? -parWidth : parWidth);
        srcRect.y1 = clipped.y0 + (parHeight < 0 ? -parHeight : parHeight);
    }

#if PSCoordSpeedUps
    if ((err = ensure_OScoords(output, job)) != nullptr)
        return err;
#endif
    if ((err = output_gsave(job)) != nullptr)
        return err;
    // From here onwards, non-error returns must return via output_grestore().

    if (srcRect.width() == 0 || srcRect.height() == 0)
        return output_grestore(job);

    if (block.isParallelogram())
        err = handleParallelogram(output, block, spriteInfo,
                                  srcRect, parWidth, parHeight, job);
    else
        err = handleMatrix(output, spriteInfo, srcRect, block, job);

    if (err)
        return err;

    return output_grestore(job);

    // colourTrans32K will be auto-freed.
    // passthrough will be auto-reset
}
