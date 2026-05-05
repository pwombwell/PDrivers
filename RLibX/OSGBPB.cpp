#include "kernel.h"

#include "OSGBPB.h"

#include "swis.h"

#include "RLibX/Utils/Debug.h"

MyError OSGBPB::xwrite(FileHandle file, const void* data, uint32_t size,
                      uint32_t& unwritten)
{
    return _swix(OS_GBPB, _INR(0,3)|_OUT(3),
                 2, file, data, size, &unwritten);
}
