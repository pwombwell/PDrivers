#ifndef RLIB_KERNEL_REGS
#define RLIB_KERNEL_REGS

#include "kernel.h"
#include "OS.h"

// Reduce the requirement for kernel.h to be included everywhere.
namespace riscos { namespace OS {

// Wrap a non-null _kernel_swi_regs, on entry from an OS entry point, such as
// a SWI handler.
//
// It can then be passed to a function (that doesn't include this header file)
// that takes either a "const OS::Regs&" or "OS::Regs&" depending on whether
// the function can modify the registers for return.
class KernelRegsWrapper {
public:
    /*explicit*/ KernelRegsWrapper(_kernel_swi_regs* regs) { u.m_kernel = regs; }
    /*explicit*/ KernelRegsWrapper(OS::Regs& regs) { u.m_rlib = &regs; }

    operator Regs&() { return *u.m_rlib; }
    operator const Regs&() const { return *u.m_rlib; }

    operator _kernel_swi_regs*() { return u.m_kernel; }

    uint32_t& operator[](size_t i) {
        return (*u.m_rlib)[i];
    }

    uint32_t operator[](size_t i) const {
        return (*u.m_rlib)[i];
    }

    // Helpers to avoid "implicit casts of 32-bit to pointer" warnings when
    // compiling on 64-bit hosts. Obviously the reg values must not contain
    // actual pointers on those machines.
    //
    // ...with the added bonus that they actually allow slightly nicer
    // getters for differing types. They'll be even better with 'auto'.
    template <typename T> T as(size_t i) {
        return reinterpret_cast<T>(static_cast<uintptr_t>((*u.m_rlib)[i]));
    }

    template <typename T> T as(size_t i) const {
        return reinterpret_cast<const T>(static_cast<uintptr_t>((*u.m_rlib)[i]));
    }

    void set(size_t i, uint32_t value) { (*u.m_rlib)[i] = value; }

    template <typename T> void set(size_t i, const T* value) {
        u.m_kernel->r[i] = static_cast<int32_t>(reinterpret_cast<uintptr_t>(value));
    }

private:
    union {
        _kernel_swi_regs*   m_kernel;
        Regs*               m_rlib;
    } u;
};

} } // namespace riscos::OS

#endif
