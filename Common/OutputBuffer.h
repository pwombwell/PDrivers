#ifndef COMMON_OUTPUTBUFFER_H
#define COMMON_OUTPUTBUFFER_H

#include "RLib/RLib.h"
#include "RLib/Draw.h"
#include "RLibX/OS.h"

// Shared buffer for ASCII85 output and BPUT.
class OutputBuffer
{
public:
    OutputBuffer(FileHandle file) : m_file(file), m_col(0), m_written(0) { }

    // Outputs a literal character (aliases: can't have 'char' as fn name).
    MyError byte(uint8_t b) { return bput(b); }

    // Output as text, from either a char or string.
    MyError str(char c) { return bput(uint8_t(c)); }    // `output_character`
    MyError str(const char* s);                         // `output_immstring`
    MyError str(const char* s, size_t len);
    // Output 's', terminated by a character not between min/max (inclusive).
    MyError str(const char *s, uint8_t min, uint8_t max); // `output_string`

    // Outputs as decimal text.
    MyError num(int8_t v);       // `output_number`
    MyError num(uint8_t v);
    MyError num(int32_t v);       // `output_number`
    MyError num(uint32_t v);
    MyError num(OS::Millipoint v);
    MyError num(Draw::Unit v);

    // Outputs as decimal text followed by a space.
    MyError numSpace(int32_t v);  // `output_numberspace`
    MyError numSpace(OS::Unit v);
    MyError numSpace(OS::Millipoint v);
    MyError numSpace(Draw::Unit v);

    // Outputs a two digit hexadecimal number, as text.
    MyError hex(uint8_t value); // `output_hexbyte`

//    MyError var(const char* v); // `output_variable`

    MyError flush();
    uint8_t col() const { return m_col; }
    size_t written() const { return m_written; }
    void append(uint8_t b) { m_bytes[m_col++] = b; }

protected:
    FileHandle file() const { return m_file; }
    uint8_t* bytes() { return m_bytes; }

private:
    MyError bput(uint8_t b);   //`bput`

private:
    const FileHandle    m_file;

    uint8_t             m_bytes[93];
    uint8_t             m_col;

    size_t              m_written;
};

#endif
