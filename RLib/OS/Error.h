#ifndef RLIB_OS_ERROR_H
#define RLIB_OS_ERROR_H

#include <stddef.h>
#include <stdint.h>

// Detect inclusion of <kernel.h>.
#ifdef __kernel_h
#define RO_KERNEL_INCLUDED
#endif

// OSLib lets us forward-declare os_error, but the optional namespace
// means it isn't as reliable as you'd like.
#ifdef NAMESPACE_OSLIB
namespace OSLib { struct os_error; }
#define OSLIB_ERROR OSLib::os_error
#else
struct os_error;
#define OSLIB_ERROR os_error
#endif

// Detect inclusion of OSLib's os.h.
#ifdef os_H
#define RO_OSLIB_INCLUDED
#endif

// Not compatible with old versions of kernel.h
struct _kernel_oserror;

namespace riscos {
namespace OS {

// Another attempt at a kernel_oserror replacement. Passed as pointer only as
// it's trailed by the message.
struct ErrorNumber {
    uint32_t errnum;

    const char* message() const { return (const char*)((&errnum)+1); }
};

template<size_t N> struct ErrorBlock;

// An unknown sized error block, with null-terminated error message (or tag)
// appended immediately after the block.
class ErrorView {
public:
    ErrorView(const ErrorNumber& b) : m_ptr(&b) { }
    ErrorView(const ErrorView& rhs) : m_ptr(rhs.m_ptr) { }
    template<size_t N>
    ErrorView(const ErrorBlock<N>& b)
        : m_ptr((const ErrorNumber*)(const void*)&b)
    {}

    ErrorView(const _kernel_oserror* e)
        : m_ptr((const ErrorNumber*)(const void*)(e))
    {
    }

#ifdef RO_OSLIB_INCLUDED
    ErrorView(const OSLIB_ERROR* e)
        : m_errnum((const uint32_t*)(e))
    {
    }
#endif

    uint32_t errnum() const { return m_ptr->errnum; }
    const char* message() const { return m_ptr->message(); }

    const ErrorNumber* data() const { return m_ptr; }

private:
    const ErrorNumber* m_ptr;
};

// A RISC OS error block, usually created by the helpers below.
// 
// It can be passed to any method that takes a `const ErrorView&`.
template<size_t N> struct ErrorBlock {
    ErrorNumber errnum;
    char        errmess[N];

    static_assert(N <= 252, "Error block string too long.");

    // implicit and explicit convertors.
//    operator const ErrorNumber&() const { return errnum; }
//    const ErrorNumber& number() const { return errnum; }
//    ErrorView view() const { return ErrorView(*this); }
};

// An ErrorBlock that's the maximum size RISC OS supports.
typedef ErrorBlock<252> MaxErrorBlock;

// Insert an inline static error block with tag, matching the behaviour
// of RISC OS macro (in ObjAsm) MakeInternatErrorBlock.
//
// Creates ErrorBlock_##Name as the real storage object.
#define MAKE_INTERNAT_ERROR_BLOCK(Name,Tag) \
    const riscos::OS::ErrorBlock<sizeof(Tag)> ErrorBlock_##Name = \
        { { ErrorNumber_##Name }, Tag }

#define DECLARE_INTERNAT_ERROR_BLOCK(Name,Tag) \
    extern const riscos::OS::ErrorBlock<sizeof(Tag)> ErrorBlock_##Name

static_assert(offsetof(ErrorBlock<8>, errnum) == 0, "errnum not first");
static_assert(offsetof(ErrorBlock<8>, errmess) == 4, "errmess not at +4");

} } // namespace riscos::OS

#endif
