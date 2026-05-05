#pragma once

#include <float.h>
#include <stdint.h>

namespace riscos {
namespace Base {

template<typename T>
struct NumericLimits;

template<typename T>
T numericMax(T*);

template<typename T>
T numericLowest(T*);

// Type traits for numeric-like types. Semantics follow std::numeric_limits:
// min()    = minimum finite value for integral types, and smallest positive
//            normal value for floating-point types.
// max()    = largest positive number.
// lowest() = most negative number.

template<>
struct NumericLimits<int32_t>
{
    static int32_t min()    { return (int32_t)0x80000000u; }
    static int32_t max()    { return 0x7fffffff; }
    static int32_t lowest() { return (int32_t)0x80000000u; }
};

inline int32_t numericMax(int32_t*)    { return 0x7fffffff; }
inline int32_t numericLowest(int32_t*) { return (int32_t)0x80000000u; }

template<>
struct NumericLimits<float>
{
    static float min()    { return FLT_MIN; }
    static float max()    { return FLT_MAX; }
    static float lowest() { return -FLT_MAX; }
};

inline float numericMax(float*)    { return FLT_MAX; }
inline float numericLowest(float*) { return -FLT_MAX; }

} } // namespace riscos::Base
