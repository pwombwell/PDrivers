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

MyError Font::convertToPoints(Point<OS::Unit> p, Point<OS::Millipoint>& pOut)
{
    return _swix(Font_Converttopoints, _INR(1,2) | _OUTR(1,2),
                 p.x, p.y, &pOut.x, &pOut.y);
}

MyError Font::convertToPoints(Offset<OS::Unit> p, Offset<OS::Millipoint>& pOut)
{
    return _swix(Font_Converttopoints, _INR(1,2) | _OUTR(1,2),
                 p.dx, p.dy, &pOut.dx, &pOut.dy);
}

MyError scanString(FontHandle handle,
                   const uint8_t* str,
                   uint32_t flags,
                   Offset<OS::Millipoint>& offset,
                   const ScanStringBlock& coordBlock,
                   uint32_t length,
                   uint32_t* splitOut)
{
    uint32_t split;

    MyError err = _swix(Font_ScanString,
                        _INR(0,5) | _IN(7) | _OUT(3) | _OUT(4) | _OUT(7),
                        handle, str, flags, offset.dx, offset.dy, &coordBlock, length,
                        &offset.dx, &offset.dy, &split);

    if (err != nullptr)
        return err;

    if (splitOut != nullptr)
        *splitOut = split;

    return nullptr;
}

MyError scanString(FontHandle handle,
                   const uint8_t* str,
                   uint32_t flags,
                   Offset<OS::Millipoint>& offset,
                   uint32_t length,
                   uint32_t* splitOut)
{
    uint32_t split;

    MyError err = _swix(Font_ScanString,
                        _INR(0,4) | _IN(7) | _OUT(3) | _OUT(4) | _OUT(7),
                        handle, str, flags, offset.dx, offset.dy, length,
                        &offset.dx, &offset.dy, &split);

    if (err != nullptr)
        return err;

    if (splitOut != nullptr)
        *splitOut = split;

    return nullptr;
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

} } // namespace riscos::Font
