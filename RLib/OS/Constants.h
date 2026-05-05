#pragma once

#ifndef RLIB_OS_CONSTANTS_H
#define RLIB_OS_CONSTANTS_H

namespace riscos {
namespace OS {

// These should probably come from RISC OS auto-generated headers, though
// they are currently #defines which isn't very typesafe.

enum ModeVar {
    ModeVar_ModeFlags    =   0,
    ModeVar_NColour      =   3,
    ModeVar_XEigFactor   =   4,
    ModeVar_YEigFactor   =   5,
    ModeVar_Log2BPP      =   9, // log2(bpp). ie. log2(log2(cols))
    ModeVar_Log2BPC      =  10  // log2(bits per char).
};

enum VDUVar {
    VduExt_ModeFlags    =   0,
    VduExt_NColour      =   3,
    VduExt_XEigFactor   =   4,
    VduExt_YEigFactor   =   5,
    VduExt_Log2BPP      =   9, // log2(bpp). ie. log2(log2(cols))
    VduExt_Log2BPC      =  10, // log2(bits-per-character).
    VduExt_GPLFMD       = 151, // GCOL action for foreground col
    VduExt_GPLBMD       = 152, // GCOL action for background col
    VduExt_GFCOL        = 153, // Graphics foreground col
    VduExt_GBCOL        = 154, // Graphics background col
    VduExt_GFTint       = 157, // Graphics foreground tint
    VduExt_GBTint       = 158, // Graphics background tint
    Force_To_32_bit     = 0x7fffffff,
    VduExt_EndList      =  -1
};

typedef VDUVar VDUVarList;

enum VarType {
    VarType_String        =    0,
    VarType_Number        =    1,
    VarType_Macro         =    2,
    VarType_Expanded      =    3,
    VarType_LiteralString =    4,
    VarType_Code          = 0x10
};

} } // namespace riscos::OS

#endif
