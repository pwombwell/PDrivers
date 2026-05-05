#pragma once

#include "RLib/RLib.h"
#include "RLibX/OS.h"
#include "RLibX/Sprite.h"
#include "RLib/OS/Sprite.h"

namespace riscos {
namespace ColourTrans { struct Block; }
}

namespace OSSpriteOp {

typedef Sprite::PlotAction plotAction;

// Creates sprite
//
// In:      r0 - areaValue
//          r1 - area
//          r2 - spriteName
//          r3 - createPalette
//          r4 - width
//          r5 - height
//          r6 - mode
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0xF

MyError createSprite(Sprite::AreaValue areaValue, Sprite::Area* area, char const* spriteName, bool createPalette, int width, int height, OS::Mode mode);

// Selects sprite
//
// In:      r0 - areaValue
//          r1 - area
//          r2 - id
//
// Out:     r2 - header (X version only)
//
// Returns: r2 - header (non-X version only)
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0x18

MyError selectSprite(Sprite::AreaValue areaValue, Sprite::Area const* area, Sprite::Id id, Sprite::Header*& header);

// Creates mask
//
// In:      r0 - areaValue
//          r1 - area
//          r2 - id
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0x1D

MyError createMask(Sprite::AreaValue areaValue, Sprite::Area* area, Sprite::Id id);

// Puts sprite at user coordinates
//
// In:      r0 - areaValue
//          r1 - area
//          r2 - id
//          r3 - x
//          r4 - y
//          r5 - action
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0x22

MyError putSpriteUserCoords(Sprite::AreaValue areaValue, Sprite::Area const* area, Sprite::Id id, int x, int y, OS::GCOLAction action);

// Reads palette size
//
// In:      r0 - areaValue
//          r1 - area
//          r2 - id
//
// Out:     r3 - size
//          r4 - palette
//          r5 - mode
//
// Calls SWI OS_SpriteOp (0x2E) with R3 = -1, R0 = 0x25

MyError readPaletteSize(Sprite::AreaValue areaValue, Sprite::Area const* area, Sprite::Id id, int& size, OS::PaletteBase*& palette, OS::Mode& mode);

// Reads sprite information
//
// In:      r0 - areaValue
//          r1 - area
//          r2 - id
//
// Out:     r3 - width
//          r4 - height
//          r5 - mask
//          r6 - mode
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0x28

MyError readSpriteInfo(Sprite::AreaValue areaValue, Sprite::Area const* area, Sprite::Id id,
                       int& width, int& height, bool& mask, OS::Mode& mode);

// Reads VDU save area size
//
// In:      r0 - areaValue
//          r1 - area
//          r2 - id
//
// Out:     r2 - saveAreaBytes
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0x3E

MyError readSaveAreaSize(Sprite::AreaValue areaValue, Sprite::Area const* area,
                         Sprite::Id id, int& saveAreaBytes);

// Plots mask scaled
//
// In:      r0 - areaValue
//          r1 - area
//          r2 - id
//          r3 - x
//          r4 - y
//          r6 - factors
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0x32

MyError plotMaskScaled(Sprite::AreaValue areaValue,
                       Sprite::Area const* area,
                       Sprite::Id id,
                       int x, int y,
                       Sprite::ScaleFactor const* factors);

// Puts sprite scaled
//
// In:      r0 - areaValue
//          r1 - area
//          r2 - id
//          r3 - x
//          r4 - y
//          r5 - action
//          r6 - factors
//          r7 - transTab
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0x34

MyError putSpriteScaled(Sprite::AreaValue areaValue,
                        Sprite::Area const* area,
                        Sprite::Id id,
                        int x, int y,
                        OS::GCOLAction action,
                        Sprite::ScaleFactor const* factors,
                        const riscos::ColourTrans::Table* colTrans);

// Switches output to sprite
//
// In:      r0 - areaValue
//          r1 - area
//          r2 - id
//          r3 - saveArea
//
// Out:     r0 - context0
//          r1 - context1
//          r2 - context2
//          r3 - context3
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0x3C

MyError switchOutputToSprite(Sprite::AreaValue areaValue, Sprite::Area const* area, Sprite::Id id, Sprite::VDUSaveArea* saveArea, int& context0, int& context1, int& context2, int& context3);

// Switches output to mask
//
// In:      r0 - areaValue
//          r1 - area
//          r2 - id
//          r3 - saveArea
//
// Out:     r0 - context0
//          r1 - context1
//          r2 - context2
//          r3 - context3
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0x3D

MyError switchOutputToMask(Sprite::AreaValue areaValue, Sprite::Area const* area, Sprite::Id id, Sprite::VDUSaveArea* saveArea, int& context0, int& context1, int& context2, int& context3);

// Removes left-hand wastage from sprite
//
// In:      r0 - areaValue
//          r1 - area
//          r2 - id
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0x36

MyError removeLeftHandWastage(Sprite::AreaValue areaValue,
                              Sprite::Area const* area,
                              Sprite::Id id);

} // namespace OSSpriteOp
