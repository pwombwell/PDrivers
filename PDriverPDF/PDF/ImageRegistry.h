#ifndef PDRIVERPDF_PDF_IMAGEREGISTRY_H
#define PDRIVERPDF_PDF_IMAGEREGISTRY_H

#include "RLib/RLib.h"

namespace PDF {

class ImageRecord;

class ImageRegistry
{
public:
    ImageRegistry() : m_head(nullptr) {}
    ~ImageRegistry();

    void addHead(ImageRecord* image);

    ImageRecord* head() const { return m_head; }

private:
    ImageRecord* m_head;
};

} // namespace PDF

#endif
