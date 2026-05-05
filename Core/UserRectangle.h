#ifndef CORE_USERRECTANGLE_H
#define CORE_USERRECTANGLE_H

#include "PDriver.h"
#include "RLibX/Utils/List.h"

/* User-specified rectangle definition (R9-relative in assembler). */
class UserRectangle {
//    template <typename T>
//    friend class riscos::Utils::List;
public:
    UserRectangle() { }
    UserRectangle(uint32_t id,
                  const Rect<OS::Unit>& box,
                  const Draw::DimensionlessTransform& transform,
                  const Point<OS::Millipoint>& bottomLeft,
                  uint32_t bgColour)
        : rectangleid(id)
        , rectanglebg(bgColour)
        , rectoffset(box.bottomLeftOffset())
        , rectbox(box.size())
        , recttransform(transform)
        , rectbottomleft(bottomLeft)
        , m_next(nullptr)
    { }

    UserRectangle* next() const { return m_next; }

    uint32_t rectangleid;
    uint32_t rectanglebg; // bbggrr00

    Offset<OS::Unit> rectoffset;
    Size<OS::Unit> rectbox;
    Draw::DimensionlessTransform recttransform;
    Point<OS::Millipoint> rectbottomleft;

public: // FIXME: Norcroft bug - can't friend namespaces
    UserRectangle* m_next;
};

typedef riscos::Utils::List<UserRectangle> UserRectangleList;

#endif
