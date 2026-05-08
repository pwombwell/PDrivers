#include "Resources.h"

#include "ObjectTable.h"

namespace PDF {

Resources::Resources(ObjectTable& objectTable)
    : m_resourcesObject(ResourcesObjectId(objectTable.newObject()))
{}

} // namespace PDF
