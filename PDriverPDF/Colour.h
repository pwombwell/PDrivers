#ifndef PDRIVERPDF_COLOUR_H
#define PDRIVERPDF_COLOUR_H

#include "Core/Device.h"

class JobWS;

// Routine to make certain the correct colour is set in the PDF
// output
MyError colour_ensure(JobWS& job);

#endif
