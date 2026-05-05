#ifndef CORE_COLOUR_H
#define CORE_COLOUR_H

#include "RLib/RLib.h"
#include "RLibX/Utils/EnumBitmaskOps.h"

#include <stdint.h>

class CoreJobWS;

typedef uint32_t Colour; // bbGGrr00.

/* Colour handling routines. */

/* Lookup the RGB value for a GCOL+tint combination. */
uint32_t gcol_lookup(uint32_t gcol_tint, const CoreJobWS& jobWS);

/* Lookup the RGB value for a pixel value in bits 0-7. */
uint32_t pixval_lookup(uint32_t pixval, const CoreJobWS& jobWS);

/* Read the current VDU variables into the job workspace. */
MyError readcurvduvariables(CoreJobWS& jobWS);

/* Values for the 'disabled' flags. */
enum Disabled {
    Disabled_None       = 0,
    Disabled_VDU        = 1u<<0,
    Disabled_Colour     = 1u<<1,
    Disabled_NoPage     = 1u<<2,
    Disabled_NullClip   = 1u<<3
};
DEFINE_ENUM_BITWISE_OPERATORS(Disabled);

/* Determine the foreground or background colour.
 * Returns nonzero if colour lookup succeeded and rgb_out is valid.
 */
int gcol_lookupfgorbg(int want_background,
                      Disabled mask,
                      uint32_t *rgb_out,
                    CoreJobWS& jobWS);
int gcol_lookupfg(Disabled mask,
                  uint32_t *rgb_out,
                CoreJobWS& jobWS);
int gcol_lookupbg(Disabled mask,
                  uint32_t *rgb_out,
                CoreJobWS& jobWS);

#endif
