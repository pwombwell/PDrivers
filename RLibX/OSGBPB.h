#ifndef OSGBPB_H
#define OSGBPB_H

#include "RLib/RLib.h"

class OSGBPB {
public:

// Writes bytes to an open file.
//
// In:      r1 - file
//          r2 - data
//          r3 - size
//
// Out:     r3 - unwritten (X version only)
//
// Returns: r3 - unwritten (non-X version only)
//
// Calls SWI OS_GBPB (0xC) with R0 = 2

static MyError xwrite(FileHandle file, const void* data, uint32_t size, uint32_t& unwritten);

}; // namespace OSGBPB

#endif