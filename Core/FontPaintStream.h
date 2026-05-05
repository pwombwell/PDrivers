#ifndef CORE_FONTPAINTSTREAM_H
#define CORE_FONTPAINTSTREAM_H

#include "PDriver.h"

#include "RLib/Draw.h"

/*
 * Width-sensitive Font_Paint stream reader.
 *
 * This mirrors the original ARM readuint1/readint1/readint3/readRGB helpers,
 * but keeps the pointer and format flags together.
 */
class FontPaintStream
{
public:
    FontPaintStream(const uint8_t* current,
                    FontPaintFlag flags,
                    const uint8_t* end = nullptr)
        : m_current(current)
        , m_end(end)
        , m_flags(FontPaintFlag_None)
        , m_width(Width8)
    {
        setFlags(flags);
    }

    const uint8_t* current() const { return m_current; }
    void advance(uint32_t bytes) { m_current += bytes; }

    void setFlags(FontPaintFlag flags);
    FontPaintFlag flags() const { return m_flags; }

    bool atEnd() const { return m_end != nullptr && m_current >= m_end; }

    bool is16Bit() const { return m_width == Width16; }
    bool is32Bit() const { return m_width == Width32; }

    uint32_t peekUnsigned(uint32_t* lengthOut = nullptr) const;
    uint32_t nextChar(bool isUtf8, uint32_t* lengthOut = nullptr);
    uint32_t printableLength(bool isUtf8) const;
    uint32_t readUnsigned();
    uint32_t readUnsignedWithLength(uint32_t* lengthOut);
    int32_t readSigned();
    int32_t readDelta();
    uint32_t readColour();
    Draw::Matrix readMatrix(bool hasTranslateTerms);
    OS::Millipoint readMillipoint();

private:
    enum Width {
        Width8,
        Width16,
        Width32
    };

    uint32_t nextCharRaw(uint32_t* lengthOut);
    uint32_t nextCharUTF8(uint32_t* lengthOut);
    uint32_t nextCharUTF16(uint32_t* lengthOut);
    uint32_t nextCharUCS4(uint32_t* lengthOut);

    const uint8_t* m_current;
    const uint8_t* m_end;
    FontPaintFlag  m_flags;
    Width          m_width;
};

#endif
