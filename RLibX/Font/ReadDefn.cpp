#include "kernel.h"

#include "ReadDefn.h"

#include "swis.h"
#include <stdlib.h>

namespace riscos { namespace Font {

ReadDefn::ReadDefn()
    : m_buffer(nullptr)
    , m_dpiX(0)
    , m_dpiY(0)
    , m_age(0)
    , m_usageCount(0)
{
}

ReadDefn::~ReadDefn()
{
    delete[] m_buffer;
}

MyError ReadDefn::init(FontHandle handle)
{
    delete[] m_buffer;
    m_buffer = nullptr;

    uint32_t size;
    MyError err = _swix(Font_ReadDefn, _INR(0,1)|_IN(3)|_OUT(2),
                        handle, 0, 0x4c4c5546, &size);
    if (err)
        return err;    

    m_buffer = new char[size];
    if (!m_buffer)
        return MyError::OOM();

    err = _swix(Font_ReadDefn, _INR(0,1)|_IN(3)|_OUTR(2,7),
                handle, m_buffer, 0x4c4c5546,
                &m_pointsX, &m_pointsY, &m_dpiX, &m_dpiY, &m_age, &m_usageCount);
    if (err)
        return err;

    return nullptr;
}

} } // namespace riscos::Font
