#ifndef RLIBX_FILESWITCH_H
#define RLIBX_FILESWITCH_H

#include "RLib/RLib.h"

namespace riscos {
namespace FileSwitch {

static MyError xosBGet(FileHandle file, char& c, unsigned int& psrOut);
static MyError xbget(FileHandle file, uint8_t& byteOut, bool& eofOut);

} } // namespace riscos::FileSwitch

#endif
