#ifndef PDRIVERPS_TYPES_H
#define PDRIVERPS_TYPES_H

#include "RLib/Base/NumericLimits.h"

namespace PS {

// A Postscript point is 1/72th of an inch.
// OS::Unit = 1/180th of an inch = 2/5 point
// OS::Millipoint = 1/1000 point
struct Unit {
    Unit() : m_value(0) {}
    Unit(int32_t value) : m_value(value) {}

    int32_t raw() const { return m_value; }

    Unit& operator+=(const Unit& rhs) { m_value += rhs.m_value; return *this; }
    Unit& operator-=(const Unit& rhs) { m_value -= rhs.m_value; return *this; }

    Unit operator+(const Unit& rhs) const { return Unit(m_value + rhs.m_value); }
    Unit operator-(const Unit& rhs) const { return Unit(m_value - rhs.m_value); }

    Unit operator-() const { return Unit(-m_value); }

    bool operator==(const Unit& rhs) const { return m_value == rhs.m_value; }
    bool operator!=(const Unit& rhs) const { return m_value != rhs.m_value; }
    bool operator<(const Unit& rhs) const { return m_value < rhs.m_value; }
    bool operator<=(const Unit& rhs) const { return m_value <= rhs.m_value; }
    bool operator>(const Unit& rhs) const { return m_value > rhs.m_value; }
    bool operator>=(const Unit& rhs) const { return m_value >= rhs.m_value; }

private:
    int32_t m_value;
};

} // namespace PS

namespace riscos {
namespace Base {

template<>
inline PS::Unit numericMax<PS::Unit>(PS::Unit*)
{
    return PS::Unit(0x7fffffff);
}

template<>
inline PS::Unit numericLowest<PS::Unit>(PS::Unit*)
{
    return PS::Unit((int32_t)0x80000000u);
}

} } // namespace riscos::Base

#endif
