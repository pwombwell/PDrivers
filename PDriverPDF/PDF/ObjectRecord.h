#pragma once

namespace PDF {

class ObjectRecord
{
public:
    ObjectRecord(ObjectId objectId) : m_objectId(objectId) {}

    ObjectId objectId() const { return m_objectId; }

private:
    ObjectId m_objectId;
};

} // namespace PDF
