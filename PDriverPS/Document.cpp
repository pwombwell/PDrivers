#include "Core/PDriver.h"
#include "Document.h"

#include "Core/UserRectangle.h"

namespace PS {

FontCatalogue::~FontCatalogue()
{
    DeclaredFont* font;

    while ((font = m_declaredFonts.detachHead()) != nullptr)
        delete font;
}

Document::Document(Flag flags, Level level)
    : m_currentRect(nullptr)
    , m_flags(Flag(flags | Flag_SendPrologue))
    , m_level(level)
{
    resetBoundingBox();
}

const UserRectangle* Document::takeCurrentRect()
{
    const UserRectangle* rect = m_currentRect;
    if (rect != nullptr)
        m_currentRect = rect->next();

    return rect;
}

void Document::resetBoundingBox()
{
    m_boundingBox.setEmpty();
}

void Document::includeBoundingBox(const Rect<Unit>& bounds)
{
    if (m_boundingBox.isEmpty())
        m_boundingBox = bounds;
    else
        m_boundingBox.unionWith(bounds);
}

Rect<Unit> Document::boundingBoxOrEmpty() const
{
    if (m_boundingBox.isEmpty())
        return Rect<Unit>(0, 0, 0, 0);

    return m_boundingBox;
}

} // namespace PS
