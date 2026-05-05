#include "PDriver.h"
#include "Colour.h"

#include "InterceptColTrans.h"
#include "Constants.h"
#include "Job.h"
#include "OS.h"
#include "Workspace.h"

#include <stdio.h>

// Routine to work out the RGB value that corresponds to a particular GCOL/tint.
//
// gcol_tint: GCOL in bits 15-8, tint in bits 7-0
// returns BBGGRR00.
Colour gcol_lookup(uint32_t gcol_tint, const CoreJobWS& jobWS)
{
    uint32_t lgbpp = jobWS.screenVars.lgbpp;

#if Medusa
    // 8bpp full palette: as for 8bpp non-full palette
    // 16/32bpp: need to generate the &BBGGRR00 as for a
    // standard palette from the GCOL and TINT

    // 16bpp or 32bpp? Then rip the bits out directly.
    if (lgbpp == 4 || lgbpp == 5) {
        // `gcol_lookup_16or32`
        uint32_t gcol = gcol_tint >> 8;
        uint32_t tint = (gcol_tint >> 6) & 3;

        uint32_t r = tint | ((gcol & 0x003) << 2);
        uint32_t g = tint |  (gcol & 0x030);
        uint32_t b = tint | ((gcol & 0x300) >> 2);

        uint32_t rgb = (r << (15 - 4)) | (g << (23 - 4)) | (b << (31 - 4));
        rgb |= rgb >> 4;

        return rgb;
    }
#endif

    // 256 colour, so convert to pixel value...
    uint32_t pixval;
    if (lgbpp != 3) {
        pixval = gcol_tint >> 8;
    } else {
        uint32_t r1 = gcol_tint >> 6;
        pixval = r1 & 0x87;
        uint32_t r2 = r1 & 0x38;

        pixval |= (r2 << 1);
        r2 = r1 & 0x40;
        pixval |= (r2 >> 3);
    }

    // ...and lookup that pixel value in our colourtrans table.
    return pixval_lookup(pixval, jobWS);
}

// Lookup the RGB value for a pixel value in bits 0-7.
uint32_t pixval_lookup(uint32_t pixval, const CoreJobWS& jobWS)
{
    return InterceptColTrans::readPalette(pixval, 16);
}

static int gcol_lookupcommon(int32_t mode,
                             int32_t gcol_tint,
                             int32_t rgb,
                             Disabled mask,
                             uint32_t *rgb_out,
                            CoreJobWS& jobWS)
{
    Disabled disabled = jobWS.disabled;

    if ((mode & 7) != 0) {
        disabled |= Disabled_Colour;
    } else {
        disabled &= ~Disabled_Colour;
    }
    jobWS.disabled = disabled;

    if (!!(disabled & ~mask))
        return 0;

    uint32_t result;
    if (gcol_tint < 0) {
        result = (uint32_t)rgb;
    } else {
        result = gcol_lookup((uint32_t)gcol_tint, jobWS);
    }

    if (rgb_out) {
        *rgb_out = result;
    }
    return 1;
}

int gcol_lookupfgorbg(int want_background,
                      Disabled mask,
                      uint32_t *rgb_out,
                    CoreJobWS& jobWS)
{
    if (want_background) {
        return gcol_lookupbg(mask, rgb_out, jobWS);
    }
    return gcol_lookupfg(mask, rgb_out, jobWS);
}

int gcol_lookupfg(Disabled mask,
                  uint32_t *rgb_out,
                CoreJobWS& jobWS)
{
    return gcol_lookupcommon(jobWS.screenVars.fgmode,
                             jobWS.screenVars.fggcol,
                             jobWS.screenVars.fgrgb,
                             mask,
                             rgb_out,
                            jobWS);
}

int gcol_lookupbg(Disabled mask,
                  uint32_t *rgb_out,
                CoreJobWS& jobWS)
{
    return gcol_lookupcommon(jobWS.screenVars.bgmode,
                             jobWS.screenVars.bggcol,
                             jobWS.screenVars.bgrgb,
                             mask,
                             rgb_out,
                            jobWS);
}
