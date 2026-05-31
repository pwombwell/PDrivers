#ifndef PDRIVERDP_COLOUR_H
#define PDRIVERDP_COLOUR_H

#include <stdint.h>

#include "Core/Device.h"

class JobWS;

/* Colour handling routines for the bitmap (DP) printer driver. */

/* Map RGB to nearest printer pixel value (bottom byte). */
MyError colour_rgbto256colpixval(uint32_t bbGGRR00, uint32_t& col, JobWS& job);

extern const uint8_t fourbyfourECFbytes[17*8];
extern const uint8_t eightbyeightECFbytes[65*8];

#endif
