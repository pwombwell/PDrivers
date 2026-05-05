#pragma once

#include "RLib/RLib.h"
#include "RLib/Geometry/Point.h"
#include "RLib/Geometry/Rect.h"
#include "RLib/Geometry/Offset.h"
#include "RLibX/OS.h"

namespace riscos {
namespace Font {

using namespace riscos::Geometry;

MyError xreadScaleFactor(int32_t& x, int32_t& y);
MyError xcurrentFont(FontHandle& font, uint32_t& bg, uint32_t& fg, uint32_t& offset);

MyError convertToPoints(OS::Unit x, OS::Unit y,
                        OS::Millipoint& xOut, OS::Millipoint& yOut);

MyError convertToPoints(Point<OS::Unit> p, Point<OS::Millipoint>& pOut);
MyError convertToPoints(Offset<OS::Unit> p, Offset<OS::Millipoint>& pOut);

// https://www.riscosopen.org/wiki/documentation/show/Font_ScanString%20Block
struct ScanStringBlock {
    Offset<OS::Millipoint>  space;
    Offset<OS::Millipoint>  letter;
    int32_t                 split;  // -1 for none
    Rect<OS::Millipoint>    bbox;   // returned, if bit 18 set.
};

MyError scanString(FontHandle handle,
                   const uint8_t* str,
                   uint32_t flags,
                   Offset<OS::Millipoint>& offset,
                   const ScanStringBlock& coordBlock,
                   uint32_t length,
                   uint32_t* splitOut);

MyError scanString(FontHandle handle,
                   const uint8_t* str,
                   uint32_t flags,
                   Offset<OS::Millipoint>& offset,
                   uint32_t length,
                   uint32_t* splitOut);

MyError xsetFont(FontHandle font);
MyError xlistFonts(char* buffer, int32_t* context, int32_t path);

} } // namespace riscos::Font
