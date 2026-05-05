#include "Core/PDriver.h"

#include "PlotDraw.h"

#include <stdint.h>

static Draw::Unit plotTimesK(Draw::Unit value)
{
    // 4*(sqrt(2)-1)/3 = &8D6289/2^24
#if 1
    return Draw::Unit::fromRaw((int64_t(value.raw()) * 0x8D6289) >> 24);
#else
    int32_t tmp = value.raw() * 0x8d;
    int32_t accum = tmp >> 8;
    tmp = value.raw() * 0x62;
    accum += tmp >> 16;
    tmp = value.raw() * 0x89;
    accum += tmp >> 24;

    return Draw::Unit::fromRaw(accum);
#endif
}

static Draw::Unit plotDivide(const Base::Fixed64<16>& numerator,
                             const Draw::Unit& denominator)
{
    return Draw::Unit::fromRaw((int32_t)(numerator.raw() / denominator.raw()));
}

static Base::Fixed<16> divideAndScale(const OS::Unit64& numerator,
                                      const OS::Unit64& divisor)
{
    int64_t d = divisor.value < 0 ? -divisor.value : divisor.value;
    if (d == 0)
        return Base::Fixed<16>::zero();

    int32_t sign = (numerator.value < 0) ? -1 : 1;
    int64_t scaled = ((numerator.value < 0) ? -numerator.value : numerator.value) << 16;
    int64_t q = (scaled + (d >> 1)) / d;
    if (sign < 0)
        q = -q;

    return Base::Fixed<16>::fromRaw((int32_t)q);
}

static Draw::Unit scaleAndSqrt(const OS::Unit64& value)
{
    return Draw::Unit::fromRaw((value << 16).sqrt().raw());
}

enum Quadrant {
    Q0, Q1, Q2, Q3
};

static Quadrant quadrant(const Draw::Offset& point)
{
    if (point.dx < Draw::Unit::zero())
        return (point.dy > Draw::Unit::zero()) ? Q1 : Q2;
    if (point.dy < Draw::Unit::zero())
        return Q3;
    if (point.dx == Draw::Unit::zero())
        return Q1;

    return Q0;
}

static Draw::Offset boundaryForQuadrant(Quadrant quadrant,
                                       const Draw::Unit& radius)
{
    const Draw::Unit zero = Draw::Unit::zero();

    switch (quadrant) {
    case Q0:
        return Draw::Offset(zero, radius);
    case Q1:
        return Draw::Offset(-radius, zero);
    case Q2:
        return Draw::Offset(zero, -radius);
    case Q3:
        return Draw::Offset(radius, zero);
    }

    return Draw::Offset();
}

// Given two points A and B on the same circle, determine whether the
// anticlockwise arc from A to B crosses the next quadrant boundary
// before reaching B.
//
// returns true if the arc from A to B reaches the next quadrant boundary
// before it reaches B.
// true: you must first go to the axis boundary for A's quadrant. `07`
// false: you can go straight from A to B. `06`
static bool needsBoundaryBefore(const Draw::Offset& a,
                                const Draw::Offset& b)
{
    const Draw::Unit zero = Draw::Unit::zero();

    // `04`
    if (b.dx >= zero) {
        // b is in Q0 or Q3, or on the y-axis
        if (b.dx == zero && a.dx == zero)
            return false;
        if (a.dy > b.dy)
            return true;         // need Ay < By
    } else {
        // `41`
        // b is in Q1 or Q2.
        if (a.dy < b.dy)
            return true;         // need Ay > By
        if (a.dy == zero)
            return false;
    }

    // `42`
    if (b.dy >= zero)
        return a.dx < b.dx;        // need Ax > Bx

    // `43`
    return a.dx > b.dx;            // need Ax < Bx
}

// `03`. Only entered if a.x == 0 || a.y == 0.
static void quarterControls(const Draw::Offset& a,
                            const Draw::Unit& radius,
                            const Draw::Unit& k,
                            Draw::Offset& control1,
                            Draw::Offset& control2)
{
    const Draw::Unit zero = Draw::Unit::zero();

    if (a.dx != zero) {
        // Ay == 0: control points determined by sign of Ax
        if (a.dx > zero) {
            control1 = Draw::Offset(radius, k);
            control2 = Draw::Offset(k, radius);
        } else {
            control1 = Draw::Offset(-radius, -k);
            control2 = Draw::Offset(-k, -radius);
        }
        return;
    }

    // Ax == 0: control points determined by sign of Ay
    if (a.dy > zero) {
        control1 = Draw::Offset(-k, radius);
        control2 = Draw::Offset(-radius, k);
    } else {
        control1 = Draw::Offset(k, -radius);
        control2 = Draw::Offset(radius, -k);
    }
}

static MyError plotPathLine(const Point<OS::Unit>& p0,
                            const Point<OS::Unit>& p1,
                            CoreJobWS& coreJob)
{
    MyError err = plot_draw_move(Draw::fromOSUnit(p1), coreJob);
    if (err != nullptr)
        return err;

    return plot_draw_line(Draw::fromOSUnit(p0), coreJob);
}

static MyError plotPathTriangle(const Point<OS::Unit>& p0,
                                const Point<OS::Unit>& p1,
                                const Point<OS::Unit>& p2,
                                CoreJobWS& coreJob)
{
    MyError err = plot_draw_move(Draw::fromOSUnit(p2), coreJob);
    if (err != nullptr)
        return err;
    if ((err = plot_draw_line(Draw::fromOSUnit(p1), coreJob)) != nullptr)
        return err;
    if ((err = plot_draw_line(Draw::fromOSUnit(p0), coreJob)) != nullptr)
        return err;

    return plot_draw_close(coreJob);
}

static MyError plotPathRectangle(const Point<OS::Unit>& p0,
                                 const Point<OS::Unit>& p1,
                                 CoreJobWS& coreJob)
{
    MyError err = plot_draw_move(Draw::fromOSUnit(p1), coreJob);
    if (err != nullptr)
        return err;
    if ((err = plot_draw_line(Draw::fromOSUnit(Point<OS::Unit>(p0.x, p1.y)), coreJob)) != nullptr)
        return err;
    if ((err = plot_draw_line(Draw::fromOSUnit(p0), coreJob)) != nullptr)
        return err;
    if ((err = plot_draw_line(Draw::fromOSUnit(Point<OS::Unit>(p1.x, p0.y)), coreJob)) != nullptr)
        return err;

    return plot_draw_close(coreJob);
}

static MyError plotPathParallelogram(const Point<OS::Unit>& p0,
                                     const Point<OS::Unit>& p1,
                                     const Point<OS::Unit>& p2,
                                     CoreJobWS& coreJob)
{
    const Point<OS::Unit> p3(p2.x + p0.x - p1.x,
                             p2.y + p0.y - p1.y);

    MyError err = plot_draw_move(Draw::fromOSUnit(p2), coreJob);
    if (err != nullptr)
        return err;
    if ((err = plot_draw_line(Draw::fromOSUnit(p1), coreJob)) != nullptr)
        return err;
    if ((err = plot_draw_line(Draw::fromOSUnit(p0), coreJob)) != nullptr)
        return err;
    if ((err = plot_draw_line(Draw::fromOSUnit(p3), coreJob)) != nullptr)
        return err;

    return plot_draw_close(coreJob);
}

static MyError plotPathCircle(const Point<OS::Unit>& edge,
                              const Point<OS::Unit>& centre,
                              CoreJobWS& coreJob)
{
    const Offset<OS::Unit> diff(centre - edge);

    const OS::Unit64 radiusSq(diff.dx.mul64(diff.dx) + diff.dy.mul64(diff.dy));
    const Draw::Unit radius = scaleAndSqrt(radiusSq);
    const Draw::Unit k = plotTimesK(radius);

    const Draw::Point centrePoint(Draw::fromOSUnit(centre));
    if (radius == Draw::Unit::zero())
        return plot_draw_move(centrePoint, coreJob);

    const Draw::Unit xRight = centrePoint.x + radius;
    const Draw::Unit xLeft = centrePoint.x - radius;
    const Draw::Unit yTop = centrePoint.y + radius;
    const Draw::Unit yBottom = centrePoint.y - radius;

    MyError err;
    if ((err = plot_draw_move(Draw::Point(centrePoint.x, yBottom), coreJob)) != nullptr)
        return err;
    if ((err = plot_draw_bezier(Draw::Point(centrePoint.x + k, yBottom),
                                Draw::Point(xRight, centrePoint.y - k),
                                Draw::Point(xRight, centrePoint.y),
                                coreJob)) != nullptr)
        return err;
    if ((err = plot_draw_bezier(Draw::Point(xRight, centrePoint.y + k),
                                Draw::Point(centrePoint.x + k, yTop),
                                Draw::Point(centrePoint.x, yTop),
                                coreJob)) != nullptr)
        return err;
    if ((err = plot_draw_bezier(Draw::Point(centrePoint.x - k, yTop),
                                Draw::Point(xLeft, centrePoint.y + k),
                                Draw::Point(xLeft, centrePoint.y),
                                coreJob)) != nullptr)
        return err;
    return plot_draw_bezier(Draw::Point(xLeft, centrePoint.y - k),
                            Draw::Point(centrePoint.x - k, yBottom),
                            Draw::Point(centrePoint.x, yBottom),
                            coreJob);
}

// `plot_GenerateArcPath`
static MyError plotPathArc(const Point<OS::Unit>& end,
                           const Point<OS::Unit>& start,
                           const Point<OS::Unit>& centre,
                           CoreJobWS& coreJob)
{
    MyError err;

    const Draw::Unit zero = Draw::Unit::zero();

    const Offset<OS::Unit> endRel(end - centre);
    const Offset<OS::Unit> startRel(start - centre);

    const OS::Unit64 angleSq(endRel.dx.mul64(endRel.dx) +
                             endRel.dy.mul64(endRel.dy));
    const Draw::Unit angleLen = scaleAndSqrt(angleSq);

    const OS::Unit64 radiusSq(startRel.dx.mul64(startRel.dx) +
                              startRel.dy.mul64(startRel.dy));
    const Draw::Unit radius = scaleAndSqrt(radiusSq);

    if (radius == zero)
        return plot_draw_move(Draw::fromOSUnit(start), coreJob);

    const Draw::Unit kQuarter = plotTimesK(radius);

    const Draw::Point centrePoint(Draw::fromOSUnit(centre));
    Draw::Offset pointA(Draw::fromOSUnit(startRel));
    Draw::Offset pointB(Draw::fromOSUnit(endRel));

    if (angleLen == zero) {
        pointB = pointA;
    } else {
        pointB = Draw::Offset(plotDivide(pointB.dx.mul64(radius), angleLen),
                                    plotDivide(pointB.dy.mul64(radius), angleLen));

        if (pointB.dy == zero)
            pointB.dx = radius;
        if (pointB.dx == zero)
            pointB.dy = radius;
    }

    if ((err = plot_draw_move(centrePoint + pointA, coreJob)) != nullptr)
        return err;

    for (;;) {
        Draw::Offset segment(pointB);

        const Quadrant quadA = quadrant(pointA);
        const Quadrant quadB = quadrant(pointB);

        if (quadA != quadB || needsBoundaryBefore(pointA, segment))
            segment = boundaryForQuadrant(quadA, radius);

        if (pointA.dx == zero || pointA.dy == zero) {
            Draw::Offset control1;
            Draw::Offset control2;
            quarterControls(pointA, radius, kQuarter, control1, control2);

            err = plot_draw_bezier(centrePoint + control1,
                                   centrePoint + control2,
                                   centrePoint + segment,
                                   coreJob);
        } else {
            const Point<OS::Unit> a(pointA.dx.toIntTruncated(),
                                    pointA.dy.toIntTruncated());
            const Point<OS::Unit> b(segment.dx.toIntTruncated(),
                                    segment.dy.toIntTruncated());

            const OS::Unit64 r9(radiusSq + a.x.mul64(b.x) + a.y.mul64(b.y));
            const OS::Unit64 doubled(r9.mul(radiusSq) * 2);
            const OS::Unit64 numerator((doubled.sqrt() - r9) * 4);
            OS::Unit64 divisor((a.x.mul64(b.y) - a.y.mul64(b.x)) * 3);

            if (divisor == OS::Unit(0))
                break;
            if (divisor < OS::Unit(0))
                divisor = -divisor;

            Base::Fixed<16> k = divideAndScale(numerator, divisor);
            if (k.raw() < 0)
                k = -k;

            const Draw::Offset control1(pointA.dx - pointA.dy.mul(k),
                                              pointA.dy + pointA.dx.mul(k));
            const Draw::Offset control2(segment.dx + segment.dy.mul(k),
                                              segment.dy - segment.dx.mul(k));

            err = plot_draw_bezier(centrePoint + control1,
                                   centrePoint + control2,
                                   centrePoint + segment,
                                   coreJob);
        }

        if (err != nullptr)
            return err;

        pointA = segment;
        if (pointA == pointB)
            break;
    }

    return nullptr;
}

static MyError plotPathEllipse(const Point<OS::Unit>& p0,
                               const Point<OS::Unit>& p1,
                               const Point<OS::Unit>& c,
                               CoreJobWS& coreJob)
{
    const Draw::Unit zero(Draw::Unit::zero());
    const Draw::Point centre(Draw::fromOSUnit(c));

    const Draw::Unit xRadius = Draw::fromOSUnit(p1.x) - centre.x; // p1.y unused

    const Draw::Offset yAxis(Draw::fromOSUnit(p0) - centre);

    const Draw::Unit yAxisXK = plotTimesK(yAxis.dx);
    const Draw::Unit yAxisYK = plotTimesK(yAxis.dy);
    const Draw::Unit xAxisK = plotTimesK(xRadius);

    const Draw::Offset rightControl(xRadius + yAxisXK, yAxisYK);
    const Draw::Offset topControl1(yAxis.dx + xAxisK, yAxis.dy);
    const Draw::Offset topControl2(-yAxis.dx + xAxisK, -yAxis.dy);
    const Draw::Offset leftControl(xRadius - yAxisXK, -yAxisYK);

    MyError err;
    // Move to right, curve to top, to left, to bottom, to right.
    if ((err = plot_draw_move(centre + Draw::Offset(xRadius, zero), coreJob)) != nullptr)
        return err;
    if ((err = plot_draw_bezier(centre + rightControl,
                                centre + topControl1,
                                centre + yAxis,
                                coreJob)) != nullptr)
        return err;
    if ((err = plot_draw_bezier(centre - topControl2,
                                centre - leftControl,
                                centre - Draw::Offset(xRadius, zero),
                                coreJob)) != nullptr)
        return err;
    if ((err = plot_draw_bezier(centre - rightControl,
                                centre - topControl1,
                                centre - yAxis,
                                coreJob)) != nullptr)
        return err;

    return plot_draw_bezier(centre + topControl2,
                            centre + leftControl,
                            centre + Draw::Offset(xRadius, zero),
                            coreJob);
}

static void plot_noend(Point<OS::Unit>& p0, const Point<OS::Unit>& p1)
{
    if (p0.x < p1.x)
        p0.x += OS::Unit(2);
    else if (p0.x > p1.x)
        p0.x -= OS::Unit(2);

    if (p0.y < p1.y)
        p0.y += OS::Unit(2);
    else if (p0.y > p1.y)
        p0.y -= OS::Unit(2);
}

static void plot_nostart(Point<OS::Unit>& p1, const Point<OS::Unit>& p0)
{
    if (p1.x < p0.x)
        p1.x += OS::Unit(2);
    else if (p1.x > p0.x)
        p1.x -= OS::Unit(2);

    if (p1.y < p0.y)
        p1.y += OS::Unit(2);
    else if (p1.y > p0.y)
        p1.y -= OS::Unit(2);
}

static MyError plotStartAndDraw(PlotDrawMode mode,
                                MyError (*pathFn)(const Point<OS::Unit>&,
                                                  const Point<OS::Unit>&,
                                                  CoreJobWS&),
                                const Point<OS::Unit>& p0,
                                const Point<OS::Unit>& p1,
                                CoreJobWS& coreJob)
{
    MyError err = plot_draw_start(mode, coreJob);
    if (err != nullptr)
        return err;
    if ((err = pathFn(p0, p1, coreJob)) != nullptr)
        return err;

    return plot_draw_finish(mode, coreJob);
}

MyError plot_linebothends(const Point<OS::Unit>& p0,
                          const Point<OS::Unit>& p1,
                          const Point<OS::Unit>& p2,
                          CoreJobWS& coreJob)
{
    (void)p2;
    return plotStartAndDraw(PlotDrawMode_Stroke, plotPathLine, p0, p1, coreJob);
}

MyError plot_linestartonly(const Point<OS::Unit>& p0,
                           const Point<OS::Unit>& p1,
                           const Point<OS::Unit>& p2,
                           CoreJobWS& coreJob)
{
    (void)p2;
    Point<OS::Unit> q0(p0);
    plot_noend(q0, p1);
    return plotStartAndDraw(PlotDrawMode_Stroke, plotPathLine, q0, p1, coreJob);
}

MyError plot_lineendonly(const Point<OS::Unit>& p0,
                         const Point<OS::Unit>& p1,
                         const Point<OS::Unit>& p2,
                         CoreJobWS& coreJob)
{
    (void)p2;
    Point<OS::Unit> q1(p1);
    plot_nostart(q1, p0);
    return plotStartAndDraw(PlotDrawMode_Stroke, plotPathLine, p0, q1, coreJob);
}

MyError plot_linenoends(const Point<OS::Unit>& p0,
                        const Point<OS::Unit>& p1,
                        const Point<OS::Unit>& p2,
                        CoreJobWS& coreJob)
{
    (void)p2;
    Point<OS::Unit> q0(p0);
    Point<OS::Unit> q1(p1);
    plot_nostart(q1, q0);
    plot_noend(q0, q1);
    return plotStartAndDraw(PlotDrawMode_Stroke, plotPathLine, q0, q1, coreJob);
}

MyError plot_line(const Point<OS::Unit>& p0,
                  const Point<OS::Unit>& p1,
                  const Point<OS::Unit>& p2,
                  CoreJobWS& coreJob)
{
    return plot_linebothends(p0, p1, p2, coreJob);
}

#if defined(RealDottedLines) && RealDottedLines
static MyError plot_dotted_path(const Point<OS::Unit>& p0,
                                const Point<OS::Unit>& p1,
                                CoreJobWS& coreJob)
{
    MyError err = plot_draw_move(Draw::fromOSUnit(p1), coreJob);
    if (err != nullptr)
        return err;

    return plot_draw_line(Draw::fromOSUnit(p0), coreJob);
}

static MyError generateDotDash(JobWS& job)
{
    // read dot-dash length
    uint32_t r0 = 163;
    uint32_t r1 = 242;
    uint32_t r2 = 65;
    MyError err = XOS_Byte(&r0, &r1, &r2);
    if (err != nullptr)
        return err;

    uint32_t length = r2 & 0x3f;  // 6 bit length only
    if (length == 0)
        length = 64u;

    Draw::DashPattern* pattern = job.plot().dashPattern();
    Draw::Unit* elements = job.plot().dashPatternElements();
    job.plot().setDashStyle(pattern);

    // read character defn 6 (dot dash patttern)
    // Rather than use a 9-byte buffer, use a 3-word buffer and ignore the
    // first three bytes. This aligns the reads, and avoids over-reading,
    // which the original ARM code does (harmlessly, as it reuses a buffer).
    union {
        uint32_t w[3];
        uint8_t  b[3 * 4];
    } buffer;
    buffer.b[3] = 6;
    if ((err = XOS_Word(10, &buffer.b[3])) != nullptr)
        return err;

    uint64_t bits = ((uint64_t)buffer.w[2] << 32) | buffer.w[1];

    uint32_t count = 0;
    Draw::Unit accum = Draw::Unit(0);
    int state = 1; // dot

    for (uint32_t i = 0; i < length; ++i) {
        const int bit = (bits >> 63) & 1;
        bits <<= 1;
        if (bit == state) {
            accum += Draw::Unit(2);
        } else {
            elements[count++] = accum;
            accum = Draw::Unit(2);
            state ^= 1;
        }
    }

    elements[count++] = accum;

    pattern->m_startOffset = Draw::Unit(0); // always start at the beginning(!)
    if ((count & 1) != 0)       // even pattern length?
        elements[count++] = Draw::Unit(0);  // force realignment

    pattern->m_elementCount = count;

    return nullptr;
}

// PDriverPS has an alternative implementation of this that creates
// a Postscript array. Possibly don't include this function, or just
// use a common Draw-path implementation.
MyError plot_dottedbothends(const Point<OS::Unit>& p0,
                            const Point<OS::Unit>& p1,
                            const Point<OS::Unit>& p2,
                            CoreJobWS& coreJob)
{
    (void)p2;
    JobWS& job = toJobWS(coreJob);

    MyError err;

    if ((err = generateDotDash(job)) != nullptr)
        return err;
    if ((err = plot_draw_start(PlotDrawMode_Stroke, coreJob)) != nullptr)
        return err;
    if ((err = plot_dotted_path(p0, p1, coreJob)) != nullptr)
        return err;

    err = plot_draw_finish(PlotDrawMode_Stroke, coreJob);
    job.plot().clearDashStyle();
    return err;
}

MyError plot_dottedstartonly(const Point<OS::Unit>& p0,
                             const Point<OS::Unit>& p1,
                             const Point<OS::Unit>& p2,
                             CoreJobWS& coreJob)
{
    Point<OS::Unit> q0(p0);
    plot_noend(q0, p1);
    return plot_dottedbothends(q0, p1, p2, coreJob);
}

MyError plot_dottedendonly(const Point<OS::Unit>& p0,
                           const Point<OS::Unit>& p1,
                           const Point<OS::Unit>& p2,
                           CoreJobWS& coreJob)
{
    Point<OS::Unit> q1(p1);
    plot_nostart(q1, p0);
    return plot_dottedbothends(p0, q1, p2, coreJob);
}

MyError plot_dottednoends(const Point<OS::Unit>& p0,
                          const Point<OS::Unit>& p1,
                          const Point<OS::Unit>& p2,
                          CoreJobWS& coreJob)
{
    Point<OS::Unit> q0(p0);
    Point<OS::Unit> q1(p1);
    plot_nostart(q1, q0);
    plot_noend(q0, q1);
    return plot_dottedbothends(q0, q1, p2, coreJob);
}

MyError plot_dotted(const Point<OS::Unit>& p0,
                    const Point<OS::Unit>& p1,
                    const Point<OS::Unit>& p2,
                    CoreJobWS& coreJob)
{
    return plot_dottedbothends(p0, p1, p2, coreJob);
}
#endif

MyError plot_point(const Point<OS::Unit>& p0,
                   const Point<OS::Unit>& p1,
                   const Point<OS::Unit>& p2,
                   CoreJobWS& coreJob)
{
    (void)p1;
    Point<OS::Unit> bottomLeft(p0);
    bottomLeft -= Offset<OS::Unit>(OS::Unit(1), OS::Unit(1));
    Rect<OS::Unit> pixel(bottomLeft,
                         Size<OS::Unit>(OS::Unit(2), OS::Unit(2)));
    return plot_rectangle(Point<OS::Unit>(pixel.x1, pixel.y1),
                          Point<OS::Unit>(pixel.x0, pixel.y0),
                          p2,
                          coreJob);
}

MyError plot_triangle(const Point<OS::Unit>& p0,
                      const Point<OS::Unit>& p1,
                      const Point<OS::Unit>& p2,
                      CoreJobWS& coreJob)
{
    MyError err = plot_draw_start(PlotDrawMode_FillStroke, coreJob);
    if (err != nullptr)
        return err;
    if ((err = plotPathTriangle(p0, p1, p2, coreJob)) != nullptr)
        return err;

    return plot_draw_finish(PlotDrawMode_FillStroke, coreJob);
}

MyError plot_rectangle(const Point<OS::Unit>& p0,
                       const Point<OS::Unit>& p1,
                       const Point<OS::Unit>& p2,
                       CoreJobWS& coreJob)
{
    (void)p2;
    MyError err = plot_draw_start(PlotDrawMode_FillStroke, coreJob);
    if (err != nullptr)
        return err;
    if ((err = plotPathRectangle(p0, p1, coreJob)) != nullptr)
        return err;

    return plot_draw_finish(PlotDrawMode_FillStroke, coreJob);
}

MyError plot_parallelogram(const Point<OS::Unit>& p0,
                           const Point<OS::Unit>& p1,
                           const Point<OS::Unit>& p2,
                           CoreJobWS& coreJob)
{
    MyError err = plot_draw_start(PlotDrawMode_FillStroke, coreJob);
    if (err != nullptr)
        return err;
    if ((err = plotPathParallelogram(p0, p1, p2, coreJob)) != nullptr)
        return err;

    return plot_draw_finish(PlotDrawMode_FillStroke, coreJob);
}

MyError plot_strokecircle(const Point<OS::Unit>& p0,
                          const Point<OS::Unit>& p1,
                          const Point<OS::Unit>& p2,
                          CoreJobWS& coreJob)
{
    (void)p2;
    MyError err = plot_draw_start(PlotDrawMode_Stroke, coreJob);
    if (err != nullptr)
        return err;
    if ((err = plotPathCircle(p0, p1, coreJob)) != nullptr)
        return err;
    if ((err = plot_draw_close(coreJob)) != nullptr)
        return err;

    return plot_draw_finish(PlotDrawMode_Stroke, coreJob);
}

MyError plot_fillcircle(const Point<OS::Unit>& p0,
                        const Point<OS::Unit>& p1,
                        const Point<OS::Unit>& p2,
                        CoreJobWS& coreJob)
{
    (void)p2;
    MyError err = plot_draw_start(PlotDrawMode_FillStroke, coreJob);
    if (err != nullptr)
        return err;
    if ((err = plotPathCircle(p0, p1, coreJob)) != nullptr)
        return err;
    if ((err = plot_draw_close(coreJob)) != nullptr)
        return err;

    return plot_draw_finish(PlotDrawMode_FillStroke, coreJob);
}

MyError plot_strokearc(const Point<OS::Unit>& p0,
                       const Point<OS::Unit>& p1,
                       const Point<OS::Unit>& p2,
                       CoreJobWS& coreJob)
{
    MyError err = plot_draw_start(PlotDrawMode_Stroke, coreJob);
    if (err != nullptr)
        return err;
    if ((err = plotPathArc(p0, p1, p2, coreJob)) != nullptr)
        return err;

    return plot_draw_finish(PlotDrawMode_Stroke, coreJob);
}

MyError plot_fillchord(const Point<OS::Unit>& p0,
                       const Point<OS::Unit>& p1,
                       const Point<OS::Unit>& p2,
                       CoreJobWS& coreJob)
{
    MyError err = plot_draw_start(PlotDrawMode_FillStroke, coreJob);
    if (err != nullptr)
        return err;
    if ((err = plotPathArc(p0, p1, p2, coreJob)) != nullptr)
        return err;
    if ((err = plot_draw_close(coreJob)) != nullptr)
        return err;

    return plot_draw_finish(PlotDrawMode_FillStroke, coreJob);
}

MyError plot_fillsector(const Point<OS::Unit>& p0,
                        const Point<OS::Unit>& p1,
                        const Point<OS::Unit>& p2,
                        CoreJobWS& coreJob)
{
    MyError err = plot_draw_start(PlotDrawMode_FillStroke, coreJob);
    if (err != nullptr)
        return err;
    if ((err = plotPathArc(p0, p1, p2, coreJob)) != nullptr)
        return err;
    if ((err = plot_draw_line(Draw::fromOSUnit(p2), coreJob)) != nullptr)
        return err;
    if ((err = plot_draw_close(coreJob)) != nullptr)
        return err;

    return plot_draw_finish(PlotDrawMode_FillStroke, coreJob);
}

MyError plot_strokeellipse(const Point<OS::Unit>& p0,
                           const Point<OS::Unit>& p1,
                           const Point<OS::Unit>& p2,
                           CoreJobWS& coreJob)
{
    MyError err = plot_draw_start(PlotDrawMode_Stroke, coreJob);
    if (err != nullptr)
        return err;
    if ((err = plotPathEllipse(p0, p1, p2, coreJob)) != nullptr)
        return err;
    if ((err = plot_draw_close(coreJob)) != nullptr)
        return err;

    return plot_draw_finish(PlotDrawMode_Stroke, coreJob);
}

MyError plot_fillellipse(const Point<OS::Unit>& p0,
                         const Point<OS::Unit>& p1,
                         const Point<OS::Unit>& p2,
                         CoreJobWS& coreJob)
{
    MyError err = plot_draw_start(PlotDrawMode_FillStroke, coreJob);
    if (err != nullptr)
        return err;
    if ((err = plotPathEllipse(p0, p1, p2, coreJob)) != nullptr)
        return err;
    if ((err = plot_draw_close(coreJob)) != nullptr)
        return err;

    return plot_draw_finish(PlotDrawMode_FillStroke, coreJob);
}
