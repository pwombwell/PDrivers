#ifndef OS_H
#define OS_H

#include "RLib/RLib.h"

#include "RLib/OS.h"
#include "RLib/OS/Mode.h"
#include "RLibX/Utils/EnumBitmaskOps.h"

namespace riscos {
class MyError;

namespace Base { template<uint8_t Shift> class Fixed; }

namespace OS {
struct Unit64;
class Mode;

enum GCOLAction {
    GCOLAction_Src      = 0, // Dest = Source
    GCOLAction_OR       = 1, // Dest = Dest OR Source
    GCOLAction_AND      = 2, // Dest = Dest AND Source
    GCOLAction_EOR      = 3, // Dest = Dest EOR Source
    GCOLAction_NOT_Dst  = 4, // Dest = NOT Dest
    GCOLAction_Dst      = 5, // Dest = Dest
    GCOLAction_AND_NOT  = 6, // Dest = Dest AND NOT Source
    GCOLAction_OR_NOT   = 7, // Dest = Dest OR NOT Source

    GCOLAction_Mask     = 7
};
DEFINE_ENUM_BITWISE_OPERATORS(GCOLAction);

// OS::Units. 1/180th inch.
struct Unit {
    Unit() : value(0) {}
    /*explicit*/ Unit(int32_t valueIn) : value(valueIn) {}

    int32_t raw() const { return value; }

    Unit& operator+=(const Unit& rhs) { value += rhs.value; return *this; }
    Unit& operator-=(const Unit& rhs) { value -= rhs.value; return *this; }

    Unit operator+(const Unit& rhs) const { return Unit(value + rhs.value); }
    Unit operator-(const Unit& rhs) const { return Unit(value - rhs.value); }
    Unit operator*(uint32_t rhs) const { return Unit(value * rhs); }
    Unit operator/(uint32_t rhs) const { return Unit(value / rhs); }

    Unit operator-() const { return Unit(-value); }

    Unit& operator++()    { value++; return *this; }
    Unit operator++(int) { Unit tmp(*this); ++*this; return tmp; }
    Unit& operator--()    { value--; return *this; }
    Unit operator--(int) { Unit tmp(*this); --*this; return tmp; }

    bool operator==(const Unit& rhs) const { return value == rhs.value; }
    bool operator!=(const Unit& rhs) const { return value != rhs.value; }
    bool operator< (const Unit& rhs) const { return value < rhs.value; }
    bool operator<=(const Unit& rhs) const { return value <= rhs.value; }
    bool operator> (const Unit& rhs) const { return value > rhs.value; }
    bool operator>=(const Unit& rhs) const { return value >= rhs.value; }

    // 32x32 bit -> 64 bit mul without loss of precision.
    Unit64 mul64(const Unit& rhs) const;

    int32_t value;
};

// 1/72000th inch. Not sure where Millipoints would be best placed.
struct Millipoint {
    Millipoint() : value(0) {}
    /*explicit*/ Millipoint(int32_t valueIn) : value(valueIn) {}

    int32_t raw() const { return value; }

    Unit toOSUnit() const {
        return Unit(roundToOS(value));
    }

    Millipoint& operator+=(const Millipoint& rhs) { value += rhs.value; return *this; }
    Millipoint& operator-=(const Millipoint& rhs) { value -= rhs.value; return *this; }

    Millipoint operator+(const Millipoint& rhs) const { return Millipoint(value + rhs.value); }
    Millipoint operator-(const Millipoint& rhs) const { return Millipoint(value - rhs.value); }
    Millipoint operator/(const Millipoint& rhs) const { return Millipoint(value / rhs.value); }

    Millipoint operator-() const { return Millipoint(-value); }

    bool operator==(const Millipoint& rhs) const { return value == rhs.value; }
    bool operator!=(const Millipoint& rhs) const { return value != rhs.value; }
    bool operator< (const Millipoint& rhs) const { return value < rhs.value; }
    bool operator<=(const Millipoint& rhs) const { return value <= rhs.value; }
    bool operator> (const Millipoint& rhs) const { return value > rhs.value; }
    bool operator>=(const Millipoint& rhs) const { return value >= rhs.value; }

private:
    static int32_t roundToOS(int32_t n) {
        return (n >= 0) ? ((n + 200) / 400) : ((n - 200) / 400);
    }

    int32_t value;
};

// Helper for OS::Unit^2 without loss of precision.
struct Unit64 {
public:
    Unit64(int64_t v) : value(v) { }
    Unit64(Unit v) : value(v.raw()) { }

    int64_t raw() const { return value; }

    Unit64 operator+(const Unit64& rhs) const { return Unit64(value + rhs.value); }
    Unit64 operator-(const Unit64& rhs) const { return Unit64(value - rhs.value); }
    Unit64 operator*(int32_t i) const { return Unit64(value * i); }

    Unit64 operator<<(size_t i) const { return Unit64(value<<i); }

    bool operator==(const Unit& rhs) const { return value == rhs.value; }
    bool operator< (const Unit& rhs) const { return value < rhs.value; }

    Unit64 operator-() const { return Unit64(-value); }

    // 64x64 bit -> 64 bit mul with loss of precision.
    Unit64 mul(const Unit64& rhs) const { return value * rhs.value; }
    Unit64 sqrt() const;

    int64_t value;
};

inline Unit64 Unit::mul64(const Unit& rhs) const {
    return Unit64((int64_t)value * rhs.value);
}

inline Unit64 operator-(Unit a, Unit64 b) { return Unit64(int64_t(a.raw())) - b; }

enum HandlerType {
  HandlerType_MemoryLimit           =    0,
  HandlerType_UndefinedInstruction  =    1,
  HandlerType_PrefetchAbort         =    2,
  HandlerType_DataAbort             =    3,
  HandlerType_AddressException      =    4,
  HandlerType_OtherExceptions       =    5,
  HandlerType_Error                 =    6,
  HandlerType_CallBack              =    7,
  HandlerType_BreakPt               =    8,
  HandlerType_Escape                =    9,
  HandlerType_Event                 =  0xa,
  HandlerType_Exit                  =  0xb,
  HandlerType_UnusedSWI             =  0xc,
  HandlerType_ExceptionRegisters    =  0xd,
  HandlerType_ApplicationSpace      =  0xe,
  HandlerType_CAO                   =  0xf,
  HandlerType_UpCall                = 0x10,
  HandlerType_BackwardCompatibility = 0x11  // Never to be used
};

typedef int DynamicAreaNo;

// Gets size of system variable or checks for its existance (PRM 1-309, 5a-661)
//
// In:      r0 - var
//          r3 - context
//          r4 - varType
//
// Out:     r2 - used
//               0 if not found or, if var_type_out != 3, then NOT the number of bytes required
//               for the variable
//          r3 - contextOut (X version only)
//          r4 - varTypeOut
//
// Returns: r2 (always positive)
//
// Calls SWI OS_ReadVarValSize (0x23) with R2 = 0x80000000, R1 = 0

uint32_t readVarValSize(char const* var, VarType varType);

// Reads a variable value (PRM 1-309, 5a-661) - Prefer OS_ReadVarValSize to read size of
// variable
//
// In:      r0 - var
//          r1 - value
//          r2 - size
//          r3 - context
//          r4 - varType
//
// Out:     r2 - used
//          r3 - contextOut (X version only)
//          r4 - varTypeOut
//
// Returns: r3 - contextOut (non-X version only)
//
// Calls SWI OS_ReadVarValSize (0x23)

MyError xreadVarVal(char const* var, char* value, int size, int context, VarType varType, int& used, int& contextOut, VarType& varTypeOut);

// Writes a variable value
//
// In:      r0 - var
//          r1 - value
//          r2 - size
//          //r3 - context
//          r4 - varType
//
// Out:     //r3 - contextOut
//          //r4 - varTypeOut
//
// Calls SWI OS_SetVarVal (0x24)

MyError setVarVal(char const* var, void const* value, size_t size, VarType varType);


MyError xgsTrans(char const* in, char* out, int32_t& len, uint32_t flags, int& truncated);
MyError xbinaryToDecimal(int32_t value, char* buffer, int32_t& len);
MyError xreadEscapeState(bool& escaped);

// Reads a series of VDU variables
//
// In:      r0 - varList
//          r1 - valueList
//
// Calls SWI OS_ReadVduVariables (0x31)

MyError xreadVduVariables(VDUVarList const* varList, uint32_t* valueList);

// Calls PaletteV vector directly and reads the palette
//
// In:      r0 - entry
//          r1 - colourType
//
// Out:     r2 - on
//          r3 - off
//          r4 - incomplete (X version only)
//
// Returns: r4 - incomplete (non-X version only)
//
// Calls SWI OS_CallAVector (0x34) with R9 = 0x23 (PaletteV), R4 = 1 (ReadEntry)

MyError xpaletteVReadEntry(uint32_t entry, int colourType, Colour& on, Colour& off, bool& incomplete);
MyError readPalette(uint32_t logical, uint32_t type, uint32_t& firstFlash, uint32_t& secondFlash);

// Reads information about a screen mode
//
// In:      r0 - mode
//          r1 - var
//
// Out:     r2 - varVal
//
// Returns: PSR (non-X version only)
//
// Calls SWI OS_ReadModeVariable (0x35)

MyError xreadModeVariable(Mode mode, ModeVar var, uint32_t& varVal);
MyError xreadModeVariable(Mode mode, ModeVar var, uint32_t& varVal, unsigned int& psrOut);
MyError xcallAVector(uint32_t vector,
                     uint32_t* r0,
                     uint32_t* r1,
                     uint32_t* r2,
                     uint32_t* r3,
                     uint32_t* r4,
                     uint32_t* r5,
                     uint32_t* r6,
                     uint32_t* r7);

// Installs a handler
//
// In:      r0 - handlerType
//          r1 - handler
//          r2 - handle
//          r3 - buffer
//
// Out:     r1 - oldHandler (X version only)
//          r2 - oldHandle
//          r3 - oldBuffer
//
// Returns: r1 - oldHandler (non-X version only)
//
// Calls SWI OS_ChangeEnvironment (0x40)

MyError xchangeEnvironment(HandlerType handlerType, void* handler, void** handle, void** buffer, void*& oldHandler, void**& oldHandle, void**& oldBuffer);
MyError xchangeDynamicArea(DynamicAreaNo area, int32_t change);
MyError xserviceCall(uint32_t code);
MyError xcli(char const* command);
MyError xwriteI(uint32_t value);
MyError xaddCallBack(void* callBack, uint32_t handle);
MyError xupCall(uint32_t r0, uint32_t r1, void* r2);
MyError xwriteC(uint32_t value);
MyError xwriteN(const void* buffer, uint32_t len);
MyError xsetColour(uint32_t flags, uint32_t colour);
MyError xword(uint32_t reason, void* block);

// Reads the space allocation of a dynamic area
//
// In:      r0 - area
//
// Out:     r0 - areaStart (X version only)
//          r1 - size
//          r2 - sizeLimit
//
// Returns: r0 - areaStart (non-X version only)
//
// Calls SWI OS_ReadDynamicArea (0x5C)

MyError readDynamicArea(DynamicAreaNo area, void*& areaStart, int& size, int& sizeLimit);

} } // namespace riscos::OS

#endif
