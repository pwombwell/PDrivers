#pragma once
#ifndef RLIB_OS_SPRITE_H
#define RLIB_OS_SPRITE_H

#include "Mode.h"
#include "RLib/RLib.h"
#include "RLib/Geometry/Rect.h"

#include <stdint.h>

namespace riscos {
namespace Sprite {

struct AreaView;
struct ConstAreaView;
struct HeaderView;

enum Type /*: uint16_t*/ {
    Type_Old       =  0,
    Type_Bpp1      =  1,
    Type_Bpp2      =  2,
    Type_Bpp4      =  3,
    Type_Bpp8      =  4,
    Type_Bpp16     =  5, // 1:5:5:5
    Type_Bpp32     =  6,
    Type_CMYK      =  7,
    Type_Packed24  =  8,
    Type_JPEG      =  9,
    Type_Bpp16_565 = 10,

    Type_RISCOS5   = 15
    // ...
};

// Area value to add to r0 on calls to OS_SpriteOp.
enum AreaValue {
  AreaValue_System   =     0, // [=  0] System sprite area (r1) + sprite name (r2).
  AreaValue_UserName = 1u<<8, // [=256] User sprite area (r1) + sprite name (r2).
  AreaValue_UserPtr  = 1u<<9  // [=512] User sprite area (r1) + sprite ptr (r2).
};

// RISC OS Sprite Area header (offsets relative to start of Area).
struct Area {
    // Matches memory layout.
    uint32_t size;         // total size of the sprite area.
    uint32_t spriteCount;  // number of sprites.
    uint32_t first;        // offset to first sprite (usually sizeof(Area)).
    uint32_t used;         // offset to first free word, or used size of the area.

    AreaView view();
    ConstAreaView view() const;
};

// RISC OS Sprite header (offsets relative to start of Header).
struct Header {
    uint32_t next;        // Offset to next sprite.
    char     name[12];    // Sprite name, up to 12 characters with trailing zeroes.
    uint32_t widthWordsM1;// Width in words -1.
    uint32_t heightM1;    // Height in pixels -1.
    uint32_t leftBit;     // First bit used (left end of row).
    uint32_t rightBit;    // Last bit used (right end of row).
    uint32_t imageOffset; // Offset to sprite image.
    uint32_t maskOffset;  // Offset to mask or =imageOffset if no mask.
    OS::Mode mode;        // Mode sprite was defined in.
    //... any palette follows immediately after the header.

    HeaderView view() const;
};

// RISC OS Sprite id.
// A pointer to a name, or Sprite::Header. The type must be known externally.
struct Id {
    Id() { u.m_name = nullptr; }

    Id(const char* name) { u.m_name = name; }
    Id(const Header* header) { u.m_header = header; }

    const char* name() const { return u.m_name; }
    const Header* header() const { return u.m_header; }

    union {
        const char*     m_name;
        const Header*   m_header;
    } u;
};

// Used to store the saved VDU state. See:
// https://www.riscosopen.org/wiki/documentation/show/VDU%20Save%20Area
// Allocate this after querying the size.
struct VDUSaveArea {
    void initialise() { words[0] = 0; }

private:
    uint32_t words[1];
};

// OS_SpriteOp takes source rectangles in pixels.
typedef int32_t Pixels;
typedef int32_t Pixel;
typedef Geometry::Rect<Pixel> PixelRect;

} } // namespace riscos::Sprite

#endif
