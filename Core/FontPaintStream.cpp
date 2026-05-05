#include "PDriver.h"
#include "FontPaintStream.h"

#include "Constants.h"

#include <string.h>

void FontPaintStream::setFlags(FontPaintFlag flags)
{
    m_flags = flags;

#if UCSText
    switch (flags & (FontPaintFlag_16bit | FontPaintFlag_32bit)) {
    case FontPaintFlag_16bit:
        m_width = Width16;
        break;

    case FontPaintFlag_32bit:
        m_width = Width32;
        break;

    default:
        m_width = Width8;
        break;
    }
#else
    m_width = Width8;
#endif
}

uint32_t FontPaintStream::peekUnsigned(uint32_t* lengthOut) const
{
    if (is32Bit()) {
        if (lengthOut != nullptr)
            *lengthOut = 4;

        return *(const uint32_t*)m_current;
    }

    if (is16Bit()) {
        if (lengthOut != nullptr)
            *lengthOut = 2;

        return *(const uint16_t*)m_current;
    }

    if (lengthOut != nullptr)
        *lengthOut = 1;

    return m_current[0];
}

uint32_t FontPaintStream::readUnsigned()
{
    uint32_t length = 0;
    uint32_t value = peekUnsigned(&length);
    advance(length);
    return value;
}

uint32_t FontPaintStream::readUnsignedWithLength(uint32_t* lengthOut)
{
    uint32_t length = 0;
    uint32_t value = peekUnsigned(&length);
    advance(length);

    if (lengthOut != nullptr)
        *lengthOut = length;

    return value;
}

// `readnextchar`
uint32_t FontPaintStream::nextChar(bool isUtf8, uint32_t* lengthOut)
{
    if (!isUtf8)
        return nextCharRaw(lengthOut);

    if (is32Bit())
        return nextCharUCS4(lengthOut);

    if (is16Bit())
        return nextCharUTF16(lengthOut);

    return nextCharUTF8(lengthOut);
}

uint32_t FontPaintStream::nextCharRaw(uint32_t* lengthOut)
{
    return readUnsignedWithLength(lengthOut);
}

uint32_t FontPaintStream::nextCharUCS4(uint32_t* lengthOut)
{
    uint32_t ch = *(const uint32_t*)m_current;
    advance(4);

    if (lengthOut != nullptr)
        *lengthOut = 4;

    return ch;
}

uint32_t FontPaintStream::nextCharUTF16(uint32_t* lengthOut)
{
    uint32_t ch = *(const uint16_t*)m_current;
    advance(2);

    if (ch < 0xD800u || ch >= 0xE000u) {
        if (lengthOut != nullptr)
            *lengthOut = 2;
        return ch;
    }

    if (ch >= 0xDC00u) {
        if (lengthOut != nullptr)
            *lengthOut = 2;
        return 0xFFFDu;
    }

    uint32_t next = *(const uint16_t*)m_current;
    if (next < 0xDC00u || next >= 0xE000u) {
        if (lengthOut != nullptr)
            *lengthOut = 2;
        return 0xFFFDu;
    }

    advance(2);

    if (lengthOut != nullptr)
        *lengthOut = 4;

    return 0x10000u + (((ch - 0xD800u) << 10) | (next - 0xDC00u));
}

uint32_t FontPaintStream::nextCharUTF8(uint32_t* lengthOut)
{
    uint32_t ch = m_current[0];
    advance(1);

    if (ch <= 0x7Fu) {
        if (lengthOut != nullptr)
            *lengthOut = 1;
        return ch;
    }

    if ((ch & 0x40u) == 0u || ch >= 0xFEu) {
        if (lengthOut != nullptr)
            *lengthOut = 1;
        return 0xFFFDu;
    }

    uint32_t length = 1;
    uint32_t minValue = 0x80u;
    uint32_t controlBit = 0x800u;
    uint32_t code = ch & 0x3Fu;

    for (;;) {
        uint32_t cont = m_current[0];
        if ((cont & 0xC0u) != 0x80u) {
            if (lengthOut != nullptr)
                *lengthOut = length;
            return 0xFFFDu;
        }

        advance(1);
        length += 1;

        code = (code << 6) | (cont & 0x3Fu);
        if ((code & 0x80000000u) != 0u) {
            if (lengthOut != nullptr)
                *lengthOut = length;
            return 0xFFFDu;
        }

        if ((code & controlBit) != 0u) {
            code &= ~controlBit;
            minValue = controlBit;
            controlBit <<= 5;
            continue;
        }

        if (code < minValue) {
            if (lengthOut != nullptr)
                *lengthOut = length;
            return 0xFFFDu;
        }

        if (lengthOut != nullptr)
            *lengthOut = length;

        return code;
    }
}

uint32_t FontPaintStream::printableLength(bool isUtf8) const
{
    FontPaintStream scan(*this);
    const uint8_t* start = scan.current();

    while (!scan.atEnd()) {
        const uint8_t* before = scan.current();
        uint32_t ch = scan.nextChar(isUtf8);
        if (ch < 32u)
            return (uint32_t)(before - start);
    }

    return (uint32_t)(scan.current() - start);
}

int32_t FontPaintStream::readSigned()
{
    int32_t value = 0;

    if (is32Bit()) {
        value = *(const int32_t*)m_current;
        advance(4);
        return value;
    }

    if (is16Bit()) {
        value = *(const int16_t*)m_current;
        advance(2);
        return value;
    }

    value = (int32_t)(int8_t)m_current[0];
    advance(1);
    return value;
}

int32_t FontPaintStream::readDelta()
{
    int32_t value = 0;

    if (is32Bit()) {
        value = *(const int32_t*)m_current;
        advance(4);
        return value;
    }

    if (is16Bit()) {
        const uint16_t* words = (const uint16_t*)m_current;
        value = (int32_t)(((uint32_t)words[0]) |
                          ((uint32_t)words[1] << 16));
        advance(4);
        return value;
    }

    value = (int32_t)(((uint32_t)m_current[0]) |
                      ((uint32_t)m_current[1] << 8) |
                      ((uint32_t)m_current[2] << 16));
    if ((value & 0x00800000u) != 0u)
        value |= (int32_t)0xFF000000u;

    advance(3);
    return value;
}

// https://www.riscosopen.org/wiki/documentation/show/Font_Paint%20Special%20Characters%20Parameters
uint32_t FontPaintStream::readColour()
{
    if (is32Bit()) {
        uint32_t value = *(const uint32_t*)m_current;
        advance(4);
        return value;
    }

    if (is16Bit()) {
        const uint16_t* words = (const uint16_t*)m_current;
        uint32_t red00 = ((uint32_t)words[0]) << 8;
        uint32_t blueGreen = ((uint32_t)words[1]) << 16;
        advance(4);
        return blueGreen | red00;
    }

    uint32_t red = *m_current++;
    uint32_t green = *m_current++;
    uint32_t blue = *m_current++;
    return (blue << 24) | (green << 16) | (red << 8);
}

Draw::Matrix FontPaintStream::readMatrix(bool hasTranslateTerms)
{
    uintptr_t aligned = (uintptr_t(m_current) + 3) & ~3;
    int32_t* ptr = (int32_t*)aligned;

    if (hasTranslateTerms)
        return Draw::Matrix::fromRaw6(ptr);
    else
        return Draw::Matrix::fromRaw4(ptr);
}
