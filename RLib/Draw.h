#ifndef RLIB_DRAW_H
#define RLIB_DRAW_H
#pragma once

#include "RLib/RLib.h"
#include "RLibX/OS.h" // For OS::Unit - clearly in the wrong directory
#include "Geometry.h"
#include "Base/Fixed.h"

namespace riscos {
namespace Draw {

enum DrawOp {
    DrawOp_End              = 0,
    DrawOp_Continue         = 1, // [->pointer to continuation path]
    DrawOp_Move             = 2, // [x, y] Start path.
    DrawOp_MoveInternal     = 3, // [x, y] Move - don't start new path.
    DrawOp_CloseWithGap     = 4,
    DrawOp_CloseWithLine    = 5,
    DrawOp_Bezier           = 6, // [cx1, cy1, cx2, cy2, x, y]
    DrawOp_Gap              = 7, // [x, y] Move - don't start new path.
    DrawOp_Line             = 8  // [x, y] Line.
};

// A Draw::Unit is a [24:8] fixed point type.
typedef Base::Fixed<8> Unit;
typedef Base::Fixed<16> TransformUnit;
typedef Geometry::Rect<Unit> Rect;
typedef Geometry::Point<Unit> Point;
typedef Geometry::Offset<Unit> Offset;

struct Matrix {
    TransformUnit a, b, c, d;
    Unit e, f;

    static Matrix identity() {
        Matrix matrix;
        matrix.a = TransformUnit::fromInt(1);
        matrix.b = TransformUnit::fromInt(0);
        matrix.c = TransformUnit::fromInt(0);
        matrix.d = TransformUnit::fromInt(1);
        matrix.e = Unit::fromInt(0);
        matrix.f = Unit::fromInt(0);
        return matrix;
    }

    static Matrix fromRaw4(const int32_t* raw) {
        Matrix matrix;
        matrix.a = TransformUnit::fromRaw(raw[0]);
        matrix.b = TransformUnit::fromRaw(raw[1]);
        matrix.c = TransformUnit::fromRaw(raw[2]);
        matrix.d = TransformUnit::fromRaw(raw[3]);
        matrix.e = Unit::fromInt(0);
        matrix.f = Unit::fromInt(0);
        return matrix;
    }

    static Matrix fromRaw6(const int32_t* raw) {
        Matrix matrix;
        matrix.a = TransformUnit::fromRaw(raw[0]);
        matrix.b = TransformUnit::fromRaw(raw[1]);
        matrix.c = TransformUnit::fromRaw(raw[2]);
        matrix.d = TransformUnit::fromRaw(raw[3]);
        matrix.e = Unit::fromRaw(raw[4]);
        matrix.f = Unit::fromRaw(raw[5]);
        return matrix;
    }

    int32_t* raw() {
        return (int32_t*)(void*)this;
    }

    const int32_t* raw() const {
        return (const int32_t*)(const void*)this;
    }

    bool isIdentity() const {
        return a == TransformUnit::fromInt(1) &&
               b.raw() == 0 &&
               c.raw() == 0 &&
               d == TransformUnit::fromInt(1) &&
               e.raw() == 0 &&
               f.raw() == 0;
    }

    void setIdentity() {
        *this = identity();
    }

    void scaleX(const TransformUnit& s) {
        a *= s;
        c *= s;
        e *= s;
    }

    void scaleY(const TransformUnit& s) {
        b *= s;
        d *= s;
        f *= s;
    }

    void translate(const Geometry::Offset<Base::Fixed<8> >& s) {
        e += s.dx;
        f += s.dy;
    }

    Offset translate() const { return Offset(e, f); }

    Point transformPoint(const Point& p) const {
        int64_t x = ((int64_t)a.raw() * p.x.raw()) + ((int64_t)c.raw() * p.y.raw());
        int64_t y = ((int64_t)b.raw() * p.x.raw()) + ((int64_t)d.raw() * p.y.raw());
        return Point(Unit::fromRaw(x >> 16) + e, Unit::fromRaw(y >> 16) + f);
    }
};

// Used more in Printer drivers and sprites than Draw.
struct DimensionlessTransform {
    TransformUnit a, b, c, d;
};

// A path consists of a DrawOp, a Draw::Unit, or a pointer to a continuation
// path. Only parsing the path can tell you what it actually is.
class PathComponent {
public:
    PathComponent() : m_v(0) { }
    PathComponent(DrawOp op) : m_v((uint32_t)op) { }
    PathComponent(Unit unit) : m_v((uint32_t)unit.raw()) { }
    PathComponent(const PathComponent* path) : m_v(uintptr_t(path)) { }

    PathComponent& operator=(const PathComponent& rhs) {
        m_v = rhs.m_v;
        return *this;
    }

    PathComponent& operator=(DrawOp op) {
        m_v = (uint32_t)op;
        return *this;
    }

    PathComponent& operator=(Unit unit) {
        m_v = (uint32_t)unit.raw();
        return *this;
    }

    bool operator==(DrawOp op) const { return DrawOp(m_v) == op; }
    bool operator>(DrawOp op) const { return DrawOp(m_v) > op; }

    DrawOp op() const { return (DrawOp)m_v; }
    Unit unit() const { return Unit::fromRaw((int32_t)m_v); }
    PathComponent* continuation() const { return (PathComponent*)m_v; }

private:
    friend class Path;

    static PathComponent raw(uint32_t value) {
        PathComponent t;
        t.m_v = value;
        return t;
    }

private:
    uint32_t m_v;
};

// FIXME: Should become a PathIterator.
class Path {
public:
    enum { OutputHeaderBytes = 2 * sizeof(PathComponent) };

    Path() : m_components(nullptr) { }
    Path(PathComponent* components) : m_components(components) { }
    Path(const PathComponent* components) : m_components((PathComponent*)components) { }

    PathComponent* components() { return m_components; }
    const PathComponent* components() const { return m_components; }

    bool isNull() const { return m_components == nullptr; }
    bool operator==(const Path& rhs) const { return m_components == rhs.m_components; }
    bool operator!=(const Path& rhs) const { return m_components != rhs.m_components; }

    DrawOp op() const { return m_components->op(); }

    DrawOp readOp() {
        DrawOp op = m_components->op();
        ++m_components;
        return op;
    }

    Point readPoint() {
        Point point(m_components[0].unit(), m_components[1].unit());
        skipPoint();
        return point;
    }

    void skipPoint() { m_components += 2; }
    void skipBezier() { m_components += 6; }
    void continueAt() { m_components = m_components->continuation(); }

    void initialiseOutputBuffer(uint32_t byteSize) {
        m_components[0] = DrawOp_End;
        m_components[1] = PathComponent::raw(byteSize - OutputHeaderBytes);
    }

private:
    PathComponent* m_components;
};

// Some explicit conversions.
inline Unit fromOSUnit(OS::Unit v) {
    return Unit::fromRaw(v.raw() << 8);
}

inline Point fromOSUnit(const Geometry::Point<OS::Unit>& p) {
    return Point(fromOSUnit(p.x), fromOSUnit(p.y));
}

inline Geometry::Offset<Unit> fromOSUnit(const Geometry::Offset<OS::Unit>& p) {
    return Geometry::Offset<Unit>(fromOSUnit(p.dx), fromOSUnit(p.dy));
}

// Note that the data is appended after this block.
// TODO: Probably template it like OS::ErrorBlock.
struct DashPattern {
    Draw::Unit m_startOffset;
    uint32_t   m_elementCount;

    Draw::Unit startOffset() const { return m_startOffset; }
    uint32_t elementCount() const { return m_elementCount; }

    Draw::Unit element(uint32_t index) const {
        const Draw::Unit* elements = (const Draw::Unit*)(const void*)(this + 1);
        return elements[index];
    }
};

template<size_t N>
struct SizedDashPattern : DashPattern {
    Draw::Unit m_element[N];
};

// Todo: Unionise?
struct CapJoin {
    uint32_t   m_capJoinWord;
    Draw::Unit m_miterLimit;

    uint8_t joinStyle() const {
        return (uint8_t)(m_capJoinWord & 0xff);
    }

    uint8_t startCapStyle() const {
        return (uint8_t)((m_capJoinWord >> 8) & 0xff);
    }

    uint8_t endCapStyle() const {
        return (uint8_t)((m_capJoinWord >> 16) & 0xff);
    }

    Draw::Unit miterLimit() const {
        return m_miterLimit;
    }

    bool isSimpleCapStyle() const {
        return startCapStyle() <= 2 && startCapStyle() == endCapStyle();
    }
};

} // namespace Draw

// Some implicit conversions - only OS::Unit to Draw::Unit since the other
// way round is probably a bug.
// These are best placed outside in a common namespace so they're
// found by g++.
inline Draw::Unit& operator+=(Draw::Unit& lhs, const OS::Unit& rhs) {
    lhs += Draw::fromOSUnit(rhs);
    return lhs;
}

inline Draw::Unit& operator-=(Draw::Unit& lhs, const OS::Unit& rhs) {
    lhs -= Draw::fromOSUnit(rhs);
    return lhs;
}

} // namespace riscos

#endif
