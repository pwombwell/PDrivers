#include "kernel.h"

#include "JPEG.h"

#include "swis.h"

#ifndef JPEG_Info
#define JPEG_Info 0x00049980
#endif
#ifndef JPEG_PlotScaled
#define JPEG_PlotScaled 0x00049982
#endif
#ifndef JPEG_PDriverIntercept
#define JPEG_PDriverIntercept 0x00049986
#endif

namespace riscos {
namespace JPEG {

MyError Info::parse(const void* jpeg, uint32_t len)
{
    return _swix(JPEG_Info,
                 _INR(0, 2) | _OUT(0), _OUTR(2,6),
                 1u<<0, jpeg, len,
                 &m_flags, &m_width, &m_height,
                 &m_dpiX, m_dpiY, &m_workspace);
}

MyError infoDimensions(const void* image,
                      int size,
                      InfoFlags& infoFlags,
                      int& width,
                      int& height,
                      int& xdpi,
                      int& ydpi,
                      int& workspaceSize)
{
    return _swix(JPEG_Info, _INR(0,2) | _OUT(0) | _OUTR(2,6),
                 1, image, size,
                 &infoFlags, &width, &height, &xdpi, &ydpi, &workspaceSize);
}

MyError plotScaled(const void* image,
                  int x,
                  int y,
                  const int32_t* scale,
                  uint32_t length,
                  ScaleFlags flags)
{
    return _swix(JPEG_PlotScaled, _INR(0,5),
                 image, x, y, scale, length, flags);
}

MyError pdriverIntercept(PrintFlags flags, PrintFlags* flagsOut)
{
    PrintFlags oldFlags;
    MyError err = _swix(JPEG_PDriverIntercept, _IN(0) | _OUT(0),
                       flags, &oldFlags);
    if (err != nullptr)
        return err;

    if (flagsOut != nullptr)
        *flagsOut = oldFlags;
    return nullptr;
}

} } // namespace riscos::JPEG
