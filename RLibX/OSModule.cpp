#include "kernel.h"

#include "OSModule.h"

#include "swis.h"

MyError OSModule::xmodule(uint32_t reason,
                         uint32_t r1,
                         void* r2,
                         uint32_t r3,
                         void** r2Out)
{
    void* block;
    MyError err = _swix(OS_Module, _INR(0,3) | _OUT(2),
                       reason, r1, r2, r3, &block);
    if (err != nullptr)
        return err;

    if (r2Out != nullptr)
        *r2Out = block;
    return nullptr;
}

MyError OSModule::xalloc(int size, void*& blk)
{
    return xmodule(Reason_Alloc, 0, nullptr, size, &blk);
}

MyError OSModule::xfree(void* blk)
{
    return _swix(OS_Module, _INR(0,2), Reason_Free, 0, blk);
}
