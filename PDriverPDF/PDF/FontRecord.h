#pragma once

#include "FontRegistry.h"
#include "ObjectId.h"
#include "ObjectRecord.h"

#include <stdint.h>

namespace PDF {

class FontRecord : public ObjectRecord
{
    friend class FontRegistry;

public:
    FontRecord(ObjectId objectId,
               uint32_t resourceId,
               FontEncoding encoding,
               const char* name);
    ~FontRecord();

    bool init();

    FontRecord* next() const { return m_next; }

    uint32_t resourceId() const { return m_resourceId; }

    FontEncoding encoding() const { return m_encoding; }
    bool usesWinAnsiEncoding() const { return m_encoding == FontEncoding_WinAnsi; }

    char* name() { return m_name; }
    const char* name() const { return m_name; }

private:
    FontRecord*  m_next;
    uint32_t     m_resourceId;
    FontEncoding m_encoding;
    const char*  m_initialName;
    char*        m_name;
};

} // namespace PDF
