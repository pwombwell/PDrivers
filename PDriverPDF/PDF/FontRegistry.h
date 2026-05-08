#ifndef PDRIVERPDF_PDF_FONTREGISTRY_H
#define PDRIVERPDF_PDF_FONTREGISTRY_H

#include <stdint.h>

namespace PDF {

enum FontEncoding
{
    FontEncoding_WinAnsi,
    FontEncoding_BuiltIn
};

class FontRecord;

class FontRegistry
{
public:
    FontRegistry();
    ~FontRegistry();

    void addHead(FontRecord* font);
    FontRecord* find(const char* fontName, FontEncoding encoding) const;
    uint32_t newResourceId();

    FontRecord* head() const { return m_head; }

private:
    FontRecord* m_head;
    uint32_t    m_nextResourceId;
};

} // namespace PDF

#endif
