#include "kernel.h"

#include "MakePSFont.h"

#include "swis.h"

MyError MakePSFont::xmakeFont(FileHandle outputHandle,
                             const char* fontName,
                             char* returnNameBuffer,
                             uint32_t* bufferSizeInOut,
                             uint32_t flags)
{
    uint32_t bufferSize = (bufferSizeInOut != nullptr) ? *bufferSizeInOut : 0;
    MyError err = _swix(MakePSFont_MakeFont, _INR(0,4) | _OUT(3),
                       outputHandle, fontName, returnNameBuffer, bufferSize, flags,
                       &bufferSize);
    if (err != nullptr)
        return err;

    if (bufferSizeInOut != nullptr)
        *bufferSizeInOut = bufferSize;
    return nullptr;
}
