#include "OutputBuffer.h"

#include "Core/OS.h"

#include "RLib/OS/Convert.h"
#include "RLibX/OSGBPB.h"

MyError OutputBuffer::flush()
{
    if (m_col == 0)
        return nullptr;

    uint8_t count = m_col;
    m_col = 0;

    uint32_t unwritten;
    return OSGBPB::xwrite(m_file, m_bytes, count, unwritten);
}

// `output_immstring`
// An immediate string in Acorn ARM code is one that immediately follows the
// instruction. The macro then jumps to PC + ceil(strlen).
MyError OutputBuffer::str(const char* s)
{
    MyError err = nullptr;

    while (*s && !err)
        err = bput(*s++);

    return err;
}

MyError OutputBuffer::str(const char* s, size_t len)
{
    MyError err = nullptr;
    const char* end = s + len;

    while (s < end && !err)
        err = bput(*s++);

    return err;
}

MyError OutputBuffer::num(int8_t value)
{
    return num(int32_t(value));
}

MyError OutputBuffer::num(uint8_t value)
{
    return num(uint32_t(value));
}

// `output_number`
MyError OutputBuffer::num(int32_t value)
{
    char out[12]; // "-2147483648\0"
    uint32_t len;

    MyError err = OS::binaryToDecimal(value, out, sizeof(out), len);
    if (err)
        return err;

    return str(out, len);
}

MyError OutputBuffer::num(uint32_t value)
{
    char out[12]; // "4294967295\0"
    uint32_t len = sizeof(out);

    MyError err = OS::convertCardinal4(value, out, len);
    if (err)
        return err;

    return str(out);
}

MyError OutputBuffer::num(OS::Millipoint v)
{
    return num(v.raw());
}

MyError OutputBuffer::num(Draw::Unit value)
{
    int32_t v = value.raw();
    int sign = 0;
    if (v < 0) {
        sign = 1;
        v = -v;
    }

    int32_t integer = v / 256;
    int32_t rem = v % 256;

    int32_t frac = 0;
    if (rem != 0) {
        frac = (rem * 10000 + (256 / 2)) / 256;
        if (frac == 10000) {
            frac = 0;
            integer += 1;
        }
    }

    MyError err = nullptr;
    if (sign) {
        if ((err = byte('-')) != nullptr)
            return err;
    }

    if ((err = num(int32_t(integer))) != nullptr)
        return err;

    if (frac != 0) {
        char buf[6];
        buf[0] = '.';
        buf[1] = (char)('0' + (frac / 1000) % 10);
        buf[2] = (char)('0' + (frac / 100) % 10);
        buf[3] = (char)('0' + (frac / 10) % 10);
        buf[4] = (char)('0' + (frac % 10));
        buf[5] = '\0';

        int end = 4;
        while (end > 1 && buf[end] == '0') {
            buf[end] = '\0';
            --end;
        }

        if ((err = str(buf)) != nullptr)
            return err;
    }

    return nullptr;
}

// `output_hexbyte`
MyError OutputBuffer::hex(uint8_t value)
{
    static const char hexdigits[] = "0123456789abcdef";
    char buf[2];

    buf[0] = hexdigits[(value >> 4) & 0xF];
    buf[1] = hexdigits[value & 0xF];

    return str(buf, 2);
}

// `output_numberspace`
MyError OutputBuffer::numSpace(int32_t v)
{
    MyError err = num(v);
    if (err)
        return err;

    return bput(' ');
}

MyError OutputBuffer::numSpace(OS::Unit v)
{
    return numSpace(v.raw());
}

MyError OutputBuffer::numSpace(OS::Millipoint v)
{
    return numSpace(v.raw());
}

MyError OutputBuffer::numSpace(Draw::Unit value)
{
    MyError err = num(value);
    if (err)
        return err;

    return bput(' ');
}

// `output_string`
MyError OutputBuffer::str(const char* s, uint8_t min, uint8_t max)
{
    MyError err;
    const uint8_t range = max - min;
    while (*s) {
        char c = *s++;
        if (uint8_t(c) - min > range)
            return nullptr;

        if ((err = bput(c)) != nullptr)
            return err;
    }

    return nullptr;
}

// `output_variable`
#if 0
MyError OutputBuffer::write(const char* s)
{
    // Needs expansion buffer... CoreWS::instance() or passed in?
}
#endif

// `output_character`
// `bput`
MyError OutputBuffer::bput(uint8_t b)
{
    append(b);
    m_written++;

    if (b != '\n' && col() < 90)
        return nullptr;

    return flush();
}
