#include "RLEncode.h"

#include "Output.h"

#include "Common/Ascii85.h"
#include "Core/ErrorBlocks.h"

#include "RLib/OS/CharInput.h"

inline MyError checkEscape() {
    if (OS::readEscapeState())
        return ErrorBlock_Escape;

    return nullptr;
}

// `sprite_outputbyte` and `sprite_outputbyte_L2`
MyError RLEncode::outputByte(uint8_t value)
{
    if (m_count != 0 && value == m_last) {
        m_count++;
        return nullptr;
    }

    MyError err = flush();
    if (err)
        return err;

    m_last = value;
    m_count = 1;

    return nullptr;
}

MyError RLEncode::flush()
{
    MyError err;

    uint32_t total = m_count + m_len;

    if (m_level1) {
        if (total <= PSSprRLMaxStr && m_count <= (PSSprRLMinRun + 1))
            return extend();

        if ((err = outputStringL1()) != nullptr)
            return err;

        if (m_count < PSSprRLMinRun)
            return extend();

        if ((err = outputRunL1()) != nullptr)
            return err;
    } else {
        if (total <= PSSprRLMaxStrL2 && m_count <= (PSSprRLMinRunL2 + 1))
            return extend();

        if ((err = outputStringL2()) != nullptr)
            return err;

        if (m_count < PSSprRLMinRunL2)
            return extend();

        if ((err = outputRunL2()) != nullptr)
            return err;
    }

    // m_count is zero to zero by outputRunLx().

    return nullptr;
}

// `sprite_outputbyte_extendandstartrun`
// `sprite_outputbyte_L2_extendandstartrun`
nullptr_t RLEncode::extend()
{
    while (m_count > 0) {
        m_buffer[m_len++] = m_last;
        m_count--;
    }

    return nullptr;
}

// `sprite_endoutput`
MyError RLEncode::end()
{
    MyError err;

    // Equivalent to sprite_endoutput's fake &100 byte, except we do not
    // actually start a new fake run afterwards (and so types can be uint8_t).
    if ((err = flush()) != nullptr)
        return err;

    if (m_level1)
        return outputStringL1();

    // level >= 2
    if ((err = outputStringL2()) != nullptr)
        return err;
    if ((err = m_ascii85.output(128)) != nullptr)
        return err;

    return m_ascii85.end();
}

// Level 1 ---------------------------------------------------------------------
// `sprite_outputstring`
MyError RLEncode::outputStringL1()
{
    MyError err;

    if (m_len == 0)
        return nullptr;

    if ((err = m_output.numSpace(m_len)) != nullptr)
        return err;

    const uint8_t* str = m_buffer;
#if PSSprRLMaxStr > 32
    uint32_t line_count = 33;
#endif

    for (size_t i = 0; i < m_len; ++i) {
#if PSSprRLMaxStr > 32
        if (--line_count == 0) {
            if ((err = m_output.str('\n')) != nullptr)
                return err;
            if (OS::readEscapeState())
                return ErrorBlock_Escape;

            line_count = 32;
        }
#endif
        if ((err = m_output.hex(str[i])) != nullptr)
            return err;
    }

    if ((err = m_output.str('\n')) != nullptr)
        return err;

    if (OS::readEscapeState())
        return ErrorBlock_Escape;

    m_len = 0;

    return nullptr;
}

// `sprite_outputrun`
// Subroutine to output the repeat count held in R3, negated, then a space,
// then the byte value held in R2, then a new line. Preserves all registers.
MyError RLEncode::outputRunL1()
{
    MyError err;

    if ((err = m_output.numSpace(-(int32_t)m_count)) != nullptr)
        return err;
    if ((err = m_output.num((int32_t)m_last)) != nullptr)
        return err;
    if ((err = m_output.str('\n')) != nullptr)
        return err;

    return checkEscape();
}

// Level 2 ---------------------------------------------------------------------
// `sprite_outputstring_L2`
MyError RLEncode::outputStringL2()
{
    if (m_len == 0)
        return nullptr;

    MyError err;

    if ((err = m_ascii85.output((m_len - 1))) != nullptr)
        return err;

    if ((err = m_ascii85.output(m_buffer, m_len)) != nullptr)
        return err;

    m_len = 0;

    return nullptr;
}


// `sprite_outputrun_L2`
// Caller must not pass runs shorter than PSSprRLMinRunL2.
MyError RLEncode::outputRunL2()
{
    MyError err;

    // PS RLE lengths are <= 128, so loop to ensure all of count is emitted.
    while (m_count > 0) {
        uint32_t run = m_count > 128 ? 128 : m_count;
        uint8_t len_byte = (uint8_t)(257 - run);

        if ((err = m_ascii85.output(len_byte)) != nullptr)
            return err;
        if ((err = m_ascii85.output(m_last)) != nullptr)
            return err;

        m_count -= run;
    }

    return checkEscape();
}
