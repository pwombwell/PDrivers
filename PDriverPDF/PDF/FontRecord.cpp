#include "FontRecord.h"

#include "string.h"

namespace PDF {

FontRecord::FontRecord(ObjectId objectId,
                       uint32_t resourceId,
                       FontEncoding encoding,
                       const char* name) :
    ObjectRecord(objectId),
    m_next(nullptr),
    m_resourceId(resourceId),
    m_encoding(encoding),
    m_initialName(name),
    m_name(nullptr)
{
}

FontRecord::~FontRecord()
{
    delete[] m_name;
}

bool FontRecord::init()
{
    if (m_name != nullptr)
        return true;

    size_t nameLength = strlen(m_initialName);
    m_name = new char[nameLength + 1];
    if (m_name == nullptr)
        return false;

    memcpy(m_name, m_initialName, nameLength + 1);
    m_initialName = nullptr;
    return true;
}

} // namespace PDF
