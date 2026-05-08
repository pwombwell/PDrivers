#pragma once

#include "ObjectId.h"

namespace PDF {

class PageRef
{
public:
    PageRef() {}
    PageRef(PageObjectId objectId) : m_objectId(objectId) {}

    PageObjectId objectId() const { return m_objectId; }
    void setObjectId(PageObjectId objectId) { m_objectId = objectId; }

private:
    PageObjectId m_objectId;
};

} // namespace PDF
