#ifndef PDRIVERPS_COLOUR_H
#define PDRIVERPS_COLOUR_H

#include <stdint.h>

#include "Core/Device.h"

class JobWS;

// Routine to make certain the correct colour is set in the PostScript
// output
MyError colour_ensure(JobWS& job);

#endif
