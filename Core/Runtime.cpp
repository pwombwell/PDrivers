#include <stddef.h>
#include <stdint.h>

#include "OS.h"

extern "C" void __vc__FPvT1iPFPv_v(void* start,
                                   void* limit,
                                   int stride,
                                   void (*mapfn)(void*))
{
    uint8_t* p = (uint8_t*)start;
    uint8_t* end = (uint8_t*)limit;

    while (p != end) {
        mapfn(p);
        p += stride;
    }
}

typedef unsigned int u32;
typedef signed int s32;

typedef union U64View {
    struct {
        u32 lo;
        u32 hi;
    } u;
    struct {
        u32 lo;
        s32 hi;
    } s;
    unsigned long long uv;
    long long sv;
} U64View;

static int u64UnsignedCompare(u32 ahi, u32 alo, u32 bhi, u32 blo)
{
    if (ahi != bhi) {
        return (ahi > bhi) ? 1 : -1;
    }
    if (alo != blo) {
        return (alo > blo) ? 1 : -1;
    }
    return 0;
}

static int u64SignedCompare(s32 ahi, u32 alo, s32 bhi, u32 blo)
{
    if (ahi != bhi) {
        return (ahi > bhi) ? 1 : -1;
    }
    if (alo != blo) {
        return (alo > blo) ? 1 : -1;
    }
    return 0;
}

extern "C" int _ll_cmpeq(long long a, long long b)
{
    U64View ua;
    U64View ub;

    ua.uv = (unsigned long long)a;
    ub.uv = (unsigned long long)b;
    return u64UnsignedCompare(ua.u.hi, ua.u.lo, ub.u.hi, ub.u.lo) == 0;
}

extern "C" int _ll_cmpne(long long a, long long b)
{
    U64View ua;
    U64View ub;

    ua.uv = (unsigned long long)a;
    ub.uv = (unsigned long long)b;
    return u64UnsignedCompare(ua.u.hi, ua.u.lo, ub.u.hi, ub.u.lo) != 0;
}

extern "C" int _ll_scmpgt(long long a, long long b)
{
    U64View ua;
    U64View ub;

    ua.uv = (unsigned long long)a;
    ub.uv = (unsigned long long)b;
    return u64SignedCompare(ua.s.hi, ua.s.lo, ub.s.hi, ub.s.lo) > 0;
}

extern "C" int _ll_scmplt(long long a, long long b)
{
    U64View ua;
    U64View ub;

    ua.uv = (unsigned long long)a;
    ub.uv = (unsigned long long)b;
    return u64SignedCompare(ua.s.hi, ua.s.lo, ub.s.hi, ub.s.lo) < 0;
}

extern "C" int _ll_ucmpgt(unsigned long long a, unsigned long long b)
{
    U64View ua;
    U64View ub;

    ua.uv = a;
    ub.uv = b;
    return u64UnsignedCompare(ua.u.hi, ua.u.lo, ub.u.hi, ub.u.lo) > 0;
}

extern "C" int _ll_ucmpge(unsigned long long a, unsigned long long b)
{
    U64View ua;
    U64View ub;

    ua.uv = a;
    ub.uv = b;
    return u64UnsignedCompare(ua.u.hi, ua.u.lo, ub.u.hi, ub.u.lo) >= 0;
}
