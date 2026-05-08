#include "ImageRegistry.h"

#include "ImageRecord.h"

namespace PDF {

ImageRegistry::~ImageRegistry()
{
    ImageRecord* image = m_head;
    m_head = nullptr;

    while (image != nullptr) {
        ImageRecord* next = image->next();
        delete image;
        image = next;
    }
}

void ImageRegistry::addHead(ImageRecord* image)
{
    image->m_next = m_head;
    m_head = image;
}

} // namespace PDF
