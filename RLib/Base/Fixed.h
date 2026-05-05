#pragma once
#ifndef RLIB_UTILS_FIXED_H
#define RLIB_UTILS_FIXED_H

#include "NumericLimits.h"

#include <stdint.h>

namespace riscos {
namespace OS { class Unit; }

namespace Base {

template<uint8_t Shift> class Fixed64;

// 32-bit fixed-point value. Multiplies truncate.
template<uint8_t Shift>
class Fixed
{
public:
    // enum { kShift = Shift };

    Fixed() : m_v(0) {}
    Fixed(const Fixed& rhs) : m_v(rhs.m_v) {}
//    explicit Fixed(int32_t i) : m_v(i << Shift) {}

    static Fixed zero() { return fromRaw(0); }

    Fixed& operator=(const Fixed& rhs) {
        m_v = rhs.m_v;
        return *this;
    }

    // Construct from an integer.
    static Fixed fromInt(int32_t i) {
        Fixed r;
        r.m_v = i << Shift;
        return r;
    }

    // Construct from raw underlying fixed-point bits.
    static Fixed fromRaw(int32_t raw) {
        Fixed r;
        r.m_v = raw;
        return r;
    }

    int32_t raw() const { return m_v; }

    int32_t toIntTruncated() const { return m_v >> Shift; }

    Fixed operator-() const { return Fixed::fromRaw(-m_v); }

    Fixed& operator+=(const Fixed& o) { m_v += o.m_v; return *this; }
    Fixed& operator-=(const Fixed& o) { m_v -= o.m_v; return *this; }

    template<uint8_t S> Fixed& operator*=(const Fixed<S>& v) {
        m_v = (int32_t)(((int64_t)m_v * v.raw()) >> S); return *this;
    }
    Fixed& operator*=(int32_t i) { m_v *= i; return *this; }
    //Fixed& operator/=(int32_t i); // wrong rounding: { m_v /= i; return *this; }
    Fixed& operator/=(int32_t i) {
        m_v = (m_v >= 0) ? ((m_v + i / 2) / i)
                         : -(((-m_v) + i / 2) / i);
        return *this;
    }

    Fixed operator+(const Fixed& o) const { Fixed r(*this); r += o; return r; }
    Fixed operator-(const Fixed& o) const { Fixed r(*this); r -= o; return r; }
    Fixed operator*(int32_t i) const { Fixed r(*this); r *= i; return r; }
    Fixed operator/(int32_t i) const { Fixed r(*this); r /= i; return r; }

    bool operator==(const Fixed& o) const { return m_v == o.m_v; }
    bool operator!=(const Fixed& o) const { return m_v != o.m_v; }
    bool operator< (const Fixed& o) const { return m_v < o.m_v; }
    bool operator<=(const Fixed& o) const { return m_v <= o.m_v; }
    bool operator> (const Fixed& o) const { return m_v > o.m_v; }
    bool operator>=(const Fixed& o) const { return m_v >= o.m_v; }

    // Explicit 32x32 bit -> 32 bit mul, truncating.
    template<uint8_t S> Fixed mul(const Fixed<S>& rhs) const {
        return Fixed::fromRaw(int64_t(m_v) * rhs.raw() >> S);
    }

    // Explicit 32x32 bit -> 64 bit mul.
    //template<uint8_t S> Fixed64<Shift+S> mul64(const Fixed<S>& rhs) const;
    // Norcroft can't cope with the templated variant.
    Fixed64<16> mul64(const Fixed<8>& rhs) const;
    Fixed64<16> mul64(const Fixed<16>& rhs) const;

    // Norcroft can't do Fixed64<Shift> mul64(int i) const;
    Fixed64<16> mul64(int i) const;

protected:
    int32_t m_v;
};

// 64-bit fixed-point container. Intended for temporary results with Fixed<>.
template<uint8_t Shift>
class Fixed64
{
public:
//    enum { kShift = Shift }; // Norcroft doesn't like this

    Fixed64() : m_v(0) {}
    Fixed64(const Fixed64& rhs) : m_v(rhs.m_v) {}
//    explicit Fixed(int32_t i) : m_v(i << Shift) {}

    Fixed64& operator=(const Fixed64& rhs) {
        m_v = rhs.m_v;
        return *this;
    }

    // Construct from raw underlying fixed-point bits.
    static Fixed64 fromRaw(int64_t raw) {
        Fixed64 r;
        r.m_v = raw;
        return r;
    }

    int64_t raw() const { return m_v; }

    // template<uint8_t S>
    // Fixed<S> toFixed() const { return Fixed<S>::fromRaw(m_v >> (Shift - S)); }
    Fixed<8> toFixed() const { return Fixed<8>::fromRaw(m_v >> (Shift - 8)); }

    Fixed64 operator-() const { return Fixed64::fromRaw(-m_v); }

    Fixed64& operator+=(const Fixed64& o) { m_v += o.m_v; return *this; }
    Fixed64& operator-=(const Fixed64& o) { m_v -= o.m_v; return *this; }

    template<uint8_t S> Fixed64& operator*=(const Fixed<S>& v) {
        m_v = (m_v * v.raw()) >> S; return *this;
    }
    Fixed64& operator*=(int32_t i) { m_v *= i; return *this; }
    Fixed64& operator/=(int32_t i); // wrong rounding: { m_v /= i; return *this; }

    Fixed64 operator+(const Fixed64& o) const { Fixed64 r(*this); r += o; return r; }
    Fixed64 operator-(const Fixed64& o) const { Fixed64 r(*this); r -= o; return r; }
    Fixed64 operator*(int32_t i) const { Fixed64 r(*this); r *= i; return r; }
    Fixed64 operator/(int32_t i) const { Fixed64 r(*this); r /= i; return r; }

//    template<uint8_t S>
//    Fixed<Shift - S> operator/(Fixed<S> o) const {
//        static_assert(Shift > S, "Fixed64 must have a larger shift than Fixed.");
//        return Fixed<Shift-S>::fromRaw(m_v / o.raw());
//    }

//    Fixed<8> operator/(const Fixed<8>& b) const { return Fixed<8>::fromRaw(m_v / b.raw()); }

    bool operator==(const Fixed64& o) const { return m_v == o.m_v; }
    bool operator!=(const Fixed64& o) const { return m_v != o.m_v; }
    bool operator< (const Fixed64& o) const { return m_v < o.m_v; }
    bool operator<=(const Fixed64& o) const { return m_v <= o.m_v; }
    bool operator> (const Fixed64& o) const { return m_v > o.m_v; }
    bool operator>=(const Fixed64& o) const { return m_v >= o.m_v; }

private:
    int64_t m_v;
};

#if 0
// Norcroft doesn't seem keen on the templated versions.
template<uint8_t Shift>
template<uint8_t S>
Fixed64<Shift + S> Fixed<Shift>::mul64(const Fixed<S>& rhs) const
{
    return Fixed64<Shift + S>::fromRaw((int64_t)m_v * rhs.raw());
}

template<uint8_t Shift1, uint8_t Shift2>
Fixed<Shift1 - Shift2> operator/(const Fixed64<Shift1>& a, const Fixed<Shift2>& b) {
    static_assert(Shift1 > Shift2, "Fixed64 must have a larger shift than Fixed.");
    return Fixed<Shift1-Shift2>::fromRaw(a.raw() / b.raw());
}
#endif

template<> inline Fixed64<16> Fixed<8>::mul64(const Fixed<8>& rhs) const {
    return Fixed64<16>::fromRaw(int64_t(m_v) * rhs.raw());
}

template<> inline Fixed64<16> Fixed<8>::mul64(const Fixed<16>& rhs) const {
    return Fixed64<16>::fromRaw(int64_t(m_v) * rhs.raw() >> 8);
}

//template<uint8_t Shift> inline Fixed64<Shift> Fixed<Shift>::mul64(int i) const {
//    return Fixed64<Shift>::fromRaw(int64_t(m_v) * i);
//}
template<> inline Fixed64<16> Fixed<16>::mul64(int i) const {
    return Fixed64<16>::fromRaw(int64_t(m_v) * i);
}

inline Fixed<8> operator/(const Fixed64<16>& a, const Fixed<8>& b) {
    return Fixed<8>::fromRaw(a.raw() / b.raw());
}

// Fails on Norcroft C++.
// template<uint8_t Shift>
// struct NumericLimits< Fixed<Shift> >
// {
//     static Fixed<Shift> min() {
//         return Fixed<Shift>::fromRaw(1);
//     }
// 
//     static Fixed<Shift> max() {
//         return Fixed<Shift>::fromRaw(0x7fffffff);
//     }
// 
//     static Fixed<Shift> lowest() {
//         return Fixed<Shift>::fromRaw((int32_t)0x80000000u);
//     }
// };
template<uint8_t Shift>
inline Fixed<Shift> numericMax(Fixed<Shift>*) {
    return Fixed<Shift>::fromRaw(0x7fffffff);
}
template<uint8_t Shift>
inline Fixed<Shift> numericLowest(Fixed<Shift>*) {
    return Fixed<Shift>::fromRaw((int32_t)0x80000000u);
}

typedef Fixed<8> _DrawUnit;

} } // namespace riscos::Base

#endif
