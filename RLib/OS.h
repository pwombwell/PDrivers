#pragma once
#ifndef RLIB_OS_H
#define RLIB_OS_H

#include "OS/Colour.h"
#include "OS/Constants.h"
#include "OS/Error.h"
#include "OS/Mode.h"
#include "OS/Sprite.h"

namespace riscos { namespace OS {

// Register container, like _kernel_swi_regs, but can be passed by reference.
class Regs {
public:
    Regs() = default;
    Regs(const Regs&) = delete;

    uint32_t& operator[](size_t i) { return m_regs[i]; }
    uint32_t operator[](size_t i) const { return m_regs[i]; }

    // Helpers to avoid "implicit casts of 32-bit to pointer" warnings when
    // compiling on 64-bit hosts. Obviously the reg values must not contain
    // actual pointers on those machines.
    //
    // ...with the added bonus that they actually allow slightly nicer
    // getters for differing types. They'll be even better with 'auto'.
    template <typename T> T as(size_t i) {
        return (T)(uintptr_t)m_regs[i];
    }

    template <typename T> T as(size_t i) const {
        return (T)(uintptr_t)m_regs[i];
    }

    void set(size_t i, uint32_t value) { m_regs[i] = value; }

    template <typename T> void set(size_t i, const T* value) {
        m_regs[i] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(value));
    }

private:
    uint32_t m_regs[10];
};

} } // namespace OS

#endif
