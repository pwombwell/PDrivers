#include "kernel.h"

#include "FileSwitch.h"

#include "swis.h"

MyError FileSwitch::xosBGet(FileHandle file, char& c, unsigned int& psrOut)
{
    return _swix(OS_BGet, _IN(0) | _OUT(0) | _OUT(_FLAGS),
                 file, &c, &psrOut);
}

MyError FileSwitch::xbget(FileHandle file, uint8_t& byteOut, bool& eofOut)
{
    char c;
    unsigned int psrOut;
    MyError err = xosBGet(file, c, psrOut);
    if (err != nullptr)
        return err;

    byteOut = c;
    eofOut = (psrOut & _C) != 0;
    return nullptr;
}
