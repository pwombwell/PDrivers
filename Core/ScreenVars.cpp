#include "PDriver.h"
#include "ScreenVars.h"

#include "RLib/OS.h"

// Colour.s `vduvariableswanted`
static const OS::VDUVar wanted[] = {
    OS::VduExt_GPLFMD,      // GCOL action for foreground col
    OS::VduExt_GFTint,      // Graphics foreground tint
    OS::VduExt_GFCOL,       // Graphics foreground col
    OS::VduExt_GPLBMD,      // GCOL action for background col
    OS::VduExt_GBTint,      // Graphics background tint
    OS::VduExt_GBCOL,       // Graphics background col
    OS::VduExt_Log2BPP,     // log2(bpp). ie. log2(log2(cols))
    OS::VduExt_XEigFactor,  // Conversion factor between OS units and pixels
    OS::VduExt_YEigFactor,  // Conversion factor between OS units and pixels
    OS::VduExt_EndList
};

// Routine to read interesting variables about the current screen mode and
// do some preprocessing on them.
MyError ScreenVars::readForCurrentMode()
{
    MyError err;

    if ((err = OS::xreadVduVariables(wanted, &fgmode)) != nullptr)
        return err;

    // Combine foreground GCOL and TINT into a single word
    fggcol = fggcol | (fgrgb << 8);

    // Combine background GCOL and TINT into a single word
    bggcol = bggcol | (bgrgb << 8);

    return nullptr;
}

void ScreenVars::setGCOLandColour(OS::GCOLAction gcolAction, uint8_t colour)
{
    if ((colour & 0x80) == 0) {
        // Colour is foreground
        fgmode = gcolAction;
        fggcol = (fggcol & 0x00ff00ffu) | (uint32_t(colour) << 8);
    } else {
        bgmode = gcolAction;
        bggcol = (bggcol & 0x00ff00ffu) | (uint32_t(colour) << 8);
    }
}

void ScreenVars::setGCOLandColour(OS::GCOLAction gcolAction,
                                  uint8_t flags,
                                  uint32_t bgr0)
{
    if ((flags & 0x80) == 0) {
        // Colour is foreground
        fgmode = gcolAction;
        fgrgb = bgr0;
        fggcol = (fggcol & 0x00ffffffu) | 0x80000000u;
    } else {
        bgmode = gcolAction;
        bgrgb = bgr0;
        bggcol = (bggcol & 0x00ffffffu) | 0x80000000u;
    }
}
