#include "FontRegistry.h"

#include "FontRecord.h"

#include <string.h>

namespace PDF {

FontRegistry::FontRegistry()
{
    m_head = nullptr;
    m_nextResourceId = 1;
}

FontRegistry::~FontRegistry()
{
    FontRecord* font = m_head;
    m_head = nullptr;

    while (font != nullptr) {
        FontRecord* next = font->next();
        delete font;
        font = next;
    }
}

void FontRegistry::addHead(FontRecord* font)
{
    font->m_next = m_head;
    m_head = font;
}

FontRecord* FontRegistry::find(const char* fontName, FontEncoding encoding) const
{
    FontRecord* font = m_head;
    while (font != nullptr) {
        if (font->encoding() == encoding && strcmp(font->name(), fontName) == 0)
            return font;

        font = font->next();
    }

    return nullptr;
}

uint32_t FontRegistry::newResourceId()
{
    uint32_t resourceId = m_nextResourceId;
    m_nextResourceId += 1;
    return resourceId;
}

} // namespace PDF
