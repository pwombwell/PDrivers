#pragma once

#include "Point.h"
#include "Offset.h"
#include "Size.h"
#include "RLib/Base/NumericLimits.h"

#include <stdint.h>

namespace riscos {
namespace Geometry {

// Rectangle class.
//
// Requires Base::NumericLimits<T> to be specialised.
template<typename T>
struct Rect {
    T x0, y0, x1, y1;

    Rect() { }
    Rect(T x0, T y0, T x1, T y1) : x0(x0), y0(y0), x1(x1), y1(y1) { }
    Rect(Point<T> bottomLeft, Point<T> topRight)
        : x0(bottomLeft.x), y0(bottomLeft.y), x1(topRight.x), y1(topRight.y)
    { }
    Rect(Point<T> bottomLeft, Size<T> size)
        : x0(bottomLeft.x), y0(bottomLeft.y), x1(x0 + size.width), y1(y0 + size.height)
    { }
    Rect(Offset<T> bottomLeft, Size<T> size)
        : x0(bottomLeft.dx), y0(bottomLeft.dy), x1(x0 + size.width), y1(y0 + size.height)
    { }

    T width() const { return x1 - x0; }
    T height() const { return y1 - y0; }

    Point<T> bottomLeft() const { return Point<T>(x0, y0); }
    Point<T> topRight() const { return Point<T>(x1, y1); }

    Offset<T> bottomLeftOffset() const { return Offset<T>(x0, y0); }
    Size<T> size() const { return Size<T>(x1-x0, y1-y0); }

    void setEmpty() {
        x0 = y0 = Base::numericMax((T*)0);
        x1 = y1 = Base::numericLowest((T*)0);
    }

    bool isEmpty() const { return x1 <= x0 || y1 <= y0; }

    void set(T left, T bottom, T right, T top) {
        x0 = left;
        y0 = bottom;
        x1 = right;
        y1 = top;
    }

    void offsetBy(T dx, T dy) {
        x0 += dx;
        y0 += dy;
        x1 += dx;
        y1 += dy;
    }

    void offsetBy(Offset<T> offset) {
        x0 += offset.dx; x1 += offset.dx;
        y0 += offset.dy; y1 += offset.dy;
    }

    void clipWith(const Rect& rect) {
        if (x0 < rect.x0) x0 = rect.x0;
        if (y0 < rect.y0) y0 = rect.y0;
        if (x1 > rect.x1) x1 = rect.x1;
        if (y1 > rect.y1) y1 = rect.y1;
    }
        
    void unionWith(const Rect& rect) {
        if (x0 > rect.x0) x0 = rect.x0;
        if (y0 > rect.y0) y0 = rect.y0;
        if (x1 < rect.x1) x1 = rect.x1;
        if (y1 < rect.y1) y1 = rect.y1;
    }
};

typedef Rect<int32_t> IntRect;

} } // namespace riscos::Geometry
