#include "Page.h"

#include "ObjectTable.h"

namespace PDF {

Page::Page()
    : m_streamStartOffset(0)
    , m_streamLength(0)
    , m_inPage(false)
{}

void Page::beginNewPage(ObjectTable& objectTable)
{
    m_pageObject = PageObjectId(objectTable.newObject());
    m_contentsObject = ContentsObjectId(objectTable.newObject());
    m_lengthObject = LengthObjectId(objectTable.newObject());
    m_streamStartOffset = 0;
    m_streamLength = 0;
    m_inPage = false;
}

void Page::beginStream(size_t fileOffset)
{
    m_streamStartOffset = fileOffset;
    m_streamLength = 0;
    m_inPage = true;
}

void Page::closeStream(size_t fileOffset)
{
    m_streamLength = fileOffset - m_streamStartOffset;
}

void Page::endStream()
{
    m_inPage = false;
}

} // namespace PDF
