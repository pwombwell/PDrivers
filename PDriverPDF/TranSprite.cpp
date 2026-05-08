#include "Core/PDriver.h"

#include "GlobalWS.h"
#include "JobWS.h"
#include "Output.h"
#include "PDriverPDF.h"
#include "Private.h"
#include "Sprite.h"

#include "Common/ColTrans.h"
#include "Common/Sprite.h"

#include "Core/Constants.h"
#include "Core/Job.h"
#include "Core/OS.h"
#include "Core/Workspace.h"

#include "RLib/Constants/Sprite.h"
#include "RLib/Geometry.h"

#include <stddef.h>

struct ColourTransBlock;

static MyError transsprite_output_realspace(int32_t numerator,
                                            int32_t denominator,
                                            Output& output)
{
    MyError err = output.writeReal(numerator, denominator);
    if (err)
        return err;

    return output.str(' ');
}

static MyError transsprite_output_fixed16_16_space(int32_t value,
                                                   Output& output)
{
    return transsprite_output_realspace(value, 65536, output);
}

static MyError transsprite_output_fixed24_8_space(int32_t value,
                                                  Output& output)
{
    return transsprite_output_realspace(value, 256, output);
}

static MyError transsprite_output_integer_cm(int32_t a,
                                                     int32_t b,
                                                     int32_t c,
                                                     int32_t d,
                                                     int32_t e,
                                                     int32_t f,
                                             Output& output)
{
    MyError err = output.numSpace(a);
    if (err) {
        return err;
    }
    if ((err = output.numSpace(b)) != nullptr)
        return err;
    if ((err = output.numSpace(c)) != nullptr)
        return err;
    if ((err = output.numSpace(d)) != nullptr)
        return err;
    if ((err = output.numSpace(e)) != nullptr)
        return err;
    if ((err = output.numSpace(f)) != nullptr)
        return err;
    return output.str("cm\n");
}

static MyError handleParallelogram(Output& output, TransformedSprite& block, bool flippedX, bool flippedY, JobWS& job)
{
    int32_t x0 = block.coords[0].x.raw();
    int32_t y0 = block.coords[0].y.raw();
    int32_t x1 = block.coords[1].x.raw();
    int32_t y1 = block.coords[1].y.raw();
    int32_t x2 = block.coords[2].x.raw();
    int32_t y2 = block.coords[2].y.raw();
    int32_t x3 = block.coords[3].x.raw();
    int32_t y3 = block.coords[3].y.raw();

    // Acorn docs: with a destination parallelogram, swapped x0/x1 and/or y0/y1
    // reflects the image. We model that by swapping the destination corners.
    // Corner mapping (source -> destination):
    //   (x0,y1)->(X0,Y0), (x1,y1)->(X1,Y1), (x1,y0)->(X2,Y2), (x0,y0)->(X3,Y3)
    // Reflect in X: swap left/right corners. Reflect in Y: swap top/bottom corners.
    if (flippedX) {
        int32_t tx, ty;
        tx = x0; ty = y0; x0 = x1; y0 = y1; x1 = tx; y1 = ty; // swap 0 <-> 1
        tx = x3; ty = y3; x3 = x2; y3 = y2; x2 = tx; y2 = ty; // swap 3 <-> 2
    }
    if (flippedY) {
        int32_t tx, ty;
        tx = x0; ty = y0; x0 = x3; y0 = y3; x3 = tx; y3 = ty; // swap 0 <-> 3
        tx = x1; ty = y1; x1 = x2; y1 = y2; x2 = tx; y2 = ty; // swap 1 <-> 2
    }

    int32_t a = x1 - x0;
    int32_t b = y1 - y0;
    int32_t c = x3 - x0;
    int32_t d = y3 - y0;
    int32_t e = x0;
    int32_t f = y0;

    MyError err;
    if ((err = transsprite_output_fixed24_8_space(a, output)) != nullptr)
        return err;
    if ((err = transsprite_output_fixed24_8_space(b, output)) != nullptr)
        return err;
    if ((err = transsprite_output_fixed24_8_space(c, output)) != nullptr)
        return err;
    if ((err = transsprite_output_fixed24_8_space(d, output)) != nullptr)
        return err;
    if ((err = transsprite_output_fixed24_8_space(e, output)) != nullptr)
        return err;
    if ((err = transsprite_output_fixed24_8_space(f, output)) != nullptr)
        return err;
    if ((err = output.str("cm\n")) != nullptr)
        return err;

    return nullptr;
}

static MyError handleMatrix(Output& output, const Sprite::Info& spriteInfo,
                            Sprite::Pixels spriteWidth, Sprite::Pixels spriteHeight,
                            TransformedSprite& block, bool flippedX, bool flippedY,
                            JobWS& job)
{
    MyError err;

    if ((err = transsprite_output_fixed16_16_space(block.matrix.a.raw(), output)) != nullptr)
        return err;
    if ((err = transsprite_output_fixed16_16_space(block.matrix.b.raw(), output)) != nullptr)
        return err;
    if ((err = transsprite_output_fixed16_16_space(block.matrix.c.raw(), output)) != nullptr)
        return err;
    if ((err = transsprite_output_fixed16_16_space(block.matrix.d.raw(), output)) != nullptr)
        return err;
    if ((err = transsprite_output_fixed24_8_space(block.matrix.e.raw(), output)) != nullptr)
        return err;
    if ((err = transsprite_output_fixed24_8_space(block.matrix.f.raw(), output)) != nullptr)
        return err;
    if ((err = output.str("cm\n")) != nullptr)
        return err;

    uint32_t eigX, eigY;
    if ((err = OS::xreadModeVariable(spriteInfo.mode(), OS::ModeVar_XEigFactor, eigX)) != nullptr)
        return err;
    if ((err = OS::xreadModeVariable(spriteInfo.mode(), OS::ModeVar_YEigFactor, eigY)) != nullptr)
        return err;

    int32_t eigScaleX = (int32_t)(1u << eigX);
    int32_t eigScaleY = (int32_t)(1u << eigY);

    if ((err = transsprite_output_integer_cm(eigScaleX, 0, 0, eigScaleY, 0, 0, output)) != nullptr)
        return err;

    if (job.sourceclip_x != 0 || job.sourceclip_y != 0) {
        if ((err = transsprite_output_integer_cm(1, 0, 0, 1,
                                                    job.sourceclip_x,
                                                    job.sourceclip_y, output)) != nullptr)
            return err;
    }

    int32_t a = flippedX ? -spriteWidth : spriteWidth;
    int32_t d = flippedY ? -spriteHeight : spriteHeight;
    int32_t e = flippedX ? spriteWidth : 0;
    int32_t f = flippedY ? spriteHeight : 0;

    return transsprite_output_integer_cm(a, 0, 0, d, e, f, output);
}

MyError sprite_plottransformed(TransformedSprite& block, CoreJobWS& coreJob)
{
    MyError err;
    JobWS& job(toJobWS(coreJob));
    Output& output(job.output());

    Sprite::Info spriteInfo;
    if ((err = spriteInfo.get(block.sprite)) != nullptr)
        return err;

    bool useMask = !!(block.plotAction & Sprite::PlotAction_UseMask);

    ColourMapping mapping(block.colTable);

    ScopedPassthrough passthrough(CoreWS::instance().interceptMgr(), Passthrough_Sprite);

    Sprite::Pixels spriteWidth = spriteInfo.width();
    Sprite::Pixels spriteHeight = spriteInfo.height();

    if (block.sourceRect) {
        // Subrect case - pointed to by sourceRect.
        Sprite::PixelRect srcRect = *block.sourceRect;

        // Clip to sprite size.
        // x0/y0 are clamped to [0 .. w-1] / [0 .. h-1]
        // x1/y1 are clamped to [-1 .. w] / [-1 .. h]
        if (srcRect.x0 < 0) srcRect.x0 = 0;
        if (srcRect.x0 >= spriteWidth) srcRect.x0 = spriteWidth - 1;
        if (srcRect.y0 < 0) srcRect.y0 = 0;
        if (srcRect.y0 >= spriteHeight) srcRect.y0 = spriteHeight - 1;

        if (srcRect.x1 < -1) srcRect.x1 = -1;
        if (srcRect.x1 > spriteWidth) srcRect.x1 = spriteWidth;
        if (srcRect.y1 < -1) srcRect.y1 = -1;
        if (srcRect.y1 > spriteHeight) srcRect.y1 = spriteHeight;

        // Determine actual width/height. If negative, alter start position
        // so that x0,y0 is the bottom-left pixel required.
        spriteWidth = srcRect.x1 - srcRect.x0;
        if (spriteWidth < 0) srcRect.x0 = srcRect.x1 + 1;

        spriteHeight = srcRect.y1 - srcRect.y0;
        if (spriteHeight < 0) srcRect.y0 = srcRect.y1 + 1;

        // srcRect.x0/srcRect.y0 is now the bottom-left point in the source sprite.
        job.sourceclip_x = srcRect.x0;
        job.sourceclip_y = srcRect.y0;
    } else {
        // Transform case (flip vertically)
        spriteHeight = -spriteHeight;
        job.sourceclip_x = 0;
        job.sourceclip_y = 0;
    }

    bool flippedX = false, flippedY = false;

    if (spriteWidth < 0)  { flippedX = true; spriteWidth = -spriteWidth; }
    if (spriteHeight < 0) { flippedY = true; spriteHeight = -spriteHeight; }

    Geometry::Size<uint32_t> clip(spriteWidth, spriteHeight); // potentially clipped size

#if PSCoordSpeedUps
    if ((err = ensure_OScoords(output, job)) != nullptr)
        return err;
#endif
    if ((err = output_gsave(job)) != nullptr)
        return err;

    if (spriteWidth == 0 || spriteHeight == 0)
        return output_grestore(job);

    if (!block.isMaskPlot()) {
        if ((err = sprite_checkR5bit4(block.sprite, block.plotAction, mapping)) != nullptr)
            return err;

        if (!spriteInfo.hasMask())
            useMask = false;

        if ((block.plotAction & Sprite::PlotAction_GCOLMask) != OS::GCOLAction_Src)
            return output_grestore(job);
    }

#if Medusa
    if (!block.isMaskPlot()) {
        uint32_t log2bpp;
        if ((err = OS::xreadModeVariable(spriteInfo.mode(), OS::ModeVar_Log2BPP, log2bpp)) != nullptr)
            return err;

        bool isMono = !job.info.isColour();
        if (isMono && (log2bpp == 4 || log2bpp == 5)) {
            Sprite::ModeWord modeWord(spriteInfo.mode());

            // Check whether the user has asked to use the sprite's palette, rather
            // than the translation table.  Adjust if so.
            if ((err = mapping.makeTable(modeWord)) != nullptr)
                return err;

            Sprite::ModeWord greyMode = medusa_same_dpi_g256_mode(modeWord);

            spriteInfo.setMode(greyMode);
        }
    }
#endif

    if (block.isParallelogram())
        err = handleParallelogram(output, block, flippedX, flippedY, job);
    else
        err = handleMatrix(output, spriteInfo, spriteWidth, spriteHeight,
                           block, flippedX, flippedY, job);
    
    if (err)
        return err;

    if (block.isMaskPlot()) {
        err = sprite_mask_commonoutput(block.sprite, spriteInfo, clip, job);
    } else {
        err = sprite_commonoutput(block.sprite, spriteInfo, clip,
                                  mapping, useMask, 0,
                                  job);
    }
    if (err)
        return err;

    return output_grestore(job);
}
