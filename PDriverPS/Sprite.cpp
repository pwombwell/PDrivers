#include "Core/PDriver.h"
#include "Sprite.h"

#include "Colour.h"
#include "GlobalWS.h"
#include "JobWS.h"
#include "Output.h"
#include "PDriverPS.h"
#include "Private.h"

#include "Common/ColTrans.h"
#include "Common/Sprite.h"

#include "Core/Colour.h"
#include "Core/ColourUtils.h"
#include "Core/InterceptColTrans.h"
#include "Core/Constants.h"
#include "Core/InterceptManager.h"
#include "Core/Job.h"
#include "Core/OS.h"
#include "Core/Workspace.h"

#include "RLib/OS/CharInput.h"
#include "RLib/OS/Sprite.h"

#include <stddef.h>

#if Medusa
extern const uint32_t SpriteType_New16bpp;
extern const uint32_t SpriteType_New32bpp;
#endif

#if Medusa
static const uint32_t sprite_fake_mask_colour = 0x00FFFFFFu;
#endif

static MyError sprite_check_escape(void) {
    if (OS::readEscapeState())
        return ErrorBlock_Escape;

    return nullptr;
}

static uint32_t sprite_ror(uint32_t value, uint32_t shift) {
    shift &= 31u;
    if (shift == 0u)
        return value;

    return (value >> shift) | (value << (32u - shift));
}


static MyError sprite_translateby(int32_t dx,
                                  int32_t dy,
                                  Output& output)
{
    if ((dx | dy) == 0)
        return nullptr;

    MyError err = output_coordpair(dx, dy, output);
    if (err)
        return err;

    return output.str("T\n");
}

#if Medusa
MyError sprite_output32bpp(const Sprite::Selector& spriteSel,
                           const Sprite::Info& spriteInfo,
                           const Sprite::PixelRect& sourceRect,
                           bool useMask,
                           JobWS& job)
{
    const uint32_t clippedWidth = (uint32_t)sourceRect.width();
    const uint32_t clippedHeight = (uint32_t)sourceRect.height();

    MyError err;
    Sprite::ResolvedSelector sprite;
    if ((err = spriteSel.resolve(sprite)) != nullptr)
        return err;

    const Sprite::HeaderView hdr = sprite.header();
    const uint8_t* image = hdr.imagePtr();
    const uint8_t* mask = nullptr;
    if (useMask)
        mask = hdr.maskPtr();

    uint32_t image_row_bytes = hdr.rowBytes();

    uint32_t modeFlags;
    if ((err = OS::xreadModeVariable(hdr.mode(),
                                     OS::ModeVar_ModeFlags,
                                     modeFlags)) != nullptr)
        return err;

    modeFlags &= ModeFlag_DataFormatFamily_Mask | ModeFlag_DataFormatSub_RGB;

    uint32_t mask_row_bytes = 0;
    if (mask)
        mask_row_bytes = ((spriteInfo.width() + 31u) >> 5) * 4u;

    uint32_t row_index = hdr.height() - 1u - (uint32_t)sourceRect.y0;
    image += row_index * image_row_bytes;
    if (mask) {
        mask += row_index * mask_row_bytes;
    }

    Output& output(job.output());
    // ASCII85 encoding only used with PS Level 2, but the constructor
    // doesn't do anything much, so fine to always construct.
    Ascii85 ascii85(output);

    if (!job.document().level1()) {
        // Flush BPUT so ASCII85 can reuse its buffers.
        if ((err = ascii85.begin()) != nullptr)
            return err;
    }

    if ((err = output_coordpair(clippedWidth, clippedHeight, output)) != nullptr)
        return err;
    if ((err = output.str("S32\n")) != nullptr)
        return err;

    for (uint32_t row = 0; row < clippedHeight; ++row) {
        if ((err = sprite_check_escape()) != nullptr)
            return err;

        uint32_t pixel_index = (uint32_t)sourceRect.x0;
        const uint8_t *img_ptr = image + (pixel_index << 2);
        const uint8_t *mask_ptr = mask;
        uint32_t mask_word = 0;
        if (mask) {
            uint32_t word_index = pixel_index >> 5;
            mask_ptr = mask + (word_index << 2);
            mask_word = *(const uint32_t *)mask_ptr;
            mask_ptr += 4;
            mask_word >>= (pixel_index & 31u);
        }

        for (uint32_t col = 0; col < clippedWidth; ++col) {
            uint32_t pixel;
            int do_swizzle = 0;
            if (mask) {
                if ((mask_word & 1u) == 0u) {
                    img_ptr += 4;
                    pixel = sprite_fake_mask_colour;
                } else {
                    pixel = *(const uint32_t *)img_ptr;
                    img_ptr += 4;
                    do_swizzle = 1;
                }
                mask_word >>= 1;
            } else {
                pixel = *(const uint32_t *)img_ptr;
                img_ptr += 4;
                do_swizzle = 1;
            }

            if (do_swizzle && (modeFlags & ModeFlag_DataFormatSub_RGB) != 0u) {
                uint32_t lr = pixel << 8;
                pixel &= 0x0000FF00u;
                pixel |= sprite_ror(lr, 24u);
            }

            if (job.document().level1()) {
                if ((err = output.hex((uint8_t)(pixel & 0xff))) != nullptr)
                    return err;
                if ((err = output.hex((uint8_t)((pixel >> 8) & 0xff))) != nullptr)
                    return err;
                if ((err = output.hex((uint8_t)((pixel >> 16) & 0xff))) != nullptr)
                    return err;
            } else {
                if ((err = ascii85.output(uint8_t(pixel & 0xff))) != nullptr)
                    return err;
                if ((err = ascii85.output(uint8_t((pixel >> 8) & 0xff))) != nullptr)
                    return err;
                if ((err = ascii85.output(uint8_t((pixel >> 16) & 0xff))) != nullptr)
                    return err;
            }

            pixel_index++;
            if (mask && (pixel_index & 0x1f) == 0) {
                mask_word = *(const uint32_t *)mask_ptr;
                mask_ptr += 4;
            }

            if (job.document().level() == PS::Document::Level_L1 &&
                (pixel_index & 0x1f) == 0)
            {
                if ((err = output.str('\n')) != nullptr)
                    return err;
            }
        }

        image -= image_row_bytes;
        if (mask)
            mask -= mask_row_bytes;
    }

    if (!job.document().level1())
        return ascii85.end();

    return output.str('\n');
}

MyError sprite_output16bpp(const Sprite::Selector& spriteSel,
                           const Sprite::Info& spriteInfo,
                           const Sprite::PixelRect& sourceRect,
                           bool useMask,
                           JobWS& job)
{
    const uint32_t clippedWidth = (uint32_t)sourceRect.width();
    const uint32_t clippedHeight = (uint32_t)sourceRect.height();

    MyError err;
    Sprite::ResolvedSelector sprite;
    if ((err = spriteSel.resolve(sprite)) != nullptr)
        return err;

    Output& output(job.output());

    // ASCII85 encoding only used with PS Level 2, but the constructor
    // doesn't do anything much, so fine to always construct.
    Ascii85 ascii85(output);

    if (!job.document().level1()) {
        // Flush BPUT so ASCII85 can reuse its buffers.
        if ((err = ascii85.begin()) != nullptr)
            return err;
    }

    const Sprite::HeaderView hdr = sprite.header();
    const uint8_t* image = hdr.imagePtr();
    const uint8_t* mask = nullptr;
    if (useMask)
        mask = hdr.maskPtr();

    uint32_t image_row_bytes = hdr.rowBytes();

    uint32_t modeFlags;
    if ((err = OS::xreadModeVariable(hdr.mode(),
                                     OS::ModeVar_ModeFlags,
                                     modeFlags)) != nullptr)
        return err;

    uint32_t ncolour;
    if ((err = OS::xreadModeVariable(hdr.mode(),
                                     OS::ModeVar_NColour,
                                     ncolour)) != nullptr)
        return err;

    ncolour++;
    if (ncolour == 65536u && (modeFlags & ModeFlag_FullPalette) != 0u) {
        ncolour >>= 1;
    }

    uint32_t (*map_fn)(uint32_t, int) = sprite_map565;
    if (ncolour < 32768u) {
        map_fn = sprite_map444;
    } else if (ncolour == 32768u) {
        map_fn = sprite_map555;
    }
    int rgb = (modeFlags & ModeFlag_DataFormatSub_RGB) != 0u;

    uint32_t mask_row_bytes = 0;
    if (mask)
        mask_row_bytes = ((spriteInfo.width() + 31u) >> 5) * 4u;

    uint32_t row_index = hdr.height() - 1u - (uint32_t)sourceRect.y0;
    image += row_index * image_row_bytes;
    if (mask) {
        mask += row_index * mask_row_bytes;
    }

    if ((err = output_coordpair(clippedWidth, clippedHeight, output)) != nullptr)
        return err;
    if ((err = output.str("S32\n")) != nullptr)
        return err;

    for (uint32_t row = 0; row < clippedHeight; ++row) {
        if ((err = sprite_check_escape()) != nullptr)
            return err;

        uint32_t pixel_index = (uint32_t)sourceRect.x0;
        const uint8_t *img_ptr = image + (pixel_index << 1);
        img_ptr = (const uint8_t *)((uintptr_t)img_ptr & ~3u);
        uint32_t pixel_word = *(const uint32_t *)img_ptr;
        img_ptr += 4;
        if ((pixel_index & 1u) != 0u) {
            pixel_word >>= 16;
        }

        const uint8_t *mask_ptr = mask;
        uint32_t mask_word = 0;
        if (mask) {
            uint32_t word_index = pixel_index >> 5;
            mask_ptr = mask + (word_index << 2);
            mask_word = *(const uint32_t *)mask_ptr;
            mask_ptr += 4;
            mask_word >>= (pixel_index & 31u);
        }

        for (uint32_t col = 0; col < clippedWidth; ++col) {
            uint32_t packed_pixel;
            if (mask) {
                if ((mask_word & 1u) != 0u) {
                    packed_pixel = pixel_word & 0xFFFFu;
                } else {
                    packed_pixel = sprite_fake_mask_colour & 0xFFFFu;
                }
                mask_word >>= 1;
            } else {
                packed_pixel = pixel_word & 0xFFFFu;
            }

            uint32_t pixel = map_fn(packed_pixel, rgb);

            if (job.document().level1()) {
                if ((err = output.hex((uint8_t)(pixel & 0xFFu))) != nullptr)
                    return err;
                if ((err = output.hex((uint8_t)((pixel >> 8) & 0xFFu))) != nullptr)
                    return err;
                if ((err = output.hex((uint8_t)((pixel >> 16) & 0xFFu))) != nullptr)
                    return err;
            } else {
                if ((err = ascii85.output(uint8_t(pixel & 0xFFu))) != nullptr)
                    return err;
                if ((err = ascii85.output(uint8_t((pixel >> 8) & 0xFFu))) != nullptr)
                    return err;
                if ((err = ascii85.output(uint8_t((pixel >> 16) & 0xFFu))) != nullptr)
                    return err;
            }

            if ((pixel_index & 1u) == 0u) {
                pixel_word >>= 16;
            } else {
                pixel_word = *(const uint32_t *)img_ptr;
                img_ptr += 4;
            }
            pixel_index++;

            if (mask && (pixel_index & 31u) == 0u) {
                mask_word = *(const uint32_t *)mask_ptr;
                mask_ptr += 4;
            }

            if (job.document().level1() && (pixel_index & 0x1f) == 0) {
                if ((err = output.str('\n')) != nullptr)
                    return err;
            }
        }

        image -= image_row_bytes;
        if (mask) {
            mask -= mask_row_bytes;
        }
    }

    if (!job.document().level1()) {
        return ascii85.end();
    }
    return output.str('\n');
}
#endif

inline uint32_t greyToRGB(uint8_t grey)
{
    // Well, bbggrr00.
    return (grey << 24) | (grey << 16) | (grey << 8);
}

static MyError sprite_setcolour(uint32_t *colourbounds_base,
                                        uint32_t *colourbounds_entry,
                                        const ColourMapping& mapping,
                                        JobWS& job)
{
    (void)colourbounds_base;

    uint32_t pixval = (uint32_t)((uintptr_t)colourbounds_entry -
                                 (uintptr_t)colourbounds_base);

    uint32_t rgb;

    switch (mapping.kind) {
        case ColourMapping::Palette:
            rgb = mapping.palette[pixval >> 2].on;
            break;

        case ColourMapping::GeneratedGreyTable:
            // SpriteOp has already plotted through the generated table into
            // an 8bpp temporary sprite, so the pixel value is itself a grey.
            rgb = greyToRGB(pixval & 0xff);
            break;

        case ColourMapping::ColourTrans: {
            const uint8_t* tableBytes = mapping.byteTable();
            uint8_t value = tableBytes[pixval >> 2];
            if (job.info.isColour()) {
                const ColourJob& colourJob(toColourJob(job));

                rgb = colourJob.lookupPixelValue(value);
            } else {
                rgb = greyToRGB(value);
            }
            break;
        }

        case ColourMapping::None:
            rgb = pixval_lookup(pixval >> 2, job);
            break;
    }

    MyError err = colour_setrealrgb(rgb, job);
    if (err)
        return err;

#if PSCoordSpeedUps
    return colour_ensure(job);
#else
    return nullptr;
#endif
}

static MyError sprite_scaling(OS::Mode spriteMode,
                              const Sprite::ScaleFactor *scale_factors,
                              JobWS &job,
                              uint32_t *log2bpc_out)
{
    int32_t scale_x = 1;
    int32_t scale_y = 1;
    int32_t scale_x_div = 1;
    int32_t scale_y_div = 1;
    MyError err;

    if (scale_factors != nullptr &&
        scale_factors != (const Sprite::ScaleFactor *)-1)
    {
        scale_x = (int32_t)scale_factors->multiplierX;
        scale_y = (int32_t)scale_factors->multiplierY;
        scale_x_div = (int32_t)scale_factors->divisorX;
        scale_y_div = (int32_t)scale_factors->divisorY;
    }

    uint32_t log2bpc;
    if ((err = OS::xreadModeVariable(spriteMode, OS::ModeVar_Log2BPC, log2bpc)) != nullptr)
        return err;

    scale_x_div <<= (int32_t)log2bpc;
    if (log2bpc_out) {
        *log2bpc_out = log2bpc;
    }

    scale_x <<= job.screenVars.currxeig;
    scale_y <<= job.screenVars.curryeig;

    Output& output(job.output());

    if ((err = output_coordpair(scale_x, scale_x_div, output)) != nullptr)
        return err;
    if ((err = output_coordpair(scale_y, scale_y_div, output)) != nullptr)
        return err;
    if ((err = output.str("SS\n")) != nullptr)
        return err;

    uint32_t log2bpp;
    if ((err = OS::xreadModeVariable(spriteMode, OS::ModeVar_Log2BPP, log2bpp)) != nullptr)
        return err;

    uint32_t bits_per_pixel = 1u << log2bpp;
    if ((err = output.num((int32_t)bits_per_pixel)) != nullptr)
        return err;
    return output.str(" SM\n");
}

static MyError sprite_createtempsprite(OS::Mode spriteMode,
                                       uint32_t log2bpc,
                                       PDriverWS& psWS)
{
    SpriteWorkspace& spriteWS(psWS.globaltempws.spritePlotting);
    uint32_t widthPixels = 256 >> log2bpc;
    MyError err;

    if ((err = XOS_SpriteInitArea(&psWS.sprarea.area)) != nullptr)
        return err;

    Sprite::Selector tmp(&psWS.sprarea.area, "tmp");
    Sprite::ResolvedSelector tmpAddress;

    if ((err = Sprite::create(tmp, false, widthPixels, 64, spriteMode)) != nullptr)
        return err;

#if Medusa
    // Create a second identical sprite to hold the mask data
    Sprite::Selector msk(&psWS.sprarea.area, "msk");
    Sprite::ResolvedSelector mskAddress;

    if ((err = Sprite::create(msk, false, widthPixels, 64, spriteMode)) != nullptr)
        return err;

    if ((err = msk.resolve(mskAddress)) != nullptr)
        return err;

    psWS.spritemskaddress = mskAddress.id().header();
#else
    // Give the sprite a mask
    if ((err == OSSpriteOp::xcreateMask(tmp) != nullptr)
        return err;
#endif

    if ((err = tmp.resolve(tmpAddress)) != nullptr)
        return err;

    spriteWS.spriteaddress = tmpAddress.id().header();

    return nullptr;
}

#if PSSprRLEncode
static MyError sprite_putmaskbyte(uint32_t *outbits, RLEncode& rle)
{
    uint8_t value = (uint8_t)((*outbits >> 24) & 0xFFu);
#if PSSprInverted
    value ^= 0xFFu;
#endif
    *outbits &= 0x00FFFFFFu;
    *outbits |= 0x01000000u;
    return rle.outputByte(value);
}

#else
static MyError sprite_putmaskbyte(uint32_t *outbits, RLEncode& rle)
{
    (void)rle;
    (void)job;
    uint8_t value = (uint8_t)((*outbits >> 24) & 0xFFu);
#if PSSprInverted
    value ^= 0xFFu;
#endif
    *outbits &= 0x00FFFFFFu;
    *outbits |= 0x01000000u;
    return output.writeHexByte(value);
}
#endif

// We should output this sprite all at once, via an 'image' operator.
//   R0:      address of sprite data.
//   R5:      width of the chunk in bits.
//   R6:      bits/pixel.
//   R7:      translation table pointer or palette (C++ passes separately)
//   R8:      Bit 16 set for unmasked plotting, clear for masked plotting.
//            Bits 31 to 24 are the pixel mask.
//            All other bits clear.
//   R9:      height of the chunk in pixels.
//   R10-R12: file handle and workspace pointers.
//   'spritelog2bpp' holds Log2(R6).
static MyError sprite_putchunk_allatonce(const uint8_t *data_base,
                                         int32_t width_bits,
                                         uint32_t bits_per_pixel,
                                         const ColourMapping& mapping,
                                         uint32_t pixel_mask,
                                         int32_t height,
                                         JobWS& job)
{
    MyError err;
    PDriverWS& psWS(PDriverWS::instance());
    Output& output(job.output());
    SpriteWorkspace& spriteWS(psWS.globaltempws.spritePlotting);

    uint32_t log2bpp = spriteWS.spritelog2bpp;
    int32_t width_pixels = width_bits >> (int32_t)log2bpp;

    // SN is 8bpp greyscale image path.
    if ((err = output_coordpair(width_pixels, height, output)) != nullptr)
        return err;
    if ((err = output.str("SN\n")) != nullptr)
        return err;

#if PSSprRLEncode
    RLEncode rle(output, job.document().level1());

    if ((err = rle.begin()) != nullptr)
        return err;
#endif

    const uint8_t *row = data_base;
    // `sprite_putchunk_allatonce_yloop`
    for (int32_t row_idx = 0; row_idx < height; ++row_idx) {
        int32_t bit = 0;
        uint32_t word = 0;
#if !PSSprRLEncode
        int32_t line_count = 33;
#endif

        // `sprite_putchunk_allatonce_xloop`
        while (bit < width_bits) {
            if ((bit & 31) == 0)
                // Get another word if it's needed.
                word = ((const uint32_t *)row)[(uint32_t)bit >> 5];

            uint32_t pixval = word & pixel_mask;
            uint32_t grey;

            switch (mapping.kind) {
                case ColourMapping::Palette:
                    // Got a 256-entry sprite palette; convert its real RGB to grey.
                    // (PSAllowHighTables bit set - use supplied palette).
                    grey = ColourUtils_rgbtogrey(mapping.palette[pixval].on);
                    break;

                case ColourMapping::GeneratedGreyTable:
                    // Is it our truecolour trans table?
                    // If so, pixel value is a valid grey level.
                    grey = pixval;
                    break;

                case ColourMapping::ColourTrans:
                    // Greyscale ColourTrans table: source pixel value -> grey byte.
                    // (PSAllowHighTables bit clear - use table.)
                    grey = mapping.byteTable()[pixval];
                    break;

                case ColourMapping::None: {
                    // Otherwise, look up screen RGB, the convert to grey.
                    uint32_t rgb = pixval_lookup(pixval, job);
                    grey = ColourUtils_rgbtogrey(rgb);
                    break;
                }
            }

#if PSSprRLEncode
            if ((err = rle.outputByte(grey)) != nullptr)
                return err;
#else
            if (--line_count == 0) {
                if ((err = output.str('\n')) != nullptr)
                    return err;
                if ((err = sprite_check_escape()) != nullptr)
                    return err;
                line_count = 32;
            }
            if ((err = output.hex((uint8_t)grey)) != nullptr)
                return err;
#endif

            bit += (int32_t)bits_per_pixel;
            word >>= bits_per_pixel;
        }

#if !PSSprRLEncode
        if ((err = output.str('\n')) != nullptr)
            return err;
        if ((err = sprite_check_escape()) != nullptr)
            return err;
#endif

        row -= 32;
    }

#if PSSprRLEncode
    return rle.end();
#else
    return nullptr;
#endif
}

static MyError sprite_fillboundingbox(uint32_t *colourbounds_base,
                                      uint32_t *colourbounds_entry,
                                      const ColourMapping& mapping,
                                      JobWS& job)
{
    MyError err;
    Output& output(job.output());
    PDriverWS& psWS(PDriverWS::instance());
    SpriteWorkspace& spriteWS(psWS.globaltempws.spritePlotting);

    if ((err = sprite_setcolour(colourbounds_base, colourbounds_entry,
                                           mapping, job)) != nullptr)
        return err;
    if ((err = output_gsave(job)) != nullptr)
        return err;

    uint32_t entry = *colourbounds_entry;
    int32_t left = (int32_t)(entry >> 24);
    int32_t right = (int32_t)((entry >> 8) & 0xFFu);
    int32_t bottom = (int32_t)((entry >> 16) & 0xFFu);
    int32_t top = (int32_t)(entry & 0xFFu);

    if ((err = sprite_translateby(left, bottom, output)) != nullptr)
        return err;

    int32_t height = top - bottom + 1;
    int32_t width_bits = right - left;
    uint32_t log2bpp = spriteWS.spritelog2bpp;
    int32_t width_pixels = (width_bits >> (int32_t)log2bpp) + 1;

    if ((err = output_coordpair(width_pixels, height, output)) != nullptr)
        return err;
    if ((err = output.str("SF ")) != nullptr)
        return err;
    return output_grestore(job);
}

// Output a 1bpp graphics plane in a solid colour.
static MyError sprite_putplane(const uint8_t *data_base,
                               const uint8_t *mask_base,
                               uint32_t *colourbounds_base,
                               uint32_t *colourbounds_entry,
                               uint32_t bits_per_pixel,
                               uint32_t pixel_mask,
                               uint32_t analysis_flags,
                               const ColourMapping& mapping,
                               JobWS& job)
{
    MyError err;
    PDriverWS& psWS = PDriverWS::instance();
    Output& output(job.output());
    SpriteWorkspace& spriteWS(psWS.globaltempws.spritePlotting);

    if ((err = sprite_setcolour(colourbounds_base, colourbounds_entry,
                                mapping, job)) != nullptr)
        return err;
    if ((err = output_gsave(job)) != nullptr)
        return err;

    uint32_t pixval = (uint32_t)(colourbounds_entry - colourbounds_base);

    uint32_t entry = *colourbounds_entry;
    int32_t left = (int32_t)(entry >> 24);
    int32_t right = (int32_t)((entry >> 8) & 0xFFu);
    int32_t bottom = (int32_t)((entry >> 16) & 0xFFu);
    int32_t top = (int32_t)(entry & 0xFFu);

    if ((err = sprite_translateby(left, bottom, output)) != nullptr)
        return err;

    const uint8_t *data_row = data_base - (bottom * 32);
    const uint8_t *mask_row = mask_base - (bottom * 32);

    int32_t height = top - bottom + 1;
    int32_t width_bits = right - left;
    uint32_t log2bpp = spriteWS.spritelog2bpp;
    int32_t width_pixels = (width_bits >> (int32_t)log2bpp) + 1;

    if ((err = output_coordpair(width_pixels, height, output)) != nullptr)
        return err;
    if ((err = output.str("S1\n")) != nullptr) // 1bpp mask
        return err;

#if PSSprRLEncode
    RLEncode rle(output, job.document().level1());

    if ((err = rle.begin()) != nullptr)
        return err;
#endif

    for (int32_t row = 0; row < height; ++row) {
        int32_t x = left;
        int32_t right_limit = right;

        uint32_t data_word = 0;
        uint32_t mask_word = 0;
        int32_t aligned = x & ~31;
        data_word = ((const uint32_t *)(data_row + ((uint32_t)aligned >> 3)))[0];
        mask_word = ((const uint32_t *)(mask_row + ((uint32_t)aligned >> 3)))[0];
        uint32_t shift = (uint32_t)(x & 31);
        data_word >>= shift;
        mask_word >>= shift;

        uint32_t outbits = 0x800000;
        while (x <= right_limit) {
            if ((x & 31) == 0) {
                data_word = ((const uint32_t *)(data_row + ((uint32_t)x >> 3)))[0];
                mask_word = ((const uint32_t *)(mask_row + ((uint32_t)x >> 3)))[0];
            }

            // `analysis_flags` is not a SpriteOp plot action here.  It mirrors
            // the ARM routine's private R8 value after analysis setup:
            //
            //   bit 16      set => unmasked plotting: ignore mask_word
            //               clear => masked plotting: only opaque pixels count
            //   bits 31..24 pixel mask for extracting one source pixel
            //
            // A synthetic pixel value of 0x100 means "transparent".  The
            // colourbounds table has 0x101 entries, so colour 0x100 records
            // whether any transparent pixels were seen.
            uint32_t pixel = 0x100;
            if ((analysis_flags & 0x10000u) != 0u) {
                pixel = data_word & pixel_mask;
            } else {
                if ((mask_word & pixel_mask) != 0u)
                    pixel = data_word & pixel_mask;
            }

            if (pixel == pixval) {
                outbits |= 0x400000;
            }

            uint32_t carry = outbits & 0x80000000;
            outbits <<= 1;
            if (carry != 0u) {
                if ((err = sprite_putmaskbyte(&outbits, rle)) != nullptr)
                    return err;
            }

            x += (int32_t)bits_per_pixel;
            data_word >>= bits_per_pixel;
            mask_word >>= bits_per_pixel;
        }

        while (1) {
            uint32_t carry = outbits & 0x80000000;
            outbits <<= 1;
            if (carry != 0) {
                if ((err = sprite_putmaskbyte(&outbits, rle)) != nullptr)
                    return err;
                break;
            }
        }

#if !PSSprRLEncode
        if ((err = output.str('\n')) != nullptr)
            return err;
        if ((err = sprite_check_escape()) != nullptr)
            return err;
#endif

        data_row -= 32;
        mask_row -= 32;
    }

#if PSSprRLEncode
    if ((err = rle.end()) != nullptr)
        return err;
#endif

    return output_grestore(job);
}

// Subroutine to output a specified rectangular chunk of a sprite.
// `chunk` is an inc/exc rect in source-sprite pixels.
// The sprite pointed to by 'spritehandle' is used as the temporary destination.
static MyError sprite_putchunk(const Sprite::Selector& sprite,
                               const Sprite::Info& info,
                               const Sprite::PixelRect& chunk,
                               OS::Mode spriteMode,
                               const ColourMapping& mapping,
                               Sprite::PlotAction action,
                               JobWS& job)
{
    Output& output(job.output());
    int colour_count = 0;

    Sprite::Pixel left = chunk.x0;
    Sprite::Pixel bottom = chunk.y0;
    Sprite::Pixel width_pixels = chunk.width();
    Sprite::Pixel height_pixels = chunk.height();

    // Plot the region of the sprite that matches the chunk to the sprite
    // pointed to by 'spriteaddress'

    // Negate the bottom left corner and convert it to OS co-ordinates (this is
    // the position at which we want to plot the given sprite in the one pointed
    // at by 'spritehandle').
    int32_t x = -left;
    int32_t y = -bottom;

    MyError err;

    uint32_t xEigF, yEigF;
    if ((err = OS::xreadModeVariable(spriteMode, OS::ModeVar_XEigFactor, xEigF)) != nullptr)
        return err;
    if ((err = OS::xreadModeVariable(spriteMode, OS::ModeVar_YEigFactor, yEigF)) != nullptr)
        return err;

    x <<= (int32_t)xEigF;
    y <<= (int32_t)yEigF;

    // Switch output to the sprite pointed to by 'spritehandle'
    PDriverWS& psWS(PDriverWS::instance());
    SpriteWorkspace& spriteWS(psWS.globaltempws.spritePlotting);

    const Sprite::Header *hdr = spriteWS.spriteaddress;
    Sprite::Selector tmp(&psWS.sprarea.area, hdr);

    Sprite::ScopedVDUContext context; // was using `oldspritestate`
    if ((err = context.switchOutputToSprite(tmp)) != nullptr)
        return err;

    // Plot the given chunk's data.
#ifdef Medusa
    // We only want to pass a translation table through if it is our own
    // 32K greyscale one.  Zero it if not.
    const ColourTrans::Table32K* chunkTable = nullptr;
    if (mapping.kind == ColourMapping::GeneratedGreyTable)
        chunkTable = &mapping.generated;

    if ((err = Sprite::putScaled(sprite, x, y,
                                 Sprite::PlotAction(OS::GCOLAction_Src),
                                 nullptr, chunkTable)) != nullptr)
        goto sprite_putchunk_restore;
#else
    if ((err = Sprite::xputSpriteUserCoords(sprite, x, y, 0)) != nullptr)
        goto sprite_putchunk_restore;
#endif

    if ((action & (Sprite::PlotAction_GCOLMask | Sprite::PlotAction_UseMask)) != OS::GCOLAction_Src) {
        // no need to restore to this as we restore to the previous one.
        Sprite::VDUContext tmpContext;

#ifdef Medusa
        Sprite::Selector msk(&psWS.sprarea.area, psWS.spritemskaddress);

        if ((err = Sprite::switchOutputToSprite(msk, nullptr, tmpContext)) != nullptr)
            goto sprite_putchunk_restore;
#else
        if ((err = OSSpriteOp::xswitchOutputToSprite(tmp, nullptr, tmpContext)) != nullptr)
            goto sprite_putchunk_restore;
#endif

        if ((err = XOS_WriteI(16)) != nullptr)
            goto sprite_putchunk_restore;
        if ((err = XOS_WriteI(18)) != nullptr)
            goto sprite_putchunk_restore;
        if ((err = XOS_WriteI(4)) != nullptr)
            goto sprite_putchunk_restore;
        if ((err = XOS_WriteI(128)) != nullptr)
            goto sprite_putchunk_restore;

        // Plot the original sprite's mask into the msk sprite.
        if ((err = Sprite::plotMaskScaled(sprite, x, y)) != nullptr)
            goto sprite_putchunk_restore;
    }

sprite_putchunk_restore:
    MyError restoreErr = context.restore();
    if (err)
        return err;
    if (restoreErr)
        return restoreErr;

    uint32_t log2bpc;
    if ((err = OS::xreadModeVariable(spriteMode, OS::ModeVar_Log2BPC, log2bpc)) != nullptr)
        return err;

    int32_t left_bits = left << (int32_t)log2bpc;
    int32_t width_bits = width_pixels << (int32_t)log2bpc;

    uint32_t log2bpp;
    if ((err = OS::xreadModeVariable(spriteMode, OS::ModeVar_Log2BPP, log2bpp)) != nullptr)
        return err;

    spriteWS.spritelog2bpp = log2bpp;
    uint32_t bits_per_pixel = 1u << log2bpp;
    uint32_t pixel_mask = bits_per_pixel >= 32u ? 0xFFFFFFFFu : ((1u << bits_per_pixel) - 1u);

    if ((err = output_gsave(job)) != nullptr)
        return err;
    if ((err = sprite_translateby(left_bits, bottom, output)) != nullptr)
        return err;

    uint32_t *colourbounds = psWS.colourbounds;
#if PSSprUseBBoxes
    uint32_t init = 0xFFFF0000u;
#else
    uint32_t init = 0x00FF0000u;
#endif
    for (int i = 0x100; i >= 0; --i) {
        colourbounds[i] = init;
    }

    const Sprite::HeaderView header = hdr;
    const uint8_t* image_base = header.imagePtr();
    const uint8_t* mask_base = header.maskPtr();
    if (mask_base == nullptr)
        mask_base = image_base;
    image_base += 63 * 32;
    mask_base += 63 * 32;

    // Norcroft can't do enum operators in a namespace, it seems.
    const Sprite::PlotAction mask = Sprite::PlotAction(Sprite::PlotAction_GCOLMask | Sprite::PlotAction_UseMask);

    // Build the ARM routine's private R8 analysis word.  This is deliberately
    // not a SpriteOp plot action, even though it is derived from one.
    //
    // On entry to the ARM sprite_putchunk, R8 is:
    //   0 => unmasked plotting, or masked plot requested but no mask exists
    //   8 => masked plotting and the sprite definitely has a mask
    //
    // The ARM then does:
    //   EOR R8,R8,#8
    //   MOV R8,R8,LSL #13
    //   ORR R8,R8,pixel_mask,LSL #24
    //
    // So after this conversion:
    //   bit 16      set => unmasked; ignore mask data during analysis/output
    //               clear => masked; use mask data to detect transparency
    //   bits 31..24 contain the source-pixel mask
    //
    // The low action bits are no longer meaningful after this point.  Keep
    // this as uint32_t; casting it back to Sprite::PlotAction can lose the
    // synthetic high bits on compilers where the enum has a narrow type.
    uint32_t analysis_flags = (((action & mask) ^ mask) << 13);
    analysis_flags |= (pixel_mask << 24);

#if !PSSprUseBBoxes
    uint32_t chunk_boundary = (uint32_t)((height_pixels - 1) +
                                         ((uint32_t)width_bits << 8) -
                                         ((uint32_t)bits_per_pixel << 8));
#endif

    const uint8_t* row_data = image_base - (height_pixels - 1) * 32;
    const uint8_t* row_mask = mask_base - (height_pixels - 1) * 32;

    for (int32_t y = height_pixels - 1; y >= 0; --y) {
        const uint32_t* data_word_ptr = (const uint32_t *)row_data;
        const uint32_t* mask_word_ptr = (const uint32_t *)row_mask;
        uint32_t data_word = 0;
        uint32_t mask_word = 0;
        int32_t x = 0;

        while (x < width_bits) {
            if ((x & 31) == 0) {
                data_word = *data_word_ptr++;
                mask_word = *mask_word_ptr++;
            }

            uint32_t pixel;
            if ((analysis_flags & 0x10000u) == 0 && (mask_word & pixel_mask) == 0) {
                pixel = 0x100u;
            } else {
                pixel = data_word & pixel_mask;
            }

#if PSSprUseBBoxes
            uint8_t *bounds = (uint8_t *)colourbounds + (pixel << 2);
            if (y > bounds[0]) {
                bounds[0] = (uint8_t)y;
            }
            if (x > bounds[1]) {
                bounds[1] = (uint8_t)x;
            }
            if (y < bounds[2]) {
                bounds[2] = (uint8_t)y;
            }
            if (x < bounds[3]) {
                bounds[3] = (uint8_t)x;
            }
#else
            colourbounds[pixel] = chunk_boundary;
#endif

            data_word = sprite_ror(data_word, bits_per_pixel);
            mask_word = sprite_ror(mask_word, bits_per_pixel);
            x += (int32_t)bits_per_pixel;
        }

        row_data += 32;
        row_mask += 32;
    }

    if ((colourbounds[0x100] & 0x00800000u) == 0u) {
        goto sprite_putchunk_colourbycolour;
    }

    colour_count = 0;
    for (int i = 0xFF; i >= 0; --i) {
        if ((colourbounds[i] & 0x00800000u) == 0u) {
            colour_count += 1;
        }
    }

#if PSSprColLimit >= 256
#if !PSSprFillChunk
    if (colour_count != 1)
        goto sprite_putchunk_colourbycolour;
#endif
    goto sprite_putchunk_fillandcols;
#else
    if (colour_count == 1)
        goto sprite_putchunk_fillandcols;

    if (job.info.isColour())
#if PSSprFillChunk
        goto sprite_putchunk_fillandcols;
#else
        goto sprite_putchunk_colourbycolour;
#endif

    if (colour_count < PSSprColLimit)
#if PSSprFillChunk
        goto sprite_putchunk_fillandcols;
#else
        goto sprite_putchunk_colourbycolour;
#endif

    err = sprite_putchunk_allatonce(image_base, width_bits, bits_per_pixel,
                                    mapping, pixel_mask, height_pixels,
                                    job);
    if (err)
        return err;

    return output_grestore(job);
#endif

sprite_putchunk_fillandcols: {
        uint32_t *skip_entry = nullptr;
#if PSSprFillChunk && PSSprUseBBoxes
        uint32_t best_area = 0u;
        uint32_t *best_ptr = nullptr;
        for (int i = 0x100; i >= 0; --i) {
            uint32_t entry = colourbounds[i];
            uint32_t low_y = (entry >> 16) & 0xFFu;
            uint32_t high_y = entry & 0xFFu;
            int32_t height = (int32_t)high_y - (int32_t)low_y;
            if (height < 0) {
                continue;
            }
            height += 1;
            uint32_t low_x = entry >> 24;
            uint32_t high_x = (entry >> 8) & 0xFFu;
            int32_t width_bits_local = (int32_t)high_x - (int32_t)low_x;
            int32_t width_bytes = (width_bits_local >> 3) + 1;
            uint32_t area = (uint32_t)width_bytes * (uint32_t)height;
            if (best_ptr == nullptr || area >= best_area) {
                best_area = area;
                best_ptr = &colourbounds[i];
            }
        }
        if (best_ptr != nullptr) {
            if ((err = sprite_fillboundingbox(colourbounds, best_ptr, table, palette, ws)) != nullptr)
                return err;
            skip_entry = best_ptr;
        }
#else
        for (int i = 256; i >= 0; --i) {
            if ((colourbounds[i] & 0x00800000u) == 0u) {
                if ((err = sprite_fillboundingbox(colourbounds, &colourbounds[i], mapping, job)) != nullptr)
                    return err;
                skip_entry = &colourbounds[i];
                break;
            }
        }
#endif

        for (int i = 255; i >= 0; --i) {
            uint32_t *entry_ptr = &colourbounds[i];
            if (entry_ptr == skip_entry)
                continue;

            if ((*entry_ptr & 0x00800000u) != 0)
                continue;

            err = sprite_putplane(image_base, mask_base, colourbounds,
                                  entry_ptr, bits_per_pixel, pixel_mask,
                                  analysis_flags, mapping, job);
            if (err)
                return err;
        }
        return output_grestore(job);
    }

sprite_putchunk_colourbycolour:
    // For each colour in the palette, output a 1bpp plane.
    for (int i = 255; i >= 0; --i) {
        uint32_t *entry_ptr = &colourbounds[i];
        if ((*entry_ptr & 0x00800000u) != 0)
            continue;

        err = sprite_putplane(image_base, mask_base, colourbounds,
                              entry_ptr, bits_per_pixel, pixel_mask,
                              analysis_flags, mapping, job);
        if (err) {
            return err;
        }
    }
    return output_grestore(job);
}

static MyError sprite_maskchunk(const Sprite::Selector& sprite,
                                const Sprite::PixelRect& chunk,
                                OS::Mode spriteMode,
                                bool mask_exists,
                                JobWS& job)
{
    // FIXME: Check with comments in Sprite.s (and copy them here!)
    PDriverWS& psWS(PDriverWS::instance());
    SpriteWorkspace& spriteWS(psWS.globaltempws.spritePlotting);
    Output& output(job.output());
    MyError err;

    Sprite::Pixel left = chunk.x0;
    Sprite::Pixel bottom = chunk.y0;
    Sprite::Pixel width_pixels = chunk.width();
    Sprite::Pixel height_pixels = chunk.height();

    if (!mask_exists) {
        uint32_t log2bpc;
        if ((err = OS::xreadModeVariable(spriteMode, OS::ModeVar_Log2BPC, log2bpc)) != nullptr)
            return err;

        int32_t left_bits = left << (int32_t)log2bpc;
        int32_t width_bits = width_pixels << (int32_t)log2bpc;

        uint32_t log2bpp;
        if ((err = OS::xreadModeVariable(spriteMode, OS::ModeVar_Log2BPP, log2bpp)) != nullptr)
            return err;

        spriteWS.spritelog2bpp = log2bpp;

        if ((err = output_gsave(job)) != nullptr)
            return err;
        if ((err = sprite_translateby(left_bits, bottom, output)) != nullptr)
            return err;

        int32_t width_pix = width_bits >> (int32_t)log2bpp;

        if ((err = output_coordpair(width_pix, height_pixels, output)) != nullptr)
            return err;
        if ((err = output.str("SF ")) != nullptr)
            return err;

        return output_grestore(job);
    }

    int32_t plot_x = -left;
    int32_t plot_y = -bottom;

    uint32_t xEigF, yEigF;
    if ((err = OS::xreadModeVariable(spriteMode, OS::ModeVar_XEigFactor, xEigF)) != nullptr)
        return err;
    if ((err = OS::xreadModeVariable(spriteMode, OS::ModeVar_YEigFactor, yEigF)) != nullptr)
        return err;

    plot_x <<= (int32_t)xEigF;
    plot_y <<= (int32_t)yEigF;

    // Transfer the mask data. Technique: switch output to the mask of the sprite
    //  pointed to by 'spriteaddress'. Clear it to zeroes, then plot the mask of
    // the given sprite, using the inverting action.

    Sprite::Area* spriteArea = &psWS.sprarea.area;

#if Medusa
    const Sprite::Header* spriteHeader = psWS.spritemskaddress;
    Sprite::Selector selector(spriteArea, spriteHeader);

    Sprite::ScopedVDUContext context; // was using `oldspritestate`
    if ((err = context.switchOutputToSprite(selector)) != nullptr)
        return err;
#else
    Sprite::Header* spriteHeader = spriteWS.spriteaddress;
    Sprite::Selector selector(spriteArea, spriteHeader);
    Sprite::ScopedVDUContext context;
    if ((err = context.switchOutputToMask(selector)) != nullptr)
        return err;
#endif

    // Current background must be zeroes after the output switch, so we can do a CLG immediately
    if ((err = XOS_WriteI(16u)) != nullptr) // CLG
        goto sprite_maskchunk_restore;
    if ((err = XOS_WriteI(18u)) != nullptr) // Set inverting background
        goto sprite_maskchunk_restore;
    if ((err = XOS_WriteI(4u)) != nullptr)
        goto sprite_maskchunk_restore;
    if ((err = XOS_WriteI(128u)) != nullptr)
        goto sprite_maskchunk_restore;

    // Plot this sprite's mask, with no scaling.
    if ((err = Sprite::plotMaskScaled(sprite, plot_x, plot_y)) != nullptr)
        goto sprite_maskchunk_restore;

sprite_maskchunk_restore:
    MyError restore_err = context.restore();
    if (err)
        return err;
    if (restore_err)
        return restore_err;

    uint32_t log2bpc;
    if ((err = OS::xreadModeVariable(spriteMode, OS::ModeVar_Log2BPC, log2bpc)) != nullptr)
        return err;

    int32_t left_bits = left << log2bpc;
    int32_t width_bits = width_pixels << log2bpc;

    // To interpret colours in the data, we need this form of Log2(bits/pixel)
    uint32_t log2bpp;
    if ((err = OS::xreadModeVariable(spriteMode, OS::ModeVar_Log2BPP, log2bpp)) != nullptr)
        return err;
    spriteWS.spritelog2bpp = log2bpp;

    if ((err = output_gsave(job)) != nullptr)
        return err;
    if ((err = sprite_translateby(left_bits, bottom, output)) != nullptr)
        return err;

    // The next step is to scan the chunk we are to plot, to find interesting
    // information about the non-zero mask pixels.

#if Medusa
    const Sprite::HeaderView header = psWS.spritemskaddress;
    // The mask data is actually in the image
    const uint8_t* maskBase = header.imagePtr();
#else
    const Sprite::HeaderView header = spriteWS.spriteaddress;
    // Get transparency mask pointer.
    const uint8_t *maskBase = header.maskPtr();
#endif

    // Adjust pointer for origin at bottom left.
    maskBase += 63 * 32;

    // Calculate bits/pixel and pixel mask.
    uint32_t bpp = 1u << log2bpp;
    uint32_t pixel_mask = (1u << bpp) - 1;

#if PSSprUseBBoxes
    // Now do the analysis: we want the bounding box and information about
    // whether transparent pixels exist.
    //   Important register contents are:
    // R0:      address of sprite mask.
    // R1:      (will be used for high X).
    // R2:      (will be used for high Y).
    // R3:      (will be used for low X).
    // R4:      (will be used for low Y).
    // R5:      width of the chunk in bits.
    // R6:      bits/pixel.
    // R7:      (will be used to indicate existence of a transparent pixel).
    // R8:      pixel mask.
    // R9:      height of the chunk in pixels.
    // R10-R12: file handle and workspace pointers.

    int32_t high_x = 0;
    int32_t high_y = 0;
    int32_t low_x = 0xFF;
    int32_t low_y = 0xFF;
    bool transparent = false;

    const uint8_t *row_mask = maskBase - (height_pixels - 1) * 32;
    for (int32_t y = height_pixels - 1; y >= 0; --y) {
        const uint32_t *mask_word_ptr = (const uint32_t *)row_mask;
        uint32_t mask_word = 0;
        int32_t x = 0;
        while (x < width_bits) {
            if ((x & 31) == 0) {
                mask_word = *mask_word_ptr++;
            }
            if ((mask_word & pixel_mask) == 0u) {
                transparent = true;
            } else {
                if (y > high_y) {
                    high_y = y;
                }
                if (x > high_x) {
                    high_x = x;
                }
                if (y < low_y) {
                    low_y = y;
                }
                if (x < low_x) {
                    low_x = x;
                }
            }
            mask_word = sprite_ror(mask_word, bpp);
            x += (int32_t)bpp;
        }
        row_mask += 32;
    }

    if (high_x < low_x) {
        return output_grestore(job);
    }

    int32_t box_left = low_x;
    int32_t box_right = high_x;
    int32_t box_bottom = low_y;
    int32_t box_top = high_y;
#else
    // Now do the analysis: we want information about whether tranparent and
    // non-transparent pixels exist. This code also sets up a fake bounding box in
    // R1-R4 to fit in with the optimised case above.
    //   Important register contents are:
    // R0:      address of sprite mask.
    // R1:      (will be used to indicate existence of a non-transparent pixel).
    // R5:      width of the chunk in bits.
    // R6:      bits/pixel.
    // R7:      (will be used to indicate existence of a transparent pixel).
    // R8:      pixel mask.
    // R9:      height of the chunk in pixels.
    // R10-R12: file handle and workspace pointers.

    uint32_t transparent = false;
    bool opaque = false;
    int32_t y = height_pixels - 1;
    const uint8_t *row_mask = maskBase - y * 32;
    while (y >= 0 && !(transparent && opaque)) {
        const uint32_t *mask_word_ptr = (const uint32_t *)row_mask;
        uint32_t mask_word = 0;
        int32_t x = 0;
        while (x < width_bits && !(transparent && opaque)) {
            if ((x & 31) == 0) {
                mask_word = *mask_word_ptr++;
            }
            if ((mask_word & pixel_mask) == 0u) {
                transparent = true;
            } else {
                opaque = true;
            }
            mask_word = sprite_ror(mask_word, bpp);
            x += (int32_t)bpp;
        }
        row_mask += 32;
        y -= 1;
    }
    if (!opaque) {
        return output_grestore(job);
    }
    int32_t box_left = 0;
    int32_t box_bottom = 0;
    int32_t box_right = width_bits - (int32_t)bpp;
    int32_t box_top = height_pixels - 1;
#endif

    if ((err = sprite_translateby(box_left, box_bottom, output)) != nullptr)
        return err;

    const uint8_t *mask_row = maskBase - (box_bottom * 32);
    int32_t height = box_top - box_bottom + 1;
    int32_t width_pix = ((box_right - box_left) >> (int32_t)log2bpp) + 1;

    if ((err = output_coordpair(width_pix, height, output)) != nullptr)
        return err;

    if (!transparent) {
        if ((err = output.str("SF ")) != nullptr)
            return err;
        return output_grestore(job);
    }

    if ((err = output.str("S1\n")) != nullptr) // 1bpp mask
        return err;

#if PSSprRLEncode
    RLEncode rle(output, job.document().level1());

    if ((err = rle.begin()) != nullptr)
        return err;
#endif

    // `sprite_maskchunk_yloop`
    for (int32_t row = 0; row < height; ++row) {
        int32_t x = box_left;
        int32_t right_limit = box_right;
        uint32_t mask_word = 0;
        int32_t aligned = x & ~31;
        mask_word = ((const uint32_t *)(mask_row + ((uint32_t)aligned >> 3)))[0];
        uint32_t shift = (uint32_t)(x & 31);
        mask_word >>= shift;

        uint32_t outbits = 0x800000u;
        while (x <= right_limit) {
            if ((x & 31) == 0) {
                mask_word = ((const uint32_t *)(mask_row + ((uint32_t)x >> 3)))[0];
            }
            if ((mask_word & pixel_mask) != 0u) {
                outbits |= 0x400000u;
            }

            uint32_t carry = outbits & 0x80000000u;
            outbits <<= 1;
            if (carry != 0u) {
                if ((err = sprite_putmaskbyte(&outbits, rle)) != nullptr)
                    return err;
            }

            x += (int32_t)bpp;
            mask_word >>= bpp;
        }

        while (1) {
            uint32_t carry = outbits & 0x80000000u;
            outbits <<= 1;
            if (carry != 0u) {
                if ((err = sprite_putmaskbyte(&outbits, rle)) != nullptr)
                    return err;
                break;
            }
        }

#if !PSSprRLEncode
        if ((err = output.str('\n')) != nullptr)
            return err;
        if ((err = sprite_check_escape()) != nullptr)
            return err;
#endif

        mask_row -= 32;
    }

#if PSSprRLEncode
    if ((err = rle.end()) != nullptr)
        return err;
#endif

    return output_grestore(job);
}

MyError sprite_commonoutput(const Sprite::Selector& sprite,
                            const Sprite::Info& spriteInfo,
                            const Sprite::PixelRect& sourceRect,
                            const ColourMapping& mapping,
                            Sprite::PlotAction action,
                            uint32_t log2bpc,
                            JobWS& job)
{
    MyError err;

    if ((err = sprite_createtempsprite(spriteInfo.mode(), log2bpc, PDriverWS::instance())) != nullptr)
        return err;

    const Sprite::Pixel maxChunkWidth = Sprite::Pixel(256 >> log2bpc);
    const Sprite::Pixel maxChunkHeight = 64;

    // Match the ARM order: columns right-to-left, rows top-to-bottom.
    for (Sprite::Pixel right = sourceRect.x1; right > sourceRect.x0; ) {
        Sprite::Pixel width = right - sourceRect.x0;
        if (width > maxChunkWidth)
            width = maxChunkWidth;

        Sprite::Pixel left = right - width;

        for (Sprite::Pixel top = sourceRect.y1; top > sourceRect.y0; ) {
            Sprite::Pixel height = top - sourceRect.y0;
            if (height > maxChunkHeight)
                height = maxChunkHeight;

            Sprite::Pixel bottom = top - height;
            Sprite::PixelRect chunk(left, bottom, right, top);

            err = sprite_putchunk(sprite, spriteInfo,
                                  chunk, spriteInfo.mode().toNumber(),
                                  mapping, action,
                                  job);
            if (err)
                return err;

            top = bottom;
        }

        right = left;
    }

    return nullptr;
}

MyError sprite_mask_commonoutput(const Sprite::Selector& sprite,
                                 const Sprite::Info& spriteInfo,
                                 const Sprite::PixelRect& sourceRect,
                                 uint32_t log2bpc,
                                 JobWS& job)
{
    MyError err;
    PDriverWS& psWS = PDriverWS::instance();

    if ((err = sprite_createtempsprite(spriteInfo.mode().toNumber(), log2bpc, psWS)) != nullptr)
        return err;

    const Sprite::Pixel maxChunkWidth = Sprite::Pixel(256 >> log2bpc);
    const Sprite::Pixel maxChunkHeight = 64;

    // Match sprite_commonoutput and the ARM order: columns right-to-left,
    // and within each column, chunks from top to bottom.
    for (Sprite::Pixel right = sourceRect.x1; right > sourceRect.x0; ) {
        Sprite::Pixel width = right - sourceRect.x0;
        if (width > maxChunkWidth)
            width = maxChunkWidth;

        Sprite::Pixel left = right - width;

        for (Sprite::Pixel top = sourceRect.y1; top > sourceRect.y0; ) {
            Sprite::Pixel height = top - sourceRect.y0;
            if (height > maxChunkHeight)
                height = maxChunkHeight;

            Sprite::Pixel bottom = top - height;
            Sprite::PixelRect chunk(left, bottom, right, top);

            err = sprite_maskchunk(sprite,
                                   chunk, spriteInfo.mode().toNumber(),
                                   spriteInfo.hasMask(),
                                   job);
            if (err)
                return err;

            top = bottom;
        }

        right = left;
    }

    return nullptr;
}

static MyError sprite_put(const Sprite::Selector& sprite,
                          const Point<OS::Unit>& point,
                          Sprite::PlotAction gcolAction,
                          const Sprite::ScaleFactor *scale,
                          const ColourTrans::Table* table,
                          JobWS& job)
{
    MyError err;
    CoreWS& ws = CoreWS::instance();
    Output& output(job.output());
    bool useMask = !!(gcolAction & Sprite::PlotAction_UseMask);
    Sprite::PlotAction action =
        Sprite::PlotAction(gcolAction & Sprite::PlotAction_GCOLMask);

    const Sprite::ScaleFactor* scale_factors = nullptr;

    ScopedPassthrough scopedPT(ws.m_interceptMgr, Passthrough_Sprite);

#if PSCoordSpeedUps
    if ((err = ensure_OScoords(output, job)) != nullptr)
        return err;
#endif
    if ((err = output_gsave(job)) != nullptr)
        return err;

    if ((err = sprite_translateby(point.x.raw(), point.y.raw(), output)) != nullptr)
        return err;

    scale_factors = scale;
    ColourMapping mapping(table);

    if ((err = sprite_checkR5bit4(sprite, gcolAction, mapping)) != nullptr)
        return err;

    Sprite::Info spriteInfo;
    if ((err = spriteInfo.get(sprite)) != nullptr)
        return err;

    const Sprite::PixelRect sourceRect(0, 0, spriteInfo.width(), spriteInfo.height());

    if (spriteInfo.width() == 0 || spriteInfo.height() == 0)
        return output_grestore(job);

    if (!spriteInfo.hasMask())
        useMask = false;

    if ((gcolAction & Sprite::PlotAction_GCOLMask) != OS::GCOLAction_Src)
        return output_grestore(job);

#if Medusa
    {
        uint32_t log2bpp;
        if ((err = OS::xreadModeVariable(spriteInfo.mode().toNumber(),
                                         OS::ModeVar_Log2BPP, log2bpp)) != nullptr)
            return err;

        if (log2bpp == 4u || log2bpp == 5u) {
            // It's a 16bpp or 32bpp sprite.

            if (job.info.isColour()) {
                err = sprite_scaling(spriteInfo.mode().toNumber(),
                                     scale_factors, job, nullptr);
                if (err)
                    return err;
                if (log2bpp == 5u) {
                    err = sprite_output32bpp(sprite, spriteInfo, sourceRect,
                                             useMask, job);
                } else {
                    err = sprite_output16bpp(sprite, spriteInfo, sourceRect,
                                             useMask, job);
                }

                {
                    MyError grestore_err = output_grestore(job);
                    if (!err) {
                        err = grestore_err;
                    }
                    if (err)
                        return err;

                }

                return nullptr;
            }

            // If output is destined for a monochrome printer, fudge the sprite mode
            Sprite::ModeWord modeWord(spriteInfo.mode());

            if ((err = mapping.makeTable(modeWord)) != nullptr)
                return err;

            Sprite::ModeWord greyMode(medusa_same_dpi_g256_mode(modeWord));

            spriteInfo.setMode(greyMode);
        }
    }
#endif

    uint32_t log2bpc = 0;
    if ((err = sprite_scaling(spriteInfo.mode(), scale_factors, job, &log2bpc)) != nullptr)
        return err;

    if (useMask) {
        action = Sprite::PlotAction(action | Sprite::PlotAction_UseMask);
    }

    err = sprite_commonoutput(sprite, spriteInfo,
                              sourceRect,
                              mapping, action, log2bpc,
                              job);
    if (err)
        return err;

    return output_grestore(job);

    // `sprite_put_cleanup`
    // colourTrans32K will be auto-freed.
    // scopedPT will be auto-freed.
}

MyError sprite_mask(const Sprite::Selector& sprite,
                    const Point<OS::Unit>& point,
                    uint32_t gcol_action,
                    const Sprite::ScaleFactor *scale,
                    const ColourTrans::Table* table,
                    JobWS& job)
{
    MyError err;

    CoreWS& ws = CoreWS::instance();
    (void)gcol_action;
    (void)table;
    Output& output(job.output());
    uint32_t log2bpc = 0;

    Sprite::Info spriteInfo;

    ScopedPassthrough scopedPT(ws.m_interceptMgr, Passthrough_Sprite);

#if PSCoordSpeedUps
    if ((err = ensure_OScoords(output, job)) != nullptr)
        return err;
    if ((err = colour_ensure(job)) != nullptr)
        return err;
#endif
    if ((err = output_gsave(job)) != nullptr)
        return err;

    if ((err = sprite_translateby(point.x.raw(), point.y.raw(), output)) != nullptr)
        return err;

    if ((err = spriteInfo.get(sprite)) != nullptr)
        return err;

    if (spriteInfo.width() == 0 || spriteInfo.height() == 0)
        return output_grestore(job);

    if ((err = sprite_scaling(spriteInfo.mode().toNumber(),
                              scale, job, &log2bpc)) != nullptr)
        return err;

    const Sprite::PixelRect sourceRect(0, 0, spriteInfo.width(), spriteInfo.height());
    if ((err = sprite_mask_commonoutput(sprite, spriteInfo,
                                        sourceRect,
                                        log2bpc, job)) != nullptr)
        return err;

    return output_grestore(job);

    // `sprite_mask_cleanup`
    // scopedPT will be auto-freed.
}

MyError sprite_put(SpritePlotBlock& block,
                   const Point<OS::Unit>& point,
                   const Sprite::ScaleFactor* scale,
                   CoreJobWS& coreJob)
{
    (void)block.reason;
    return sprite_put(block.sprite, point, block.plotAction,
                      scale, block.colTable, toJobWS(coreJob));
}

MyError sprite_mask(SpritePlotBlock& block,
                    const Point<OS::Unit>& point,
                    const Sprite::ScaleFactor* scale,
                    CoreJobWS& coreJob)
{
    (void)block.reason;
    return sprite_mask(block.sprite, point, Sprite::PlotAction(OS::GCOLAction_Src),
                       scale, nullptr, toJobWS(coreJob));
}
