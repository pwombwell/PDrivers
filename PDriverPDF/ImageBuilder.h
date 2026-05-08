#ifndef PDRIVERPDF_IMAGE_BUILDER_H
#define PDRIVERPDF_IMAGE_BUILDER_H

#include "Core/Constants.h"

#include "RLib/RLib.h"
#include "RLibX/Sprite.h"

class ColourMapping;
class JobWS;

namespace PDF {
class ImageRecord;

class ImageBuilder
{
public:
    ImageBuilder(const Sprite::ResolvedSelector& sprite,
                 const Sprite::Info& spriteInfo,
                 const Sprite::PixelRect& sourceRect,
                 const ColourMapping& mapping,
                 bool useMask,
                 JobWS& job)
        : m_spriteInfo(spriteInfo)
        , m_sourceRect(sourceRect)
        , m_mapping(mapping)
        , m_useMask(useMask)
        , m_job(job)
        , m_hdr(sprite.header())
        , m_spritePalette()
        , m_log2bpp(0)
        , m_bitsPerPixel(0)
        , m_modeFlags(0)
        , m_nColour(0)
        , m_type(Sprite::Type_Old)
    {
    }

    MyError init();
    MyError registerColourImage(PDF::ImageRecord*& imageRecord);
    MyError registerMaskImage(PDF::ImageRecord*& imageRecord);

private:
    typedef uint32_t (*Map16Fn)(uint32_t, int);

    Sprite::Pixel width() const { return m_sourceRect.width(); }
    Sprite::Pixel height() const { return m_sourceRect.height(); }
    bool isPaletted() const { return m_bitsPerPixel <= 8; }
    bool hasAlpha() const {
        return m_bitsPerPixel == 32 &&
               (m_modeFlags & ModeFlag_DataFormatSub_Alpha) != 0u;
    }
    bool hasSpritePalette() const { return !m_spritePalette.isEmpty(); }

    uint32_t readPalettedPixel(const uint8_t* row, Sprite::Pixel x) const;
    uint32_t read16Pixel(const uint8_t* row, Sprite::Pixel x) const;
    uint32_t read24Pixel(const uint8_t* row, Sprite::Pixel x) const;
    uint32_t read32Pixel(const uint8_t* row, Sprite::Pixel x, uint8_t* alpha) const;
    static bool readMaskPixel(const uint8_t* row, Sprite::Pixel x);

    uint32_t lookupRGB(uint32_t pixval) const;
    Map16Fn map16Fn() const;
    MyError buildIndexed(uint8_t*& data, uint32_t& length,
                         uint8_t*& palette, uint32_t& paletteLength);
    MyError buildRGB(uint8_t*& data, uint32_t& length);
    MyError buildMask(uint8_t*& data, uint32_t& length);
    MyError buildSoftMask(uint8_t*& data, uint32_t& length);

    const uint8_t* imageRow(Sprite::Pixel y) const {
        uint32_t row = m_hdr.height() - 1u - uint32_t(y);
        return m_hdr.imagePtr() + row * m_hdr.rowBytes();
    }

    const uint8_t* maskRow(Sprite::Pixel y) const {
        uint32_t row = m_hdr.height() - 1u - uint32_t(y);
        return m_hdr.maskPtr() + row * rowBytes((Sprite::Pixel)m_spriteInfo.width());
    }

    static uint32_t rowBytes(Sprite::Pixel width) {
        return ((uint32_t(width) + 31u) >> 5) * 4u;
    }

    static uint32_t packedRowBytes(Sprite::Pixel width, uint32_t bitsPerPixel) {
        return (uint32_t(width) * bitsPerPixel + 7u) >> 3;
    }

private:
    const Sprite::Info&             m_spriteInfo;
    const Sprite::PixelRect&        m_sourceRect;
    const ColourMapping&            m_mapping;
    bool                            m_useMask;
    JobWS&                          m_job;
    const Sprite::HeaderView        m_hdr;
    Sprite::Palette                 m_spritePalette;
    uint32_t                        m_log2bpp;
    uint32_t                        m_bitsPerPixel;
    uint32_t                        m_modeFlags;
    uint32_t                        m_nColour;
    Sprite::Type                    m_type;
};

} // namespace PDF

#endif
