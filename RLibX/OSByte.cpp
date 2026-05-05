#include "kernel.h"

#include "RLib/RLib.h"

#include "OSByte.h"

#include "swis.h"

MyError OSByte::osbyte1(Op op, int r1, int r2, uint8_t& r1Out)
{
    uint32_t v;
    MyError err = _swix(OS_Byte, _INR(0,2) | _OUT(1), op, r1, r2, &v);
    if (err)
        return err;

    r1Out = v;
    return nullptr;
}

MyError OSByte::osbyte1(Var var, int r1, int r2, uint8_t& r1Out)
{
    uint32_t v;
    MyError err = _swix(OS_Byte, _INR(0,2) | _OUT(1), var, r1, r2, &v);
    if (err)
        return err;

    r1Out = v;
    return nullptr;
}

MyError OSByte::xbyte(uint32_t& r0, uint32_t& r1, uint32_t& r2)
{
    return _swix(OS_Byte, _INR(0,2) | _OUTR(0,2),
                 r0, r1, r2, &r0, &r1, &r2);
}

MyError OSByte::read(Var var, uint8_t& value)
{
    uint32_t v;
    MyError err = _swix(OS_Byte, _INR(0,2) | _OUT(1), var, 0, 0xff, &v);
    if (err)
        return err;

    value = v;
    return nullptr;
}

MyError OSByte::write(Var var, uint8_t value)
{
    return _swix(OS_Byte, _INR(0,2), var, value, 0);
}
