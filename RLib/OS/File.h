#ifndef RLIB_OS_FILE_H
#define RLIB_OS_FILE_H

#include "RLib/RLib.h"

namespace riscos {
namespace OS {

enum Find {
    Find_ReadFile     = 0x40,
    Find_CreateFile   = 0x80,
    Find_ReadWrite    = 0xc0
};

// Calls OS_Find with OSFind_ReadFile (OS::Find::ReadFile).
MyError openFile(const char* filename, FileHandle& file);

// Calls OS_Find with r0=0.
MyError closeFile(FileHandle file);

// Calls OS_Args with reason SetExt.
MyError setExtent(FileHandle file, int ext);

MyError bget(FileHandle file, char& c, bool& eof);

enum ArgsReason {
    Args_ReadPtr         = 0,
    Args_SetPtr          = 1,
    Args_ReadExt         = 2,
    Args_SetExt          = 3,
    Args_ReadAllocation  = 4,
    Args_ReadEOFStatus   = 5,
    Args_Ensure          = 0xff
};

MyError args(ArgsReason reason, FileHandle file, uint32_t value, uint32_t& valueOut);
MyError args(ArgsReason reason, FileHandle file, uint32_t value);

} } // namespace riscos::OS

#endif
