#ifndef RLIB_GEOMETRY_POINT_H
#define RLIB_GEOMETRY_POINT_H
#pragma once

#include "RLib/RLib.h"

#include "Offset.h"

namespace riscos {
namespace Geometry {

template<typename T> struct Offset;

template<typename T>
struct Point {
    T x;
    T y;

    Point() {}
    Point(const Point& rhs) = default;

    Point(const T& x, const T& y) : x(x), y(y) {}

    template<typename U> Point& operator+=(const Offset<U>& rhs) {
        x += rhs.dx; y += rhs.dy; return *this;
    }
    template<typename U> Point& operator-=(const Offset<U>& rhs) {
        x -= rhs.dx; y -= rhs.dy; return *this;
    }

    Point operator+(const Offset<T>& rhs) const {
        return Point<T>(x + rhs.dx, y + rhs.dy);
    }

    Point operator-(const Offset<T>& rhs) const {
        return Point<T>(x - rhs.dx, y - rhs.dy);
    }


    Offset<T> operator-(const Point<T>& rhs) const {
        return Offset<T>(x - rhs.x, y - rhs.y);
    }

    Point operator-() const { return Point(-x, -y); }

    bool operator==(const Point& rhs) const { return x == rhs.x && y == rhs.y; }

    Offset<T> toOffset() const { return Offset<T>(x, y); }
};

} } // namsepace riscos::Geometry

#endif
