#include "kernel.h"
#include "RLib/RLib.h"

#include "MessageTrans.h"

#include "swis.h"

MyError MessageTrans::xopenFile(const Block& block, const char* filename,
                               char* buffer)
{
    return _swix(MessageTrans_OpenFile, _INR(0,2), &block, filename, buffer);
}

MyError MessageTrans::xopenFile(const Block& block, const char* filename)
{
    return _swix(MessageTrans_OpenFile, _INR(0,2), &block, filename, 0);
}

MyError MessageTrans::lookup(const Block& block, char const* token,
                             char* buffer, int size, int& used,
                             const char* param0, const char* param1,
                             const char* param2, const char* param3)
{
    return _swix(MessageTrans_Lookup, _INR(0,7) | _OUT(3),
                 &block, token, buffer, size, param0, param1, param2, param3,
                 &used);
}

MyError MessageTrans::lookup(const Block& block, char const* token,
                             char* buffer, int size,
                             const char* param0, const char* param1,
                             const char* param2, const char* param3)
{
    return _swix(MessageTrans_Lookup, _INR(0,7) | _OUT(3),
                 &block, token, buffer, size, param0, param1, param2, param3);
}

MyError MessageTrans::lookup(const Block& block, char const* token,
                             const char*& result, uint32_t& resultLen)
{
    return _swix(MessageTrans_Lookup, _INR(0,7) | _OUTR(2,3),
                 &block, token, 0, 0, 0, 0, 0, 0,
                 &result, &resultLen);
}

MyError MessageTrans::xcloseFile(const Block& block)
{
    return _swix(MessageTrans_CloseFile, _IN(0), &block);
}

MyError MessageTrans::xerrorLookup(OS::ErrorView error, const Block& block,
                                   OS::MaxErrorBlock& buffer, const char* param0,
                                   const char* param1, const char* param2,
                                   const char* param3)
{
    return _swix(MessageTrans_ErrorLookup, _INR(0,7),
                  error.data(), &block, &buffer, sizeof(buffer),
                  param0, param1, param2, param3);
}
