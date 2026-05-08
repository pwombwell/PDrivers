#ifndef PDRIVERPDF_PDF_OBJECTTABLE_H
#define PDRIVERPDF_PDF_OBJECTTABLE_H

#include "ObjectId.h"

#include "RLibX/Utils/Vector.h"

namespace PDF {

class ObjectTable
{
public:
    ObjectTable();

    ObjectId newObject();

    bool setObjectOffset(ObjectId objectId, size_t offset);
    size_t objectOffset(ObjectId objectId) const { return m_offsets[objectId.value()]; }
    size_t objectCount() const { return m_nextObject - 1; }

private:
    uint32_t                m_nextObject;
    RLib::Vector<size_t>    m_offsets;
};

} // namespace PDF

#endif
