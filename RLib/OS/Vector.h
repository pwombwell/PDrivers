#ifndef RLIB_OS_VECTOR
#define RLIB_OS_VECTOR

#include "RLib/RLib.h"

struct _kernel_oserror;

namespace riscos {

// Functions that need to flag they're returning by claiming the vector
// should return one of these. Any Errors can be cast to it.
// Values based on cmunge's return values - error pointer or bool.
class VectorReturn {
public:
    VectorReturn() : m_ret(0) { }
    VectorReturn(const MyError& error) { m_ret = uint32_t(uintptr_t(error.raw())); }
    /*explicit*/ VectorReturn(const _kernel_oserror* error) {
        m_ret = uint32_t(uintptr_t(error));
    }

    static VectorReturn claim() { VectorReturn v; v.m_ret = 0; return v; }
    static VectorReturn pass() { VectorReturn v; v.m_ret = 1; return v; }

    bool isError() const { return m_ret > 1; }
    bool isPass()  const { return m_ret == 1; }
    bool isClaim() const { return m_ret == 0; } // Claim = no error.

    int toCMHG() const { return (int)m_ret; }
    MyError toError() const { return MyError(*(const OS::ErrorBlock<1>*)m_ret); }
    _kernel_oserror* toCMHGTrap() const { return (_kernel_oserror*)m_ret; }

private:
    uint32_t m_ret;
};

// OS namespace from now on ---------------------------------------------------
namespace OS {

enum Vector {
    Vector_ColourV     = 0x22,
    Vector_DrawV       = 0x20,
    Vector_WrchV       = 0x03,
    Vector_SpriteV     = 0x1F,
    Vector_ByteV       = 0x06
};

// Module contains cleaner variants of claimVector and releaseVector.
// Note that:
//      `entry` is the entry point defined by cmhg.
//      `pw` is the Module's private word, which is an opaque pointer.
MyError claimVector(OS::Vector vector, void(*entry)(void), void* pw);
MyError releaseVector(OS::Vector vector, void (*entry)(void), void* pw);

} // namespace OS
} // namespace riscos

#endif
