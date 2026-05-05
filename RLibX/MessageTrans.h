#ifndef RLIBX_MESSAGETRANS_H
#define RLIBX_MESSAGETRANS_H

#include "RLib/RLib.h"
#include "RLib/MessageTrans.h"

namespace riscos {
namespace MessageTrans {

struct BlockState : Block {
    bool  open;
};

// Opens a message file
//
// In:      r0 - block
//          r1 - filename
//          r2 - buffer [optional]
//
// Calls SWI MessageTrans_OpenFile (0x41501)

MyError xopenFile(const Block& block, const char* filename, char* buffer);
MyError xopenFile(const Block& block, const char* filename);

// Translates a message token into a string
//
// In:      r0 - block
//          r1 - token
//          r2 - buffer [optional]
//          r3 - size   [optional]
//          r4 - arg0
//          r5 - arg1
//          r6 - arg2
//          r7 - arg3
//
// Out:     r3 - used
//
// Calls SWI MessageTrans_Lookup (0x41502)

MyError lookup(const Block& block, char const* token, char* buffer, int size,
               int& used,
               const char* param0 = nullptr, const char* param1 = nullptr,
               const char* param2 = nullptr, const char* param3 = nullptr);

MyError lookup(const Block& block, char const* token, char* buffer, int size,
               const char* param0 = nullptr, const char* param1 = nullptr,
               const char* param2 = nullptr, const char* param3 = nullptr);

// Translates a message token into a string (no substition)
//
// resultLen does not include terminator, which is '\n', not '\0'.
//
// In:      r0 - block
//          r1 - token
//          r2-r7 - 0
//
// Out:     r2 - result (X version only)
//          r3 - used
//
// Returns: r2 - result (non-X version only)
//
// Calls SWI MessageTrans_Lookup (0x41502)
MyError lookup(const Block& block, char const* token,
               const char*& result, uint32_t& resultLen);

// Closes a message file
//
// In:      r0 - block
//
// Calls SWI MessageTrans_CloseFile (0x41504)

MyError xcloseFile(const Block& block);

// Translates a message token from an error-token view.
//
// In:      r0 - error token block (size dependent on errmess)
//          r1 - block
//          r2 - buffer (return buffer, must be max size)
//          r3 - size    =sizeof(_kernel_oserror) = 256
//          r4 - arg0
//          r5 - arg1
//          r6 - arg2
//          r7 - arg3
//
// Calls SWI MessageTrans_ErrorLookup (0x41506)

MyError xerrorLookup(OS::ErrorView error, const Block& block, OS::MaxErrorBlock& buffer,
                     const char* param0 = nullptr, const char* param1 = nullptr,
                     const char* param2 = nullptr, const char* param3 = nullptr);

} } // namespace riscos::MessageTrans

#endif
