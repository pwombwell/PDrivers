#include "kernel.h"

#include "OSFind.h"

#include "swis.h"

MyError OSFind::xfind(uint32_t reason,
                     const void* nameOrHandle,
                     FileHandle* handleOut)
{
    FileHandle file = 0;
    MyError err = _swix(OS_Find, _INR(0,1) | _OUT(0),
                       reason, nameOrHandle, &file);
    if (err != nullptr)
        return err;

    if (handleOut != nullptr)
        *handleOut = file;
    return nullptr;
}

MyError OSFind::xcloseW(FileHandle file)
{
    return xfind(Reason_Close, (const void*)file, nullptr);
}
