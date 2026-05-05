#include "File.h"

#include "swis.h"

namespace riscos {
namespace OS {

MyError openFile(const char* filename, FileHandle& file)
{
    uint32_t handle;
    _kernel_oserror* e = _swix(OS_Find, _INR(0,1) | _OUT(0),
                               OS::Find_ReadFile, filename, &handle);
    if (e)
        return e;

    file = handle;

    return nullptr;
}

MyError closeFile(FileHandle file)
{
    return _swix(OS_Find, _INR(0,1), 0, file);
}

MyError bget(FileHandle file, char& c, bool& eof)
{
    uint32_t byte, flags;
    _kernel_oserror* e = _swix(OS_BGet, _IN(1) | _OUT(0) | _OUT(_FLAGS),
                               file, &byte, &flags);
    if (e)
        return e;

    eof = !!(flags & _C);
    c = (char) byte;

    return nullptr;
}

MyError setExtent(FileHandle file, int ext)
{
    return args(Args_SetExt, file, ext);
}

MyError args(ArgsReason reason, FileHandle file, uint32_t value, uint32_t& valueOut)
{
    return _swix(OS_Args, _INR(0,2) | _OUT(2), reason, file, value, &valueOut);
}

MyError args(ArgsReason reason, FileHandle file, uint32_t value)
{
    return _swix(OS_Args, _INR(0,2), reason, file, value);
}

} } // namespace riscos::OS
