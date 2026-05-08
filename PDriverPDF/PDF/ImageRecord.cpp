#include "ImageRecord.h"

#include <string.h>

namespace PDF {

ImageRecord::ImageRecord(ObjectId objectId,
                         const uint8_t* jpeg, uint32_t length,
                         const JPEG::Info& info)
: ObjectRecord(objectId)
, m_info(info)
, m_colourSpace(info.monochrome() ? PDF::ImageColourSpace_Gray :
                PDF::ImageColourSpace_Rgb)
, m_filter(ImageFilter_DCT)
, m_width(info.width())
, m_height(info.height())
, m_bitsPerComponent(8)
, m_imageMask(false)
, m_mask()
, m_softMask()
, m_initialJpeg(jpeg)
, m_initialPalette(nullptr)
, m_data(nullptr)
, m_length(length)
, m_palette(nullptr)
, m_paletteLength(0)
, m_next(nullptr)
{
}

ImageRecord::ImageRecord(ObjectId objectId,
                         uint32_t width, uint32_t height,
                         ImageColourSpace colourSpace,
                         uint32_t bitsPerComponent,
                         const uint8_t* data, uint32_t length,
                         const uint8_t* palette, uint32_t paletteLength,
                         bool imageMask,
                         ObjectId mask,
                         ObjectId softMask)
: ObjectRecord(objectId)
, m_info()
, m_colourSpace(colourSpace)
, m_filter(ImageFilter_None)
, m_width(width)
, m_height(height)
, m_bitsPerComponent(bitsPerComponent)
, m_imageMask(imageMask)
, m_mask(mask)
, m_softMask(softMask)
, m_initialJpeg(data)
, m_initialPalette(palette)
, m_data(nullptr)
, m_length(length)
, m_palette(nullptr)
, m_paletteLength(paletteLength)
, m_next(nullptr)
{
}

ImageRecord::~ImageRecord()
{
    delete[] m_data;
    delete[] m_palette;
}

bool ImageRecord::init()
{
    if (m_length == 0) {
        m_initialJpeg = nullptr;
        return true;
    }

    if (m_data != nullptr)
        return true;

    m_data = new uint8_t[m_length];
    if (m_data == nullptr)
        return false;

    memcpy(m_data, m_initialJpeg, m_length);
    m_initialJpeg = nullptr;

    if (m_paletteLength != 0 && m_palette == nullptr) {
        m_palette = new uint8_t[m_paletteLength];
        if (m_palette == nullptr)
            return false;

        memcpy(m_palette, m_initialPalette, m_paletteLength);
        m_initialPalette = nullptr;
    }

    return true;
}

} // namespace PDF
