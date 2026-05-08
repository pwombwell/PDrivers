#include "Core/PDriver.h"

#include "Document.h"
#include "FontRecord.h"
#include "PageRef.h"

#include "GlobalWS.h"

#include "Core/MsgCode.h"
#include "Core/OS.h"
#include "Core/Workspace.h"

#include <string.h>

namespace PDF {

Document::Document()
    : m_pagesRootObject(PagesRootObjectId(m_objectTable.newObject()))
    , m_catalogObject(CatalogObjectId(m_objectTable.newObject()))
    , m_resources(m_objectTable)
{}

Document::~Document()
{}

MyError Document::createNewPage()
{
    m_currentPage.beginNewPage(m_objectTable);

    if (!m_pageRefs.append(PageRef(m_currentPage.pageObject())))
        return MyError::OOM();

    return nullptr;
}

ImageRecord* Document::registerImage(const uint8_t* jpeg, uint32_t length,
                                     const JPEG::Info& info)
{
    ImageRecord* imageRecord;

    imageRecord = new ImageRecord(newObject(), jpeg, length, info);
    if (!imageRecord || !imageRecord->init()) {
        delete imageRecord;
        return nullptr;
    }

    m_resources.addImage(imageRecord);
    return imageRecord;
}

ImageRecord* Document::registerImage(uint32_t width, uint32_t height,
                                     ImageColourSpace colourSpace,
                                     uint32_t bitsPerComponent,
                                     const uint8_t* data, uint32_t length,
                                     const uint8_t* palette,
                                     uint32_t paletteLength,
                                     bool imageMask,
                                     ObjectId mask,
                                     ObjectId softMask)
{
    ImageRecord* imageRecord;

    imageRecord = new ImageRecord(newObject(),
                                  width,
                                  height,
                                  colourSpace,
                                  bitsPerComponent,
                                  data,
                                  length,
                                  palette,
                                  paletteLength,
                                  imageMask,
                                  mask,
                                  softMask);
    if (imageRecord == nullptr || !imageRecord->init()) {
        delete imageRecord;
        return nullptr;
    }

    m_resources.addImage(imageRecord);
    return imageRecord;
}

MyError Document::registerFont(const char* fontName,
                                        FontEncoding encoding,
                                        FontRecord*& fontRecord)
{
    fontRecord = m_resources.findFont(fontName, encoding);
    if (fontRecord != nullptr)
        return nullptr;

    fontRecord = new FontRecord(newObject(),
                                m_resources.newFontResourceId(),
                                encoding,
                                fontName);
    if (fontRecord == nullptr || !fontRecord->init()) {
        delete fontRecord;
        fontRecord = nullptr;
        return MyError::OOM();
    }

    m_resources.addFont(fontRecord);
    return nullptr;
}

} // namespace PDF
