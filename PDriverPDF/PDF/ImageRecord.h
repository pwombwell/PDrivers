#pragma once

#include "ObjectId.h"
#include "ObjectRecord.h"

#include <stdint.h>

namespace PDF {

enum ImageColourSpace
{
    ImageColourSpace_Rgb,
    ImageColourSpace_Gray,
    ImageColourSpace_Indexed
};

enum ImageFilter
{
    ImageFilter_None,
    ImageFilter_DCT
};

class ImageRecord : public ObjectRecord
{
    friend class ImageRegistry;

public:
    ImageRecord(ObjectId objectId,
                const uint8_t* jpeg, uint32_t length,
                const JPEG::Info& info);
    ImageRecord(ObjectId objectId,
                uint32_t width, uint32_t height,
                ImageColourSpace colourSpace,
                uint32_t bitsPerComponent,
                const uint8_t* data, uint32_t length,
                const uint8_t* palette, uint32_t paletteLength,
                bool imageMask,
                ObjectId mask,
                ObjectId softMask);
    ~ImageRecord();

    bool init();

    ImageRecord* next() const { return m_next; }

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }

    uint32_t dpiX() const { return m_info.dpiX(); }
    uint32_t dpiY() const { return m_info.dpiY(); }

    uint32_t length() const { return m_length; }

    ImageColourSpace colourSpace() const { return m_colourSpace; }
    ImageFilter filter() const { return m_filter; }
    uint32_t bitsPerComponent() const { return m_bitsPerComponent; }
    bool isImageMask() const { return m_imageMask; }
    ObjectId mask() const { return m_mask; }
    ObjectId softMask() const { return m_softMask; }

    const uint8_t* palette() const { return m_palette; }
    uint32_t paletteLength() const { return m_paletteLength; }
    uint32_t paletteEntries() const { return m_paletteLength / 3; }

    uint8_t* data() { return m_data; }
    const uint8_t* data() const { return m_data; }

private:
    const JPEG::Info m_info;
    ImageColourSpace m_colourSpace;
    ImageFilter m_filter;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_bitsPerComponent;
    bool     m_imageMask;
    ObjectId m_mask;
    ObjectId m_softMask;

    const uint8_t*   m_initialJpeg;
    const uint8_t*   m_initialPalette;

    uint8_t*         m_data;
    uint32_t         m_length;
    uint8_t*         m_palette;
    uint32_t         m_paletteLength;

    ImageRecord*     m_next;
};

} // namespace PDF
