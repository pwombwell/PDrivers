#ifndef RLIB_OSBYTE_H
#define RLIB_OSBYTE_H

class OSByte {
public:

enum Op {
    Op_AcknowledgeEscape    = 126 // 0x7e
};

enum Var {
    Var_EscapeState         = 229, // 0xe5
    Var_EscapeEffects       = 230  // 0xe6
};

// First special case of call to perform OS_Byte actions returning a result
//
// In:      r0 - op
//          r1 - r1
//          r2 - r2
//
// Out:     r1 - r1Out (X version only)
//
// Returns: r1 - r1Out (non-X version only)
//
// Calls SWI OS_Byte (0x6)

static MyError osbyte1(Op op, int r1, int r2, uint8_t& r1Out);
static MyError osbyte1(Var var, int r1, int r2, uint8_t& r1Out);
static MyError xbyte(uint32_t& r0, uint32_t& r1, uint32_t& r2);

// Reads an OS_Byte status variable
//
// In:      r0 - var
//
// Out:     r1 - value (X version only)
//
// Returns: r1 - value (non-X version only)
//
// Calls SWI OS_Byte (0x6) with R2 = 0xFF, R1 = 0

static MyError read(Var var, uint8_t& value);

// Writes an OS_Byte status variable
//
// In:      r0 - var
//          r1 - value
//
// Calls SWI OS_Byte (0x6) with R2 = 0
static MyError write(Var var, uint8_t value);

};

#endif
