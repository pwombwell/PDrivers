#ifndef COMMON_ASCII85_H
#define COMMON_ASCII85_H

#include "Core/PDriver.h"

class OutputBuffer;

class Ascii85
{
public:
    Ascii85(OutputBuffer& output);

    // Flushes any pending BPUT data from the shared buffer.
    MyError begin();

    // Output a block of data as PostScript ASCII85 encoded data.
    MyError output(const uint8_t* data, uint32_t length);

    // Output a single byte as PostScript ASCII85 encoded data.  
    MyError output(uint8_t c);

    // Flush and output end of data for ASCII85.
    MyError end();

private:
    MyError encode(uint32_t value, uint32_t count);

private:
    OutputBuffer&   m_output;

    // ASCII85 encodes four bytes at a time. They are stored in m_pendingBytes.
    // m_numPendingBytes stores how many bytes are pending.
    uint32_t        m_pendingBytes;
    uint8_t         m_numPendingBytes;
};

#endif
