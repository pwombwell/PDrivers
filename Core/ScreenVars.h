#ifndef SCREENVARS_H
#define SCREENVARS_H

#include <stdint.h>
#include "RLibX/OS.h"

// Interesting variables about the current screen mode.
struct ScreenVars {
    MyError readForCurrentMode();

    // Top-bit set of a byte-sized GCOL colour = background

    // Set GCOL from VDU byte stream.
    void setGCOLandColour(OS::GCOLAction gcolAction, uint8_t colour);

    // Set closest GCOL given 32-bit BGR0 colour value.
    void setGCOLandColour(OS::GCOLAction gcolAction, uint8_t flags, uint32_t bgr0);

    // Order must match the table in the .cpp.
    // Name these nicely as soon as practical!
    uint32_t fgmode;  // GCOL action for foreground col
    uint32_t fggcol;        // Graphics foreground tint
    uint32_t fgrgb;         // Graphics foreground col

    uint32_t bgmode;  // GCOL action for background col
    uint32_t bggcol;        // Graphics background tint
    uint32_t bgrgb;         // Graphics background col

    uint32_t lgbpp;         // log2(bpp). ie. log2(log2(cols))
    uint32_t currxeig;      // Conversion factor between OS units and pixels
    uint32_t curryeig;      // Conversion factor between OS units and pixels
};

#endif
