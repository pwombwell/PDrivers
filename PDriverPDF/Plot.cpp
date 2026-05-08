#include "Core/PDriver.h"

#include "Colour.h"
#include "JobWS.h"
#include "Output.h"
#include "PDriverPDF.h"
#include "Private.h"

#include "Core/Job.h"
#include "Core/OS.h"
#include "Common/PlotDraw.h"

#include <stdint.h>

static MyError plotPrepareOutput(Output& output,
                                 JobWS& job)
{
#if PSCoordSpeedUps
    MyError err = ensure_OScoords(output, job);
    if (err != nullptr)
        return err;

    return colour_ensure(job);
#else
    (void)output;
    (void)job;
    return nullptr;
#endif
}

static MyError pdf_rect(const Rect<OS::Unit>& rect,
                        Output& output)
{
    return output.writeRect(rect);
}

MyError plot_draw_start(PlotDrawMode mode,
                        CoreJobWS& coreJob)
{
    (void)mode;
    JobWS& job = toJobWS(coreJob);
    Output& output(job.output());
    return plotPrepareOutput(output, job);
}

MyError plot_draw_move(const Draw::Point& point,
                       CoreJobWS& coreJob)
{
    Output& output(toJobWS(coreJob).output());
    return output.moveTo(point);
}

MyError plot_draw_line(const Draw::Point& point,
                       CoreJobWS& coreJob)
{
    Output& output(toJobWS(coreJob).output());
    return output.lineTo(point);
}

MyError plot_draw_bezier(const Draw::Point& control1,
                         const Draw::Point& control2,
                         const Draw::Point& endPoint,
                         CoreJobWS& coreJob)
{
    Output& output(toJobWS(coreJob).output());
    return output.bezierTo(control1, control2, endPoint);
}

MyError plot_draw_close(CoreJobWS& coreJob)
{
    Output& output(toJobWS(coreJob).output());
    return output.closePath();
}

MyError plot_draw_finish(PlotDrawMode mode,
                         CoreJobWS& coreJob)
{
    Output& output(toJobWS(coreJob).output());

    switch (mode) {
    case PlotDrawMode_Stroke:
        return output.str("S\n");
    case PlotDrawMode_FillStroke:
        return output.str("B\n");
    case PlotDrawMode_FillOnly:
        return output.str("f\n");
    }

    return nullptr;
}

static void plot_noend(Point<OS::Unit>& p0,
                       const Point<OS::Unit>& p1)
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

static void plot_nostart(Point<OS::Unit>& p1,
                         const Point<OS::Unit>& p0)
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

#if defined(RealDottedLines) && RealDottedLines

static MyError plotOutputDashPattern(const Draw::Unit* dashValues,
                                     uint32_t dashCount,
                                     Output& output)
{
    MyError err = output.str("[ ");
    if (err != nullptr)
        return err;

    for (uint32_t i = 0; i < dashCount; ++i) {
        if ((err = output.writeCoordSpace(dashValues[i])) != nullptr)
            return err;
    }

    return output.str("] 0 d\n");
}

// Uses helpers now in Common/PlotDraw.cpp.
MyError plot_dottedbothends(const Point<OS::Unit>& p0,
                            const Point<OS::Unit>& p1,
                            const Point<OS::Unit>& p2,
                            CoreJobWS& coreJob)
{
    (void)p2;
    JobWS& job = toJobWS(coreJob);
    Output& output(job.output());

    MyError err = plot_draw_start(PlotDrawMode_Stroke, coreJob);
    if (err != nullptr)
        return err;

    Draw::Unit dashValues[66];
    uint32_t dashCount = 0;
    if ((err = plotGenerateDotDash(dashValues, dashCount)) != nullptr)
        return err;

    if ((err = output.str("q\n")) != nullptr)
        return err;
    if ((err = plotOutputDashPattern(dashValues, dashCount, output)) != nullptr)
        return err;
    if ((err = plot_dotted_path(p0, p1, coreJob)) != nullptr)
        return err;
    if ((err = plot_draw_finish(PlotDrawMode_Stroke, coreJob)) != nullptr)
        return err;

    return output.str("Q\n");
}
#endif

MyError plot_fillclipbox(CoreJobWS& coreJob)
{
    const Rect<OS::Unit>& graphicsWindow = coreJob.graphicswindow;
    if (graphicsWindow.isEmpty())
        return nullptr;

    JobWS& job = toJobWS(coreJob);
    Output& output(job.output());

    MyError err = plotPrepareOutput(output, job);
    if (err != nullptr)
        return err;
    if ((err = pdf_rect(graphicsWindow, output)) != nullptr)
        return err;

    return output.str("f\n");
}
