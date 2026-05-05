#ifndef PDRIVERPS_RLENCODE_H
#define PDRIVERPS_RLENCODE_H

#include "PDriverPS.h"

#include "Common/Ascii85.h"

#include "RLib/RLib.h"

#include <stdint.h>

#if PSSprRLEncode
class Ascii85;
class OutputBuffer;

class RLEncode {
public:
    RLEncode(OutputBuffer& output, bool level1)
        : m_output(output)
        , m_ascii85(output)
        , m_level1(level1)
        , m_len(0)
        , m_count(0)
    {}

    // Flush bput before any ascii85`.
    MyError begin() { if (!m_level1) return m_ascii85.begin(); return nullptr; }

    // `sprite_outputbyte`
    // `sprite_outputbyte_L2`
    MyError outputByte(uint8_t b);

    MyError end();

private:
    // `sprite_outputbyte_extendandstartrun` and `sprite_outputbyte_L2_extendandstartrun`
    nullptr_t extend();

    // `sprite_outputbyte`
    MyError flush();

    // Level 1 -----------------------------------------------------------------
    // `sprite_outputstring`
    MyError outputStringL1();

    // `sprite_outputrun`
    MyError outputRunL1();

    // Level 2 -----------------------------------------------------------------
    // `sprite_outputstring_L2`
    MyError outputStringL2();

    // `sprite_outputrun_L2`
    MyError outputRunL2();

private:
    OutputBuffer&   m_output;
    Ascii85         m_ascii85;

    bool            m_level1;
    uint8_t         m_len;                      // `sprstringlen`
    uint8_t         m_last;                     // `sprlastbyte`
    uint32_t        m_count;                    // `sprrepeatcount`
    uint8_t         m_buffer[PSSprRLMaxStr];    // `sprstring`
};

#endif

#endif
