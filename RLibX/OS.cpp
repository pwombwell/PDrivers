#include "kernel.h"

#include "OS.h"

#include "Draw.h"

#include "swis.h"

#include "RLibX/Utils/Debug.h"

namespace riscos {
namespace OS {

Unit64
Unit64::sqrt() const
{
    uint32_t v = value;
    uint64_t res = 0;
    uint64_t bit = 1ull << 62;
    while (bit > v) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (v >= res + bit) {
            v -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return (uint32_t)res;
}

uint32_t readVarValSize(char const* var, VarType varType)
{
    // Note: this can not be implemented with _swix() as it returns an error
    // if the buffer size is negative... which is how you get the size.

    _kernel_swi_regs r;
    r.r[0] = (int)var;
    r.r[1] = 0;     // buffer
    r.r[2] = -1;    // buffer size (ie. read)
    r.r[3] = 0;     // context
    r.r[4] = varType;

    (void)_kernel_swi(OS_ReadVarVal | XOS_Bit, &r, &r);

    int32_t size = r.r[2];

    // Return negated result, ignoring inevitable error.
    return (varType != 3) ? -size : size;
}

MyError xreadVarVal(char const* var, char* value, int size, int context,
                       VarType varType, int& used, int& contextOut,
                       VarType& varTypeOut)
{
    return _swix(OS_ReadVarVal, _INR(0,4) | _OUTR(2,4),
                 var, value, size, context, varType,
                 &used, &contextOut, &varTypeOut);
}

MyError setVarVal(char const* var, void const* value, size_t size, VarType varType)
{
    return _swix(OS_SetVarVal, _INR(0,4),
                 var, value, size, 0, varType);
}

MyError xgsTrans(char const* in, char* out, int32_t& len, uint32_t flags, int& truncated)
{
    uint32_t psrOut = 0;
    MyError err = _swix(OS_GSTrans, _INR(0,3) | _OUT(2) | _OUT(_FLAGS),
                       in, out, len, flags, &len, &psrOut);
    if (err != nullptr)
        return err;

    truncated = (psrOut & _C) != 0;
    return nullptr;
}

MyError xbinaryToDecimal(int32_t value, char* buffer, int32_t& len)
{
    return _swix(OS_BinaryToDecimal, _INR(0,2) | _OUT(2),
                 value, buffer, len, &len);
}

MyError xreadEscapeState(bool& escaped)
{
    uint32_t psrOut;
    MyError err = _swix(OS_ReadEscapeState, _OUT(_FLAGS), &psrOut);
    if (err != nullptr)
        return err;

    escaped = (psrOut & _C) != 0;
    return nullptr;
}

MyError xreadVduVariables(VDUVarList const* varList, uint32_t* valueList)
{
    return _swix(OS_ReadVduVariables, _INR(0,1), varList, valueList);
}

MyError xpaletteVReadEntry(uint32_t entry, int colourType, Colour& on, Colour& off, bool& incomplete)
{
    uint32_t incompleteFlag;
    MyError err = _swix(OS_CallAVector, _INR(0,1) | _IN(4) | _IN(9) | _OUTR(2,4),
                       entry, colourType, 1, 35,
                       &on, &off, &incompleteFlag);
    if (err != nullptr)
        return err;

    incomplete = incompleteFlag != 0;
    return nullptr;
}

MyError readPalette(uint32_t logical, uint32_t type, uint32_t& firstFlash, uint32_t& secondFlash)
{
    return _swix(OS_ReadPalette, _INR(0,1) | _OUTR(2,3),
                 logical, type, &firstFlash, &secondFlash);
}

MyError xreadModeVariable(Mode mode, ModeVar var, uint32_t& varVal)
{
    return _swix(OS_ReadModeVariable, _INR(0,1) | _OUT(2),
                 mode.toNumber(), var,
                 &varVal);
}

MyError xreadModeVariable(Mode mode, ModeVar var, uint32_t& varVal, uint32_t& psrOut)
{
    return _swix(OS_ReadModeVariable, _INR(0,1) | _OUT(2) | _OUT(_FLAGS),
                 mode.toNumber(), var,
                 &varVal, &psrOut);
}

MyError xcallAVector(uint32_t vector,
                        uint32_t* r0,
                        uint32_t* r1,
                        uint32_t* r2,
                        uint32_t* r3,
                        uint32_t* r4,
                        uint32_t* r5,
                        uint32_t* r6,
                        uint32_t* r7)
{
    uint32_t v0 = (r0 != nullptr) ? *r0 : 0;
    uint32_t v1 = (r1 != nullptr) ? *r1 : 0;
    uint32_t v2 = (r2 != nullptr) ? *r2 : 0;
    uint32_t v3 = (r3 != nullptr) ? *r3 : 0;
    uint32_t v4 = (r4 != nullptr) ? *r4 : 0;
    uint32_t v5 = (r5 != nullptr) ? *r5 : 0;
    uint32_t v6 = (r6 != nullptr) ? *r6 : 0;
    uint32_t v7 = (r7 != nullptr) ? *r7 : 0;
    MyError err = _swix(OS_CallAVector, _INR(0,7) | _IN(9) | _OUTR(0,7),
                       v0, v1, v2, v3, v4, v5, v6, v7, vector,
                       &v0, &v1, &v2, &v3, &v4, &v5, &v6, &v7);
    if (err != nullptr)
        return err;

    if (r0 != nullptr)
        *r0 = v0;
    if (r1 != nullptr)
        *r1 = v1;
    if (r2 != nullptr)
        *r2 = v2;
    if (r3 != nullptr)
        *r3 = v3;
    if (r4 != nullptr)
        *r4 = v4;
    if (r5 != nullptr)
        *r5 = v5;
    if (r6 != nullptr)
        *r6 = v6;
    if (r7 != nullptr)
        *r7 = v7;
    return nullptr;
}

MyError xchangeEnvironment(HandlerType handlerType, void* handler, void** handle,
                              void** buffer, void*& oldHandler, void**& oldHandle,
                              void**& oldBuffer)
{
    return _swix(OS_ChangeEnvironment, _INR(0,3) | _OUTR(1,3),
                 handlerType, handler, handle, buffer,
                 &oldHandler, &oldHandle, &oldBuffer);
}

MyError xchangeDynamicArea(DynamicAreaNo area, int32_t change)
{
    int32_t changeOut;
    return _swix(OS_ChangeDynamicArea, _INR(0,1) | _OUT(1),
                 area, change, &changeOut);
}

MyError xserviceCall(uint32_t code)
{
    return _swix(OS_ServiceCall, _IN(1), code);
}

MyError xcli(char const* command)
{
    return _swix(OS_CLI, _IN(0), command);
}

MyError xwriteI(uint32_t value)
{
    return _swix(OS_WriteI + (value & 0xffu), 0);
}

MyError xaddCallBack(void* callBack, uint32_t handle)
{
    return _swix(OS_AddCallBack, _INR(0,1), callBack, handle);
}

MyError xupCall(uint32_t r0, uint32_t r1, void* r2)
{
    return _swix(OS_UpCall, _INR(0,2), r0, r1, r2);
}

MyError xwriteC(uint32_t value)
{
    return _swix(OS_WriteC, _IN(0), value);
}

MyError xwriteN(const void* buffer, uint32_t len)
{
    return _swix(OS_WriteN, _INR(0,1), buffer, len);
}

MyError xsetColour(uint32_t flags, uint32_t colour)
{
    return _swix(OS_SetColour, _INR(0,1), flags, colour);
}

MyError xword(uint32_t reason, void* block)
{
    return _swix(OS_Word, _INR(0,1), reason, block);
}

MyError readDynamicArea(DynamicAreaNo area, void*& areaStart,
                        int& size, int& sizeLimit)
{
    return _swix(OS_ReadDynamicArea, _IN(0) | _OUTR(0,2),
                 area, &areaStart, &size, &sizeLimit);
}

} } // namespace riscos::OS
