#ifndef RLIBX_FONT_READDEFN_H
#define RLIBX_FONT_READDEFN_H

#include "RLib/RLib.h"
#include "RLib/Draw.h"

#include "Identifier.h"

namespace riscos { namespace Font {

class ReadDefn {
public:
    ReadDefn();
    ~ReadDefn();

    MyError init(FontHandle handle);

    Identifier getIdentifier() { return Identifier(m_buffer); }
    Draw::Unit pointsX() const { return m_pointsX; }
    Draw::Unit pointsY() const { return m_pointsY; }

private:
    char*       m_buffer;

    Draw::Unit  m_pointsX;
    Draw::Unit  m_pointsY;
    uint32_t    m_dpiX;
    uint32_t    m_dpiY;
    uint32_t    m_age;
    uint32_t    m_usageCount;
};

} } // namespace riscos::Font

#endif
