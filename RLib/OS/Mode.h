#pragma once
#ifndef RLIB_OS_MODE_H
#define RLIB_OS_MODE_H

#include <stdint.h>

namespace riscos {
namespace OS {

struct Mode;
struct ModeSelector;

// A Mode was replaced by a ModeSpecifier in RO3.5+.
//
// They can be a number (<256), a pointer to a ModeSelector or (in certain
// cases) a SpriteModeWord (of which, there are two versions).
//
// Not all APIs support all three types. In particular, a SpriteModeWord
// has no size info.
//
// https://www.riscosopen.org/wiki/documentation/show/Mode%20Specifier
class Mode {
public:
    Mode() { (void)this; }
    Mode(uint32_t number) { u.number = number; }
    Mode(ModeSelector* selector) { u.selector = selector; }
  
    bool isNumber() const   { return u.number < 256; }
    bool isSelector() const { return !isNumber() && (u.number & 1) == 0; }
    bool isModeWord() const { return !isSelector(); }
  
    uint32_t toNumber() const { return u.number; }
    ModeSelector* toSelector() const { return u.selector; }
  
private:
    union {
        uint32_t number;
        ModeSelector* selector;
    } u;
};

} } // namespace riscos::OS

#endif
