#ifndef RLIB_H
#define RLIB_H

#pragma once

#include "OS/Error.h"

#include <stddef.h>
#include <stdint.h>

typedef int32_t FileHandle;
typedef uint32_t FontHandle;

#if defined(__CC_NORCROFT)
#define nullptr 0
typedef int nullptr_t;
#ifndef INT64_MAX
typedef long long int64_t;
typedef unsigned long long uint64_t;
#endif
#endif // CC_NORCROFT

// Not compatible with old versions of kernel.h
struct _kernel_oserror;

namespace riscos {

// Forward-declare a fixed-size error block (defined in OS/Error.h, if needed).
namespace OS { template<size_t N> struct ErrorBlock; }

// Holds a pointer to a RISC OS variable-sized error block.
class MyError {
public:
    MyError() : m_ptr(nullptr) { }
    MyError(const MyError& rhs) : m_ptr(rhs.m_ptr) { }
    /* explicit */ MyError(nullptr_t) : m_ptr(nullptr) { }
    template<size_t N> MyError(const OS::ErrorBlock<N>& block)
        : m_ptr(&block.errnum)
    { }

    static MyError OOM();

    /*explicit*/ operator bool() const { return m_ptr != nullptr; }

    uint32_t errnum() const { return m_ptr ? m_ptr->errnum : 0; }
    OS::ErrorNumber number() const { return *m_ptr; }
    const char* message() const { return m_ptr ? m_ptr->message() : nullptr; }
    const char* tag() const { return message(); }
    const OS::ErrorNumber* raw() const { return m_ptr; }

    bool operator==(const MyError& rhs) const { return m_ptr == rhs.m_ptr; }
    bool operator==(nullptr_t) const { return m_ptr == nullptr; }
    bool operator!=(nullptr_t) const { return m_ptr != nullptr; }

    // _kernel_oserror* wrapper. If <kernel.h> has been included, allow
    // easy conversions to/from it. This provides a much safer wrapper that
    // is both const-safe and does not assume the size of the return error
    // block is 256 bytes.
    /*explicit*/ MyError(const _kernel_oserror* error)
        : m_ptr((const OS::ErrorNumber*)(const void*)error)
    { }
    // Explicit cast to `_kernel_oserror*` for safety. A _kernel_oserror is:
    //  - incorrectly 256 bytes but often points to an object smaller
    //    than that. Bytes after the terminator must not be accessed.
    //  - not const correct.
    _kernel_oserror* toKernelOSError() const { return (_kernel_oserror*)(void*)m_ptr; }

#ifdef OSLIB_ERROR
    // OSLib os_error* wrapper.
    // It has the same theoretical problems as _kernel_oserror, above.
    // If these fail to link, you have not included OSLib:os.h before
    // including this header. See RO_OSLIB_INCLUDED below.
    /*explicit*/ MyError(const OSLIB_ERROR* error);
    OSLIB_ERROR* toOSLib() const;
#endif

private:
    const OS::ErrorNumber* m_ptr;
};

#ifdef RO_OSLIB_INCLUDED
// Can be optimised to a no-op if these are in the header file, but does
// require OSLib:os.h to be included first.
MyError::MyError(const OSLIB_ERROR* error)
    : m_ptr((const OS::ErrorNumber*)(const void*)error)
{ }
OSLIB_ERROR* MyError::toOSLib() const { return (OSLIB_ERROR*)m_ptr; }
#endif

// Not sure this should be here.
template<typename T>
static const T& min(const T& x, const T& y) { return x < y ? x : y; }

} // namespace riscos

using namespace riscos;

#endif
