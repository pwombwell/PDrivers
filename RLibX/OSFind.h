#ifndef RLIB_OSFIND_H
#define RLIB_OSFIND_H

#include "RLib/RLib.h"

class OSFind {
public:

enum Flags {
    Flags_Path          = 1u << 0,
    Flags_PathVar       = 1u << 1,
    Flags_NoPath        = 3u,
    Flags_ErrorIfDir    = 1u << 2,
    Flags_ErrorIfAbsent = 1u << 3
};

enum Reason {
    Reason_Close   = 0,
    Reason_OpenIn  = 0x40,
    Reason_OpenOut = 0x80,
    Reason_OpenUp  = 0xc0
};

static MyError xfind(uint32_t reason,
                    const void* nameOrHandle,
                    FileHandle* handleOut);
static MyError xcloseW(FileHandle file);

};

#endif
