#ifndef PDRIVERPDF_TYPES_H
#define PDRIVERPDF_TYPES_H

#include "RLib/Base/NumericLimits.h"
#include "RLibX/OS.h"

namespace PDF {

struct Point {
    Point() : m_value(0) {}
    Point(int32_t value) : m_value(value) {}
    Point(OS::Millipoint value) : m_value(value.raw()) {}

    int32_t raw() const { return m_value; }

    OS::Millipoint toMillipoint() const { return OS::Millipoint(m_value); }

    Point& operator+=(const Point& rhs) { m_value += rhs.m_value; return *this; }
    Point& operator-=(const Point& rhs) { m_value -= rhs.m_value; return *this; }

    Point operator+(const Point& rhs) const { return Point(m_value + rhs.m_value); }
    Point operator-(const Point& rhs) const { return Point(m_value - rhs.m_value); }

    Point operator-() const { return Point(-m_value); }

    bool operator==(const Point& rhs) const { return m_value == rhs.m_value; }
    bool operator!=(const Point& rhs) const { return m_value != rhs.m_value; }
    bool operator<(const Point& rhs) const { return m_value < rhs.m_value; }
    bool operator<=(const Point& rhs) const { return m_value <= rhs.m_value; }
    bool operator>(const Point& rhs) const { return m_value > rhs.m_value; }
    bool operator>=(const Point& rhs) const { return m_value >= rhs.m_value; }

private:
    int32_t m_value;
};

} // namespace PDF

namespace riscos {
namespace Base {

inline PDF::Point numericMax(PDF::Point*)
{
    return PDF::Point(0x7fffffff);
}

inline PDF::Point numericLowest(PDF::Point*)
{
    return PDF::Point((int32_t)0x80000000u);
}

} } // namespace riscos::Base

#endif
