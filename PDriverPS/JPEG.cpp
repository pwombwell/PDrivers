#include "Core/PDriver.h"

#include "GlobalWS.h"
#include "JobWS.h"
#include "Output.h"
#include "PDriverPS.h"
#include "Private.h"

#include "Core/Constants.h"
#include "Core/Device.h"
#include "Core/Job.h"
#include "Core/OS.h"
#include "Core/Workspace.h"

#include "RLibX/Sprite.h"

#include <stddef.h>

enum PS_JPEG_Flags {
    PS_JPEG_Flags_None            = 0,
    PS_JPEG_Flags_Image_Grey      = 1u << 0, // from source image.
    PS_JPEG_Flags_Printer_Grey    = 1u << 1  // from pdriver.
};
DEFINE_ENUM_BITWISE_OPERATORS(PS_JPEG_Flags);

static inline Sprite::Area **jpeg_buffer_sprite_slot(PDriverWS& psWS)
{
    return (Sprite::Area **)&psWS.globaltempws.jpegPlotting.jpeg_buffer_sprite;
}

static MyError jpeg_translate_by(int32_t x,
                                 int32_t y,
                                 Output& output)
{
    if ((x | y) == 0) {
        return nullptr;
    }
    MyError err = output_coordpair(x, y, output);
    if (err) {
        return err;
    }
    return output.str("T\n");
}

static MyError jpeg_scaling(const Sprite::ScaleFactor *scale,
                            int32_t height,
                            const CoreJobWS& coreJob,
                            Output& output)
{
    int32_t scale_x = 1;
    int32_t scale_y = 1;
    int32_t scale_x_den = 1;
    int32_t scale_y_den = 1;

    if (scale) {
        scale_x = (int32_t)scale->multiplierX;
        scale_y = (int32_t)scale->multiplierY;
        scale_x_den = (int32_t)scale->divisorX;
        scale_y_den = (int32_t)scale->divisorY;
    }

    scale_x <<= coreJob.screenVars.currxeig;
    scale_y <<= coreJob.screenVars.curryeig;

    MyError err;
    if ((err = output_coordpair(scale_x, scale_x_den, output)) != nullptr)
        return err;
    if ((err = output_coordpair(scale_y, scale_y_den, output)) != nullptr)
        return err;
    if ((err = output.str("SS\n")) != nullptr)
        return err;
    if ((err = output.num(height)) != nullptr)
        return err;

    return output.str("JM\n");
}

static MyError jpeg_output_to_buffersprite(const JPEG::Info& info, PDriverWS& psWS)
{
    MyError err;

    uint32_t bytes = (info.width() << 3) + 64;
    void* block;
    if ((err = rma_claim(bytes, block)) != nullptr)
        return err;

    Sprite::Area* spriteArea = (Sprite::Area*)block;
    psWS.globaltempws.jpegPlotting.jpeg_buffer_sprite = spriteArea;

    spriteArea->view().initialiseCheekily(bytes);

    Sprite::Selector selector(spriteArea, "jp");
    static const Sprite::ModeWord jpeg_deep_sp32 = 0x301680B5;

    if ((err = Sprite::create(selector, false, info.width(), 2, jpeg_deep_sp32)) != nullptr)
        return err;

    Sprite::VDUContext& save(psWS.globaltempws.jpegPlotting.jpeg_buffersp_save);
    if ((err = Sprite::switchOutputToSprite(selector, nullptr, save)) != nullptr)
        return err;

    return nullptr;
}

static void jpeg_release_buffersprite(PDriverWS& psWS)
{
    Sprite::Area* sprite_area = *jpeg_buffer_sprite_slot(psWS);
    if (sprite_area == nullptr)
        return;

    const Sprite::VDUContext& save(psWS.globaltempws.jpegPlotting.jpeg_buffersp_save);
    (void)save.restore();

    (void)rma_free(sprite_area);
}

static MyError jpeg_psimage_output_from_sprite(const uint8_t *jpeg, uint32_t length,
                                               const JPEG::Info& info, PS_JPEG_Flags psFlags,
                                               JobWS& job)
{
    MyError err;
    
    if ((err = XJPEG_PDriverIntercept(0)) != nullptr)
        return err;

    Output& output(job.output());

    PDriverWS& psWS(PDriverWS::instance());

    Sprite::AreaView spriteArea = psWS.globaltempws.jpegPlotting.jpeg_buffer_sprite;
    Sprite::HeaderView firstSprite = spriteArea.firstSprite();

    uint8_t* image_base = firstSprite.imagePtr();
    image_base += info.width() * 4; // -> 2nd row (ie bottom row) of sprite image
    uint32_t *row = (uint32_t *)image_base;

    int32_t y = 0; // JPEG y plot coord starts at 0
    uint32_t rows_left = info.height();

    while (rows_left > 0) {
        if ((err = XJPEG_PlotScaled(jpeg, 0, y, nullptr, length, 0)) != nullptr)
            return err;

        uint32_t index = 0;
        while (index < (info.width() << 2)) {
            uint32_t pixel = row[index >> 2];

            if (psFlags == PS_JPEG_Flags_None) {
                if ((err = output.hex((uint8_t)(pixel & 0xFF))) != nullptr)
                    return err;
                if ((err = output.hex((uint8_t)((pixel >> 8) & 0xFF))) != nullptr)
                    return err;
                if ((err = output.hex((uint8_t)((pixel >> 16) & 0xFF))) != nullptr)
                    return err;
            } else if (!!(psFlags & PS_JPEG_Flags_Printer_Grey)) {
                uint32_t red = pixel & 0xFF;
                uint32_t green = (pixel >> 8) & 0xFF;
                uint32_t blue = (pixel >> 16) & 0xFF;
                uint32_t grey = red * 5 + green * 9 + blue * 2;
                grey >>= 4;
                if ((err = output.hex((uint8_t)grey)) != nullptr)
                    return err;
            } else {
                if ((err = output.hex((uint8_t)((pixel >> 8) & 0xFF))) != nullptr)
                    return err;
            }

            index += 4;
            if ((index & (31u * 4u)) != 0 && index != (info.width() << 2)) {
                continue;
            }
            if ((err = output.str('\n')) != nullptr)
                return err;
            if (index != (info.width() << 2)) {
                continue;
            }
        }

        y -= 2;
        rows_left -= 1;
    }

    return nullptr;
}

static MyError outputAscii85(const uint8_t* jpeg, uint32_t length,
                             const JPEG::Info& info, PS_JPEG_Flags psFlags,
                             Output& output)
{
    MyError err;

    if (!!(psFlags & PS_JPEG_Flags_Image_Grey)) {
        if ((err = output.str("J8\n")) != nullptr)
            return err;
    } else if (psFlags == PS_JPEG_Flags_None) {
        if ((err = output.str("J32\n")) != nullptr)
            return err;
    }

    Ascii85 ascii85(output);

    if ((err = ascii85.begin()) != nullptr)
        return err;

    if ((err = ascii85.output(jpeg, length)) != nullptr)
        return err;

    return ascii85.end();
}

static MyError jpeg_do_outputL2(const uint8_t *jpeg, uint32_t length,
                                const JPEG::Info& info, PS_JPEG_Flags psFlags,
                                JobWS& job)
{
    MyError err;
    Output& output(job.output());

    if ((err = output_coordpair(info.width(), info.height(), output)) != nullptr)
        return err;

    if (!!(psFlags & PS_JPEG_Flags_Image_Grey) ||
        psFlags == PS_JPEG_Flags_None)
    {
        return outputAscii85(jpeg, length, info, psFlags, output);
    }

    if ((err = output.str("J8G\n")) != nullptr)
        return err;
    if ((err = jpeg_output_to_buffersprite(info, PDriverWS::instance())) != nullptr)
        return err;

    return jpeg_psimage_output_from_sprite(jpeg, length, info, psFlags, job);
}

static MyError jpeg_do_output(const uint8_t *jpeg, uint32_t length,
                              const JPEG::Info& info, PS_JPEG_Flags psFlags,
                              JobWS& job)
{
    MyError err;

    Output& output(job.output());

    if (job.document().level() >= PS::Document::Level_L2) {
        if ((err = jpeg_do_outputL2(jpeg, length, info, psFlags, job)) != nullptr)
            return err;
    } else {
        if ((err = jpeg_output_to_buffersprite(info, PDriverWS::instance())) != nullptr)
            return err;
        if ((err = output_coordpair(info.width(), info.height(), output)) != nullptr)
            return err;
        if (psFlags != PS_JPEG_Flags_None) {
            err = output.str("J8\n");
        } else {
            err = output.str("J32\n");
        }
        if (err) {
            return err;
        }
        if ((err = jpeg_psimage_output_from_sprite(jpeg, length, info, psFlags, job)) != nullptr)
            return err;
    }

    if ((err = output_grestore(job)) != nullptr)
        return err;

    return XJPEG_PDriverIntercept(1);
}

static MyError jpeg_trans_matrix_scaling(const JPEG::Info& info, Output& output)
{
    MyError err;

    if ((err = output_coordpair(180, info.dpiX(), output)) != nullptr)
        return err;
    if ((err = output_coordpair(180, info.dpiY(), output)) != nullptr)
        return err;
    if ((err = output.str("SS\n")) != nullptr)
        return err;
    if ((err = output.num(info.height())) != nullptr)
        return err;

    return output.str("JM\n");
}

MyError jpeg_ctrans_handle(ColourTrans::Table32K& outTableDescriptor,
                           CoreJobWS& coreJob)
{
    return nullptr;
}

MyError jpeg_plotscaled(const uint8_t *jpeg,
                        uint32_t length,
                        uint32_t flags,
                        OS::Unit x,
                        OS::Unit y,
                        const Sprite::ScaleFactor *scale,
                        CoreJobWS& coreJob)
{
    JobWS& job(toJobWS(coreJob));
    PDriverWS& ws(PDriverWS::instance());
    Output& output(job.output());
    PS_JPEG_Flags psFlags = PS_JPEG_Flags_None;
    MyError err = nullptr;

    if (ws.m_countingPass)
        return nullptr;

    *jpeg_buffer_sprite_slot(ws) = 0;

#if PSCoordSpeedUps
    if ((err = ensure_OScoords(output, job)) != nullptr)
        return err;
#endif

    if ((err = output_gsave(job)) != nullptr)
        return err;
    if ((err = jpeg_translate_by(x.raw(), y.raw(), output)) != nullptr)
        return err;

    JPEG::Info info;
    if ((err = info.parse(jpeg, length)) != nullptr)
        return err;

    if ((err = jpeg_scaling(scale, info.height(), job, output)) != nullptr)
        return err;

    if (info.monochrome())
        psFlags |= PS_JPEG_Flags_Image_Grey;
    if (!job.info.isColour())
        psFlags |= PS_JPEG_Flags_Printer_Grey;

    err = jpeg_do_output(jpeg, length, info, psFlags, job);

    // Clean up after jpeg_do_output()
    jpeg_release_buffersprite(ws);

    return err;
}

static MyError commonPrologue(const uint8_t* jpeg, uint32_t length,
                              JPEG::Info& info, PS_JPEG_Flags& psFlags,
                              JobWS& job)
{
    MyError err;
    PDriverWS& ws(PDriverWS::instance());
    Output& output(job.output());

    psFlags = PS_JPEG_Flags_None;
    *jpeg_buffer_sprite_slot(ws) = 0;

    if ((err = info.parse(jpeg, length)) != nullptr)
        return err;

    if (info.monochrome())
        psFlags |= PS_JPEG_Flags_Image_Grey;
    if (!job.info.isColour())
        psFlags |= PS_JPEG_Flags_Printer_Grey;

#if PSCoordSpeedUps
    if ((err = ensure_OScoords(output, job)) != nullptr)
        return err;
#endif

    if ((err = output_gsave(job)) != nullptr)
        return err;

    return nullptr;
}

MyError jpeg_plottransformed(const uint8_t *jpeg, uint32_t length,
                             const Draw::Matrix& matrix, CoreJobWS& coreJob)
{
    MyError err;
    JobWS& job(toJobWS(coreJob));
    PDriverWS& ws(PDriverWS::instance());
    Output& output(job.output());

    if (ws.m_countingPass)
        return nullptr;

    JPEG::Info info;
    PS_JPEG_Flags psFlags;
    if ((err = commonPrologue(jpeg, length, info, psFlags, job)) != nullptr)
        return err;

    if ((err = output.num(matrix.a.raw())) != nullptr)
        return err;
    if ((err = output.num(matrix.b.raw())) != nullptr)
        return err;
    if ((err = output.num(matrix.c.raw())) != nullptr)
        return err;
    if ((err = output.num(matrix.d.raw())) != nullptr)
        return err;
    if ((err = output.num(matrix.e.raw())) != nullptr)
        return err;
    if ((err = output.num(matrix.f.raw())) != nullptr)
        return err;

    if ((err = output.str("SDM\n")) != nullptr)
        return err;
    if ((err = jpeg_trans_matrix_scaling(info, output)) != nullptr)
        return err;

    err = jpeg_do_output(jpeg, length, info, psFlags, job);

    // Clean up after jpeg_do_output()
    jpeg_release_buffersprite(ws);

    return err;
}

MyError jpeg_plottransformed(const uint8_t *jpeg, uint32_t length,
                             const Sprite::Coords& coords, CoreJobWS& coreJob)
{
    MyError err;
    JobWS& job(toJobWS(coreJob));
    PDriverWS& ws(PDriverWS::instance());
    Output& output(job.output());

    if (ws.m_countingPass)
        return nullptr;

    JPEG::Info info;
    PS_JPEG_Flags psFlags;
    if ((err = commonPrologue(jpeg, length, info, psFlags, job)) != nullptr)
        return err;

    for (int i = 0; i < 4; ++i) {
        if ((err = output.num(coords[i].x.raw())) != nullptr)
            return err;
        if ((err = output.num(coords[i].y.raw())) != nullptr)
            return err;
    }

    // Output the width and height in pixels - negate height (coordinate flip).
    if ((err = output_coordpair(info.width(), -info.height(), output)) != nullptr)
        return err;

    // fit into parallogram command (PAR in prologue).
    if ((err = output.str("PAR\n")) != nullptr)
        return err;

    if ((err = output.str("1 1 1 1 SS\n")) != nullptr)
        return err;
    if ((err = output.num(info.height())) != nullptr)
        return err;
    if ((err = output.str("JM\n")) != nullptr)
        return err;

    err = jpeg_do_output(jpeg, length, info, psFlags, job);

    // Clean up after jpeg_do_output()
    jpeg_release_buffersprite(ws);

    return err;
}
