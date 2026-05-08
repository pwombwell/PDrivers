#include "ObjectTable.h"

namespace PDF {

ObjectTable::ObjectTable()
    : m_nextObject(1)
{}

ObjectId ObjectTable::newObject()
{
    ObjectId objectId(m_nextObject);
    m_nextObject += 1;
    return objectId;
}

bool ObjectTable::setObjectOffset(ObjectId objectId, size_t offset)
{
    // Object IDs are 1-based, so index 0 is unused. The objectId is likely
    // to be the next slot, but it may not be - populate gaps with 0.
    while (m_offsets.size() < objectId.value()) {
        if (!m_offsets.append(0))
            return false;
    }

    if (m_offsets.size() == objectId.value())
        return m_offsets.append(offset);

    m_offsets[objectId.value()] = offset;

    return true;
}

} // namespace PDF
