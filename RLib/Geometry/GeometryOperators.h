#ifndef RLIB_GEOMETRY_GEOMETRY_OPERATORS_H
#define RLIB_GEOMETRY_GEOMETRY_OPERATORS_H

#pragma once

#include "Point.h"
#include "Offset.h"

namespace riscos {
namespace Geometry {

//template<typename T>
//inline Point<T> operator+(const Point<T>& point, const Offset<T>& offset)
//{
//    return Point<T>(point.x + offset.dx, point.y + offset.dy);
//}

//template<typename T>
//inline Point<T> operator-(const Point<T>& point, const Offset<T>& offset)
//{
//    return Point<T>(point.x - offset.dx, point.y - offset.dy);
//}

// ambiguity with member operator-. Seems to be a C++ spec bug?
// template<typename T>
// inline Offset<T> operator-(const Point<T>& lhs, const Point<T>& rhs)
// {
//     return Offset<T>(lhs.x - rhs.x, lhs.y - rhs.y);
// }

} } // namespace riscos::Geometry

#endif