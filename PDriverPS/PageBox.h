#ifndef PDRIVERPS_PAGEBOX_H
#define PDRIVERPS_PAGEBOX_H

#include "Core/Device.h"

class JobWS;

extern const char prologue_filename[];
extern const char prologue2_filename[];
extern const char epilogue_filename[];
extern const char psfeed_filename[];
extern const char pspaper_filename[];

MyError pagebox_insertfile(const char* filename, JobWS& job);

#endif
