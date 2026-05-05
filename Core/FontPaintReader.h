#ifndef FONTPAINTREADER_H
#define FONTPAINTREADER_H

#include "PDriver.h"

#include "FontPaintStream.h"
#include "RLibX/Font/ReadDefn.h"

class FontPaintReader
{
public:
    FontPaintReader(const uint8_t* current,
                    FontPaintFlag flags,
                    const uint8_t* end = nullptr)
        : m_stream(current, flags, end)
    {}

    bool atEnd() const { return m_stream.atEnd(); }
    FontPaintStream& stream() { return m_stream; }
    const FontPaintStream& stream() const { return m_stream; }
    bool isUtf8() const { return m_encodingCache.isUtf8(); }

    MyError readNextChar(FontHandle font, uint32_t& ch);

private:
    FontPaintStream     m_stream;
    FontEncodingCache   m_encodingCache;
};

// `readnextchar`
inline MyError FontPaintReader::readNextChar(FontHandle font, uint32_t& ch)
{
    uint32_t raw = m_stream.peekUnsigned();

    // This mirrors FontSWI.s: readnextchar only inspects the font definition
    // when the raw character value is above &7F. For ASCII text, the encoding
    // is immaterial and no Font_ReadDefn call is made.
    if (raw > 0x7fu) {
        MyError err = m_encodingCache.lookup(font);
        if (err)
            return err;
    }

    ch = m_stream.nextChar(m_encodingCache.isUtf8());

    return nullptr;
}

#endif
