#ifndef PDRIVERPDF_PDF_RESOURCES_H
#define PDRIVERPDF_PDF_RESOURCES_H

#include "FontRegistry.h"
#include "ImageRegistry.h"
#include "ObjectId.h"

namespace PDF {

class ImageRecord;
class FontRecord;
class ObjectTable;

class Resources
{
public:
    Resources(ObjectTable& objectTable);

    ResourcesObjectId resourcesObject() const { return m_resourcesObject; }

    ImageRecord* firstImage() const { return m_images.head(); }
    FontRecord* firstFont() const { return m_fonts.head(); }

    void addImage(ImageRecord* image) { m_images.addHead(image); }
    void addFont(FontRecord* font) { m_fonts.addHead(font); }
    FontRecord* findFont(const char* fontName, FontEncoding encoding) const
    {
        return m_fonts.find(fontName, encoding);
    }
    uint32_t newFontResourceId() { return m_fonts.newResourceId(); }

private:
    ResourcesObjectId m_resourcesObject;
    FontRegistry      m_fonts;
    ImageRegistry     m_images;
};

} // namespace PDF

#endif
