#pragma once

#include "RLib/ColourTrans.h"
#include "RLib/RLib.h"
#include "RLib/OS/Colour.h"
#include "RLib/OS/Mode.h"
#include "RLibX/Utils/EnumBitmaskOps.h"

namespace riscos {
namespace ColourTrans {

using riscos::OS::Mode;
using namespace riscos::ColourTrans;

enum TableFlags {
    TableFlags_GivenSprite        =  1u<<0,
    TableFlags_CurrentIfAbsent    =  1u<<1,
    TableFlags_GivenTransferFn    =  1u<<2,
    TableFlags_ReturnGCOLList     =  1u<<3,
    TableFlags_ReturnWideEntries  =  1u<<4,
    TableFlags_ReturnPaletteTable =  1u<<24
};
DEFINE_ENUM_BITWISE_OPERATORS(TableFlags);

// Sets up a translation table in a buffer
//
// In:      r0 - sourceMode
//          r1 - sourcePalette
//          r2 - destMode
//          r3 - destPalette
//          r4 - transTab
//          r5 - flags
//
// Calls SWI ColourTrans_GenerateTable (0x40763)

MyError generateTable(OS::Mode sourceMode, const OS::PaletteBase* sourcePalette, OS::Mode destMode, const OS::PaletteBase* destPalette, ColourTrans::Table* transTab, TableFlags flags);

// Gets the translation table size
//
// In:      r0 - sourceMode
//          r1 - sourcePalette
//          r2 - destMode
//          r3 - destPalette
//          r5 - flags
//
// Out:     r4 - size (X version only)
//
// Returns: r4 - size (non-X version only)
//
// Calls SWI ColourTrans_GenerateTable (0x40763) with r4 = 0.

MyError generateTableGetSize(OS::Mode sourceMode,
                                    const OS::PaletteBase* sourcePalette,
                                    OS::Mode destMode,
                                    const OS::PaletteBase* destPalette,
                                    TableFlags flags,
                                    uint32_t& size);

MyError returnGCOLForMode(uint32_t paletteEntry,
                                 uint32_t mode,
                                 uint32_t* palette,
                                 uint32_t& gcol);


} } // namespace riscos::ColourTrans
