#pragma once

#include "RLib/RLib.h"

namespace riscos {
namespace Geometry {

template<typename T>
struct Offset {
    T dx;
    T dy;

    Offset() = default;
    Offset(const Offset& rhs) = default;

    Offset(const T& dx, const T& dy) : dx(dx), dy(dy) {}

    Offset& operator+=(const Offset& rhs) { dx += rhs.dx; dy += rhs.dy; return *this; }
    Offset& operator-=(const Offset& rhs) { dx -= rhs.dx; dy -= rhs.dy; return *this; }

    Offset operator+(const Offset& rhs) const { return Offset(dx + rhs.dx, dy + rhs.dy); }
    Offset operator-(const Offset& rhs) const { return Offset(dx - rhs.dx, dy - rhs.dy); }
    Offset operator-() const { return Offset(-dx, -dy); }

    bool operator==(const Offset& rhs) const { return dx == rhs.dx && dy == rhs.dy; }
};

} } // namsepace riscos::Geometry
