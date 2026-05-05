#pragma once

#include "RLib/Types/OS.h"
#include "RLib/Types/OSMode.h"
#include "RLib/Types/ColourTrans.h"

namespace ColourTrans {

// Sets up a translation table in a buffer
//
// In:      r0 - sourceMode
//          r1 - sourcePalette
//          r2 - destMode
//          r3 - destPalette
//          r4 - transTab
//          r5 - flags
//          r6 - workspace
//          r7 - transferFn
//
// Out:     r4 - size (X version only)
//
// Returns: r4 - size (non-X version only)
//
// Calls SWI ColourTrans_GenerateTable (0x40763)

static MyError generateTable(OS::Mode sourceMode, const OS::Palette* sourcePalette, OS::Mode destMode, const OS::Palette* destPalette, ColourTrans::Table* transTab, TableFlags flags, void* workspace, void* transferFn, int& size);

// Sets the closest GCOL for a palette entry
//
// In:      r0 - colour
//          r1 - mode
//          r2 - destPalette
//
// Out:     r0 - gcol (X version only)
//
// Returns: r0 - gcol (non-X version only)
//
// Calls SWI ColourTrans_ReturnGCOLForMode (0x40745)

static MyError returnGCOLForMode(OS::Colour colour, OS::Mode mode, OS::Palette const* destPalette, OS::GCOL& gcol);

// Informs ColourTrans that the palette has been changed by some other means
//
// Calls SWI ColourTrans_InvalidateCache (0x40750)

static MyError invalidateCache(void);


} // namespace ColourTrans
