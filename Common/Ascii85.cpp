#include "Core/PDriver.h"

#include "Ascii85.h"
#include "OutputBuffer.h"
#include "JobWS.h"

static const uint32_t Ascii85Pow4 = 85u * 85u * 85u * 85u;
static const uint32_t Ascii85Pow3 = 85u * 85u * 85u;
static const uint32_t Ascii85Pow2 = 85u * 85u;

Ascii85::Ascii85(OutputBuffer& output)
    : m_output(output)
    , m_pendingBytes(0)
    , m_numPendingBytes(0)
{
}

MyError Ascii85::begin()
{
    return m_output.flush();
}

// `ascii85_encode`
MyError Ascii85::encode(uint32_t value, uint32_t count)
{
    if (value == 0 && count == 4) {
        m_output.append('z');
    } else {
        uint32_t v = value;

        m_output.append('!' + uint8_t(v / Ascii85Pow4));
        v %= Ascii85Pow4;

        if (count >= 1) {
            m_output.append('!' + uint8_t(v / Ascii85Pow3));
            v %= Ascii85Pow3;
        }
        if (count >= 2) {
            m_output.append('!' + uint8_t(v / Ascii85Pow2));
            v %= Ascii85Pow2;
        }
        if (count >= 3) {
            m_output.append('!' + uint8_t(v / 85));
            v %= 85;
        }
        if (count >= 4) {
            m_output.append('!' + uint8_t(v));
        }
    }

    if (m_output.col() < 80)
        return nullptr;

    // Wrap around 80-cols.
    m_output.append('\n');

    return m_output.flush();
}

MyError Ascii85::output(const uint8_t* data, uint32_t length)
{
    MyError err;

    for (uint32_t i = 0; i < length; ++i) {
        m_pendingBytes = (m_pendingBytes << 8) | data[i];
        m_numPendingBytes++;
        
        if (m_numPendingBytes == 4) {
            if ((err = encode(m_pendingBytes, 4)) != nullptr)
                return err;

            m_pendingBytes = 0;
            m_numPendingBytes = 0;
        }
    }

    return nullptr;
}

MyError Ascii85::output(uint8_t value)
{
    MyError err;

    m_pendingBytes = (m_pendingBytes << 8) | value;
    m_numPendingBytes++;

    if (m_numPendingBytes == 4) {
        if ((err = encode(m_pendingBytes, 4)) != nullptr)
            return err;

        m_pendingBytes = 0;
        m_numPendingBytes = 0;
    }

    return nullptr;
}

MyError Ascii85::end()
{
    MyError err;

    if (m_numPendingBytes != 0) {
        if (m_numPendingBytes < 4)
            m_pendingBytes <<= (4 - m_numPendingBytes) * 8;

        if ((err = encode(m_pendingBytes, m_numPendingBytes)) != nullptr)
            return err;
    }

    m_output.append('~');
    m_output.append('>');
    m_output.append('\n');

    m_numPendingBytes = 0;
    m_pendingBytes = 0;

    return m_output.flush();
}
