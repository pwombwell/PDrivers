#include "ImageBuilder.h"

#include "JobWS.h"

#include "Common/ColTrans.h"

#include "RLib/OS.h"

// Not really correct enough for RLib.
static uint32_t getBPP(Sprite::Type type)
{
    switch (type) {
        case Sprite::Type_Bpp1:      return 1;
        case Sprite::Type_Bpp2:      return 2;
        case Sprite::Type_Bpp4:      return 4;
        case Sprite::Type_Bpp8:      return 8;
        case Sprite::Type_Bpp16:
        case Sprite::Type_Bpp16_565: return 16;
        case Sprite::Type_Bpp32:     return 32;
        case Sprite::Type_Packed24:  return 24;
        default:                     return 0;
    }
}

static uint32_t ror(uint32_t value, uint32_t shift) {
    shift &= 31u;
    if (shift == 0u) {
        return value;
    }
    return (value >> shift) | (value << (32u - shift));
}

static inline void storeRGB(uint32_t bbggrr00, uint8_t* out)
{
    out[0] = (uint8_t)((bbggrr00 >> 8) & 0xFFu);
    out[1] = (uint8_t)((bbggrr00 >> 16) & 0xFFu);
    out[2] = (uint8_t)((bbggrr00 >> 24) & 0xFFu);
}

static uint32_t lookupRGB(const JobWS& job,
                          uint32_t pixval,
                          const ColourMapping& mapping)
{
    switch (mapping.kind) {
        case ColourMapping::None:
            return pixval_lookup(pixval);

        case ColourMapping::Palette:
            return mapping.palette[pixval].on;

        case ColourMapping::GeneratedGreyTable: {
            // SpriteOp has already plotted through the generated 32K table into
            // an 8bpp temporary sprite, so the pixel value is itself a grey.
            uint32_t grey = pixval & 0xFFu;
            return (grey << 16) | (grey << 8) | grey;
        }

        case ColourMapping::ColourTrans: {
            uint8_t value = mapping.byteTable()[pixval];

            if (job.info.isColour()) {
                const ColourJob& colourJob(toColourJob(job));
                return colourJob.lookupPixelValue(value);
            }

            return (uint32_t(value) << 24) |
                   (uint32_t(value) << 16) |
                   (uint32_t(value) << 8);
        }
    }

    return pixval_lookup(pixval);
}

// ----------------------------------------------------------------------------
namespace PDF {

MyError ImageBuilder::init()
{
    MyError err;
    if ((err = OS::xreadModeVariable(m_hdr.mode(),
                                     OS::ModeVar_Log2BPP,
                                     m_log2bpp)) != nullptr)
    {
        return err;
    }
    if ((err = OS::xreadModeVariable(m_hdr.mode(),
                                     OS::ModeVar_ModeFlags,
                                     m_modeFlags)) != nullptr)
    {
        return err;
    }
    if ((err = OS::xreadModeVariable(m_hdr.mode(),
                                     OS::ModeVar_NColour,
                                     m_nColour)) != nullptr)
    {
        return err;
    }

    m_type = Sprite::ModeWord(m_hdr.mode()).isModeNumber()
           ? Sprite::Type_Old
           : Sprite::ModeWord(m_hdr.mode()).type();

    m_bitsPerPixel = getBPP(m_type);
    if (m_bitsPerPixel == 0) {
        if (m_log2bpp <= 5u)
            m_bitsPerPixel = 1u << m_log2bpp;
        else if (m_log2bpp == 6u)
            m_bitsPerPixel = 24;
    }

    if (isPaletted() && m_hdr.hasPalette()) {
        Sprite::Palette palette = m_hdr.palette();
        if (palette.size() >= (1u << m_bitsPerPixel))
            m_spritePalette = palette;
    }

    return nullptr;
}

uint32_t ImageBuilder::lookupRGB(uint32_t pixval) const
{
    if (m_mapping.kind == ColourMapping::None &&
        hasSpritePalette() &&
        pixval < m_spritePalette.size())
    {
        return m_spritePalette[pixval].on;
    }

    return ::lookupRGB(m_job, pixval, m_mapping);
}

ImageBuilder::Map16Fn ImageBuilder::map16Fn() const
{
    uint32_t colours = m_nColour + 1u;
    if (colours == 65536u && !!(m_modeFlags & ModeFlag_FullPalette))
        colours >>= 1;

    if (colours < 32768u)
        return sprite_map444;
    if (colours == 32768u)
        return sprite_map555;
    return sprite_map565;
}

uint32_t ImageBuilder::readPalettedPixel(const uint8_t* row,
                                                  Sprite::Pixel x) const
{
    uint32_t bitIndex = uint32_t(x) * m_bitsPerPixel;
    const uint32_t* wordPtr =
        (const uint32_t*)(row + ((bitIndex >> 5) << 2));
    uint32_t word = *wordPtr;
    if ((bitIndex & 31u) != 0u)
        word = ror(word, bitIndex & 31u);

    return word & ((1u << m_bitsPerPixel) - 1u);
}

uint32_t ImageBuilder::read16Pixel(const uint8_t* row,
                                            Sprite::Pixel x) const
{
    const uint8_t* ptr = row + (uint32_t(x) << 1);
    ptr = (const uint8_t*)((uintptr_t)ptr & ~3u);
    uint32_t word = *(const uint32_t*)ptr;
    if ((uint32_t(x) & 1u) != 0u)
        word >>= 16;

    int rgb = (m_modeFlags & ModeFlag_DataFormatSub_RGB) != 0u;
    return map16Fn()(word & 0xffffu, rgb);
}

uint32_t ImageBuilder::read24Pixel(const uint8_t* row,
                                            Sprite::Pixel x) const
{
    const uint8_t* ptr = row + uint32_t(x) * 3u;
    if ((m_modeFlags & ModeFlag_DataFormatSub_RGB) != 0u)
        return (uint32_t(ptr[2]) << 16) | (uint32_t(ptr[1]) << 8) | ptr[0];

    return (uint32_t(ptr[0]) << 16) | (uint32_t(ptr[1]) << 8) | ptr[2];
}

uint32_t ImageBuilder::read32Pixel(const uint8_t* row,
                                            Sprite::Pixel x,
                                            uint8_t* alpha) const
{
    const uint8_t* ptr = row + (uint32_t(x) << 2);
    uint32_t pixel = *(const uint32_t*)ptr;

    if (alpha != nullptr)
        *alpha = (uint8_t)((pixel >> 24) & 0xffu);

    if ((m_modeFlags & ModeFlag_DataFormatSub_RGB) != 0u) {
        uint32_t lr = pixel << 8;
        pixel &= 0x0000FF00u;
        pixel |= ror(lr, 24u);
    }

    return pixel;
}

bool ImageBuilder::readMaskPixel(const uint8_t* row,
                                          Sprite::Pixel x)
{
    const uint32_t* wordPtr = (const uint32_t*)(row + ((uint32_t(x) >> 5) << 2));
    uint32_t word = *wordPtr;
    word >>= (uint32_t(x) & 31u);
    return (word & 1u) != 0u;
}

MyError ImageBuilder::buildIndexed(uint8_t*& data, uint32_t& length,
                                            uint8_t*& palette, uint32_t& paletteLength)
{
    uint32_t entries = 1u << m_bitsPerPixel;
    paletteLength = entries * 3u;
    palette = new uint8_t[paletteLength];
    if (palette == nullptr)
        return MyError::OOM();

    for (uint32_t i = 0; i < entries; ++i)
        storeRGB(lookupRGB(i), palette + i * 3u);

    uint32_t rowBytes = packedRowBytes(width(), m_bitsPerPixel);
    length = rowBytes * uint32_t(height());
    data = new uint8_t[length];
    if (data == nullptr)
        return MyError::OOM();

    uint8_t* out = data;
    for (Sprite::Pixel y = 0; y < height(); ++y) {
        const uint8_t* image = imageRow(m_sourceRect.y0 + y);
        uint8_t outByte = 0;
        uint32_t outBits = 0;

        for (Sprite::Pixel x = 0; x < width(); ++x) {
            uint32_t pixval = readPalettedPixel(image, m_sourceRect.x0 + x);
            outByte |= (uint8_t)(pixval << (8u - m_bitsPerPixel - outBits));
            outBits += m_bitsPerPixel;
            if (outBits == 8u) {
                *out++ = outByte;
                outByte = 0;
                outBits = 0;
            }
        }

        if (outBits != 0u)
            *out++ = outByte;
    }

    return nullptr;
}

MyError ImageBuilder::buildRGB(uint8_t*& data, uint32_t& length)
{
    length = uint32_t(width()) * uint32_t(height()) * 3u;
    data = new uint8_t[length];
    if (data == nullptr)
        return MyError::OOM();

    uint8_t* out = data;
    for (Sprite::Pixel y = 0; y < height(); ++y) {
        const uint8_t* image = imageRow(m_sourceRect.y0 + y);
        for (Sprite::Pixel x = 0; x < width(); ++x) {
            Sprite::Pixel sourceX = m_sourceRect.x0 + x;
            uint32_t rgb;
            if (m_bitsPerPixel <= 8)
                rgb = lookupRGB(readPalettedPixel(image, sourceX));
            else if (m_bitsPerPixel == 16)
                rgb = read16Pixel(image, sourceX);
            else if (m_bitsPerPixel == 24)
                rgb = read24Pixel(image, sourceX);
            else
                rgb = read32Pixel(image, sourceX, nullptr);

            out[0] = (uint8_t)(rgb & 0xffu);
            out[1] = (uint8_t)((rgb >> 8) & 0xffu);
            out[2] = (uint8_t)((rgb >> 16) & 0xffu);
            out += 3;
        }
    }

    return nullptr;
}

MyError ImageBuilder::buildMask(uint8_t*& data, uint32_t& length)
{
    uint32_t rowBytes = packedRowBytes(width(), 1);
    length = rowBytes * uint32_t(height());
    data = new uint8_t[length];
    if (data == nullptr)
        return MyError::OOM();

    uint8_t* out = data;
    for (Sprite::Pixel y = 0; y < height(); ++y) {
        const uint8_t* mask = maskRow(m_sourceRect.y0 + y);
        uint8_t outByte = 0;
        uint32_t outBits = 0;

        for (Sprite::Pixel x = 0; x < width(); ++x) {
            if (readMaskPixel(mask, m_sourceRect.x0 + x))
                outByte |= (uint8_t)(0x80u >> outBits);

            outBits++;
            if (outBits == 8u) {
                *out++ = outByte;
                outByte = 0;
                outBits = 0;
            }
        }

        if (outBits != 0u)
            *out++ = outByte;
    }

    return nullptr;
}

MyError ImageBuilder::buildSoftMask(uint8_t*& data, uint32_t& length)
{
    length = uint32_t(width()) * uint32_t(height());
    data = new uint8_t[length];
    if (data == nullptr)
        return MyError::OOM();

    uint8_t* out = data;
    for (Sprite::Pixel y = 0; y < height(); ++y) {
        const uint8_t* image = imageRow(m_sourceRect.y0 + y);
        for (Sprite::Pixel x = 0; x < width(); ++x) {
            uint8_t alpha = 255;
            read32Pixel(image, m_sourceRect.x0 + x, &alpha);
            *out++ = alpha;
        }
    }

    return nullptr;
}

MyError ImageBuilder::registerMaskImage(PDF::ImageRecord*& imageRecord)
{
    imageRecord = nullptr;

    if (width() <= 0 || height() <= 0)
        return nullptr;

    uint8_t* data = nullptr;
    uint32_t length = 0;
    MyError err = buildMask(data, length);
    if (err != nullptr) {
        delete[] data;
        return err;
    }

    imageRecord = m_job.m_pdfDocument.registerImage(uint32_t(width()),
                                                    uint32_t(height()),
                                                    PDF::ImageColourSpace_Gray,
                                                    1,
                                                    data,
                                                    length,
                                                    nullptr,
                                                    0,
                                                    true);
    delete[] data;
    if (imageRecord == nullptr)
        return MyError::OOM();
    return nullptr;
}

MyError ImageBuilder::registerColourImage(PDF::ImageRecord*& imageRecord)
{
    imageRecord = nullptr;

    PDF::ImageRecord* maskRecord = nullptr;
    if (m_useMask && m_hdr.maskPtr() != nullptr) {
        MyError err = registerMaskImage(maskRecord);
        if (err != nullptr)
            return err;
    }

    PDF::ImageRecord* softMaskRecord = nullptr;
    if (hasAlpha()) {
        uint8_t* softMask = nullptr;
        uint32_t softMaskLength = 0;
        MyError err = buildSoftMask(softMask, softMaskLength);
        if (err != nullptr) {
            delete[] softMask;
            return err;
        }

        softMaskRecord = m_job.m_pdfDocument.registerImage(uint32_t(width()),
                                                           uint32_t(height()),
                                                           PDF::ImageColourSpace_Gray,
                                                           8,
                                                           softMask,
                                                           softMaskLength);
        delete[] softMask;
        if (softMaskRecord == nullptr)
            return MyError::OOM();
    }

    uint8_t* data = nullptr;
    uint32_t length = 0;
    uint8_t* palette = nullptr;
    uint32_t paletteLength = 0;
    PDF::ImageColourSpace colourSpace = PDF::ImageColourSpace_Rgb;
    uint32_t bitsPerComponent = 8;

    MyError err;
    if (isPaletted()) {
        colourSpace = PDF::ImageColourSpace_Indexed;
        bitsPerComponent = m_bitsPerPixel;
        err = buildIndexed(data, length, palette, paletteLength);
    } else {
        err = buildRGB(data, length);
    }
    if (err != nullptr) {
        delete[] data;
        delete[] palette;
        return err;
    }

    PDF::ObjectId maskObject;
    if (maskRecord != nullptr)
        maskObject = maskRecord->objectId();

    PDF::ObjectId softMaskObject;
    if (softMaskRecord != nullptr)
        softMaskObject = softMaskRecord->objectId();

    imageRecord = m_job.m_pdfDocument.registerImage(uint32_t(width()),
                                                    uint32_t(height()),
                                                    colourSpace,
                                                    bitsPerComponent,
                                                    data,
                                                    length,
                                                    palette,
                                                    paletteLength,
                                                    false,
                                                    maskObject,
                                                    softMaskObject);
    delete[] data;
    delete[] palette;
    if (imageRecord == nullptr)
        return MyError::OOM();
    return nullptr;
}

} // namespace PDF