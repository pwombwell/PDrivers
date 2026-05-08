#ifndef PDRIVERPDF_PDF_PAGE_H
#define PDRIVERPDF_PDF_PAGE_H

#include "ObjectId.h"

namespace PDF {

class ObjectTable;

class Page
{
public:
    Page();

    void beginNewPage(ObjectTable& objectTable);

    PageObjectId pageObject() const { return m_pageObject; }
    ContentsObjectId contentsObject() const { return m_contentsObject; }
    LengthObjectId lengthObject() const { return m_lengthObject; }
    size_t streamLength() const { return m_streamLength; }

    void beginStream(size_t fileOffset);
    void closeStream(size_t fileOffset);
    void endStream();

    bool inPage() const { return m_inPage; }

private:
    PageObjectId     m_pageObject;
    ContentsObjectId m_contentsObject;
    LengthObjectId   m_lengthObject;
    size_t           m_streamStartOffset;
    size_t           m_streamLength;
    bool             m_inPage;
};

} // namespace PDF

#endif
