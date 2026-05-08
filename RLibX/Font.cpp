#include "kernel.h"

#include "Font.h"

#include "swis.h"

namespace riscos {
namespace Font {

MyError xreadScaleFactor(int32_t& x, int32_t& y)
{
    return _swix(Font_ReadScaleFactor, _OUTR(1,2), &x, &y);
}

MyError xcurrentFont(FontHandle& font, uint32_t& bg, uint32_t& fg, uint32_t& offset)
{
    return _swix(Font_CurrentFont, _OUTR(0,3), &font, &bg, &fg, &offset);
}

MyError Font::convertToPoints(OS::Unit x, OS::Unit y,
                        OS::Millipoint& xOut, OS::Millipoint& yOut)
{
    return _swix(Font_Converttopoints, _INR(1,2) | _OUTR(1,2),
                 x, y, &xOut, &yOut);
}

MyError Font::convertToPoints(Geometry::Point<OS::Unit> p, Font::Point& pOut)
{
    return _swix(Font_Converttopoints, _INR(1,2) | _OUTR(1,2),
                 p.x, p.y, &pOut.x, &pOut.y);
}

MyError Font::convertToPoints(Geometry::Offset<OS::Unit> p, Font::Offset& pOut)
{
    return _swix(Font_Converttopoints, _INR(1,2) | _OUTR(1,2),
                 p.dx, p.dy, &pOut.dx, &pOut.dy);
}

MyError scanString(FontHandle handle, const uint8_t* str, size_t len,
                   Font::Offset& offset)
{
    const ScanStringFlag flags = ScanStringFlag_Length | ScanStringFlag_UseHandle;
    return _swix(Font_ScanString,
                 _INR(0,4) | _IN(7) | _OUTR(3,4),
                 handle, str, flags, offset.dx, offset.dy, len,
                 &offset.dx, &offset.dy);
}

MyError scanStringWithKerning(FontHandle handle, const uint8_t* str, size_t len,
                              Font::Offset& offset)
{
    const ScanStringFlag flags = ScanStringFlag_Length |
                                 ScanStringFlag_UseHandle |
                                 ScanStringFlag_Kern;
    return _swix(Font_ScanString,
                 _INR(0,4) | _IN(7) | _OUTR(3,4),
                 handle, str, flags, offset.dx, offset.dy, len,
                 &offset.dx, &offset.dy);
}

MyError scanString(FontHandle handle, const uint8_t* str, size_t len,
                   ScanStringFlag flags, const ScanStringBlock& coordBlock,
                   Font::Offset& offset)
{
    return _swix(Font_ScanString,
                 _INR(0,5) | _IN(7) | _OUTR(3,4),
                 handle, str, flags, offset.dx, offset.dy, &coordBlock, len,
                 &offset.dx, &offset.dy);
}

MyError scanString(FontHandle handle, const uint8_t* str, size_t len,
                   ScanStringFlag flags, const ScanStringBlock& coordBlock,
                   Font::Offset& offset)
{
    return _swix(Font_ScanString,
                 _INR(0,5) | _IN(7) | _OUTR(3,4),
                 handle, str, flags, offset.dx, offset.dy, &coordBlock, len,
                 &offset.dx, &offset.dy);
}

MyError scanString(FontHandle handle, const uint8_t* str, size_t len,
                   ScanStringFlag flags, const ScanStringBlock& coordBlock,
                   Font::Offset& offset, uint32_t& splitCount)
{
    return _swix(Font_ScanString,
                 _INR(0,5) | _IN(7) | _OUTR(3,4) | _OUT(7),
                 handle, str, flags, offset.dx, offset.dy, &coordBlock, len,
                 &offset.dx, &offset.dy, &splitCount);
}

MyError xsetFont(FontHandle font)
{
    return _swix(Font_SetFont, _IN(1), font);
}

MyError xlistFonts(char* buffer, int32_t* context, int32_t path)
{
    int32_t currentContext = (context != nullptr) ? *context : 0;
    char* end;
    MyError err = _swix(Font_ListFonts, _INR(1,3) | _OUT(1) | _OUT(2),
                       buffer, currentContext, path, &end, &currentContext);
    if (err != nullptr)
        return err;

    if (context != nullptr)
        *context = currentContext;
    return nullptr;
}

MyError currentFont(FontHandle& font)
{
    return _swix(Font_CurrentFont, _OUT(0), &font);
}

} } // namespace riscos::Font
