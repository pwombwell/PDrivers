#ifndef PDRIVERPDF_PDF_DOCUMENT_H
#define PDRIVERPDF_PDF_DOCUMENT_H

#include "ImageRecord.h"
#include "ImageRegistry.h"
#include "ObjectTable.h"
#include "Page.h"
#include "PageRef.h"
#include "Resources.h"

#include "RLibX/Utils/Vector.h"

namespace PDF {

class Document
{
public:
    Document();
    ~Document();

    MyError createNewPage();

    ObjectTable& objectTable() { return m_objectTable; }
    const ObjectTable& objectTable() const { return m_objectTable; }

    PagesRootObjectId pagesRootObject() const { return m_pagesRootObject; }
    CatalogObjectId catalogObject() const { return m_catalogObject; }

    Page& currentPage() { return m_currentPage; }
    const Page& currentPage() const { return m_currentPage; }

    uint32_t pageCount() const { return m_pageRefs.size(); }
    const PageRef& pageAt(uint32_t index) const { return m_pageRefs[index]; }

    ResourcesObjectId resourcesObject() const { return m_resources.resourcesObject(); }

    ImageRecord* firstImage() const { return m_resources.firstImage(); }
    FontRecord* firstFont() const { return m_resources.firstFont(); }

    ImageRecord* registerImage(const uint8_t* jpeg, uint32_t length,
                               const JPEG::Info& info);
    ImageRecord* registerImage(uint32_t width, uint32_t height,
                               ImageColourSpace colourSpace,
                               uint32_t bitsPerComponent,
                               const uint8_t* data, uint32_t length,
                               const uint8_t* palette = nullptr,
                               uint32_t paletteLength = 0,
                               bool imageMask = false,
                               ObjectId mask = ObjectId(),
                               ObjectId softMask = ObjectId());
    MyError registerFont(const char* fontName,
                         FontEncoding encoding,
                         FontRecord*& fontRecord);

private:
    ObjectId newObject() { return m_objectTable.newObject(); }

    ObjectTable       m_objectTable;
    PagesRootObjectId m_pagesRootObject;
    CatalogObjectId   m_catalogObject;
    RLib::Vector<PageRef> m_pageRefs;
    Page              m_currentPage;
    Resources         m_resources;
};

} // namespace PDF

#endif
