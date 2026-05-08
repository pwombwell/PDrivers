#pragma once

#include "RLib/RLib.h"
#include "RLib/Geometry/Point.h"
#include "RLib/Geometry/Rect.h"
#include "RLib/Geometry/Offset.h"
#include "RLibX/OS.h"

namespace riscos {
namespace Font {

typedef Geometry::Offset<OS::Millipoint> Offset;
typedef Geometry::Point<OS::Millipoint> Point;
typedef Geometry::Rect<OS::Millipoint> Rect;
typedef Geometry::Size<OS::Millipoint> Size;

MyError xreadScaleFactor(int32_t& x, int32_t& y);
MyError xcurrentFont(FontHandle& font, uint32_t& bg, uint32_t& fg, uint32_t& offset);

MyError convertToPoints(OS::Unit x, OS::Unit y,
                        OS::Millipoint& xOut, OS::Millipoint& yOut);

MyError convertToPoints(Geometry::Point<OS::Unit> p, Font::Point& pOut);
MyError convertToPoints(Geometry::Offset<OS::Unit> p, Font::Offset& pOut);

enum PaintFlag /* : uint32_t */ {
    PaintFlag_None          =      0,
    PaintFlag_Justify       =  1u<<0,
    PaintFlag_Rubout        =  1u<<1,
    PaintFlag_Mpoint        =  1u<<4,
    PaintFlag_CoordsBlk     =  1u<<5,
    PaintFlag_Matrix        =  1u<<6,
    PaintFlag_Length        =  1u<<7,
    PaintFlag_UseHandle     =  1u<<8,
    PaintFlag_Kern          =  1u<<9,
    PaintFlag_Reversed      = 1u<<10,
    PaintFlag_Blended       = 1u<<11,
    PaintFlag_16bit         = 1u<<12,
    PaintFlag_32bit         = 1u<<13,
    PaintFlag_Blendsupr     = 1u<<14
};
// DEFINE_ENUM_BITWISE_OPERATORS(PaintFlag); // Norcroft should allow this here.

// https://www.riscosopen.org/wiki/documentation/show/Font_Paint%20Block
struct PaintBlock {
    Font::Offset  space;
    Font::Offset  letter;
    Font::Rect    bbox;   // returned, if bit 18 set.
};

enum ScanStringFlag {
    ScanStringFlag_None         = 0,
    // Haven't bothered to define all of yet - use a cast

    ScanStringFlag_HasBlock     =  1u<<5, // Has a ScanStringBlock.
    ScanStringFlag_Matrix       =  1u<<6, // Has a transformation matrix.
    ScanStringFlag_Length       =  1u<<7, // String length is provided.
    ScanStringFlag_UseHandle    =  1u<<8, // R0 is initial font handle.
    ScanStringFlag_Kern         =  1u<<9, // Perform kerning.
    ScanStringFlag_Reversed     = 1u<<10, // Writing dir is right to left.

    ScanStringFlag_16bit        = 1u<<12, // Mutually exclusive with 32bit.
    ScanStringFlag_32bit        = 1u<<13, // Mutually exclusive with 32bit.

    ScanStringFlag_ReturnBBox   = 1u<<18, // Return bbox in ScanStringBlock.
    ScanStringFlag_Split        = 1u<<20  // Return number of split chars.
};
// DEFINE_ENUM_BITWISE_OPERATORS(ScanStringFlag); // Norcroft should allow this

// https://www.riscosopen.org/wiki/documentation/show/Font_ScanString%20Block
struct ScanStringBlock {
    Font::Offset  space;
    Font::Offset  letter;
    int32_t       split;  // -1 for none
    Font::Rect    bbox;   // returned, if bit 18 set.
};

// Infers flags based on params (Length & UseHandle)
MyError scanString(FontHandle handle, const uint8_t* str, size_t len,
                   Font::Offset& offset);

// Infers flags based on params (Length, UseHandle & Kerning)
MyError scanStringWithKerning(FontHandle handle, const uint8_t* str, size_t len,
                              Font::Offset& offset);

MyError scanString(FontHandle handle, const uint8_t* str, size_t len,
                   ScanStringFlag flags, const ScanStringBlock& coordBlock,
                   Font::Offset& offset);

MyError scanString(FontHandle handle, const uint8_t* str, size_t len,
                   ScanStringFlag flags,
                   Font::Offset& offset);

MyError scanString(FontHandle handle, const uint8_t* str, size_t len,
                   ScanStringFlag flags, const ScanStringBlock& coordBlock,
                   Font::Offset& offset, uint32_t& splitCount);

MyError xsetFont(FontHandle font);
MyError xlistFonts(char* buffer, int32_t* context, int32_t path);

MyError currentFont(FontHandle& font);

} } // namespace riscos::Font

// These shouldn't really be here - ought to work in the namespace.
// At least, they work in clang.
DEFINE_ENUM_BITWISE_OPERATORS(Font::PaintFlag);
DEFINE_ENUM_BITWISE_OPERATORS(Font::ScanStringFlag);
