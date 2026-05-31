#include "Core/PDriver.h"

#include "JobWS.h"
#include "Private.h"

#include "Core/Constants.h"
#include "Core/Job.h"
#include "Common/PlotDraw.h"

#include "RLibX/Draw.h"

#include <stdint.h>

static const uint32_t joinstyles[2] = {0u, 0x10000u};

static const uint8_t plot_clgstring[] = {
    12, 25, 4, 0, 0, 0, 0, 25, (5 + 96), 255, 127, 255, 127
};

static inline Draw::Path plot_path(JobWS& job)
{
    return job.plotWS().path();
}

static inline Draw::PathComponent*& plot_path_cursor(JobWS& job)
{
    return job.plotWS().pathCursor();
}

// `buildDrawMatrix`
static Draw::Matrix buildDrawMatrix(JobWS& job)
{
    Draw::Matrix matrix = Draw::Matrix::identity();
    matrix.translate(Draw::fromOSUnit(job.usersoffset));
    return matrix;
}

static MyError plot_strokeit(Draw::Path path,
                             JobWS& job)
{
    uint32_t flags = 0;
#if PrinterDrawBit
    flags |= (1u << 16);
#endif

    Draw::Matrix matrix = buildDrawMatrix(job);
    const Draw::DashPattern* dash = job.plotWS().dashStyle();
    const Draw::CapJoin* capJoin = (const Draw::CapJoin*)(const void*)joinstyles;
    return Draw::stroke(path, flags, &matrix,
                        Draw::Unit::zero(), Draw::Unit::fromRaw(512),
                        capJoin, dash);
}

static MyError plot_fillonly(Draw::Path path,
                             JobWS& job)
{
    uint32_t flags = 0;
#if PrinterDrawBit
    flags |= (1u << 16);
#endif

    const Draw::Matrix matrix = buildDrawMatrix(job);
    return Draw::fill(path, flags, &matrix, Draw::Unit::zero());
}

static MyError plot_fillit(Draw::Path path,
                           JobWS& job)
{
    MyError err = plot_fillonly(path, job);
    if (err != nullptr)
        return err;

    return plot_strokeit(path, job);
}

MyError plot_draw_start(PlotDrawMode mode,
                        CoreJobWS& coreJob)
{
    (void)mode;
    JobWS& job = toJobWS(coreJob);
    job.plotWS().resetPathCursor();
    return nullptr;
}

MyError plot_draw_move(const Draw::Point& point,
                       CoreJobWS& coreJob)
{
    JobWS& job = toJobWS(coreJob);
    Draw::PathComponent*& path = plot_path_cursor(job);

    *path++ = Draw::DrawOp_Move;
    *path++ = point.x;
    *path++ = point.y;
    return nullptr;
}

MyError plot_draw_line(const Draw::Point& point,
                       CoreJobWS& coreJob)
{
    JobWS& job = toJobWS(coreJob);
    Draw::PathComponent*& path = plot_path_cursor(job);

    *path++ = Draw::DrawOp_Line;
    *path++ = point.x;
    *path++ = point.y;
    return nullptr;
}

MyError plot_draw_bezier(const Draw::Point& control1,
                         const Draw::Point& control2,
                         const Draw::Point& endPoint,
                         CoreJobWS& coreJob)
{
    JobWS& job = toJobWS(coreJob);
    Draw::PathComponent*& path = plot_path_cursor(job);

    *path++ = Draw::DrawOp_Bezier;
    *path++ = control1.x;
    *path++ = control1.y;
    *path++ = control2.x;
    *path++ = control2.y;
    *path++ = endPoint.x;
    *path++ = endPoint.y;
    return nullptr;
}

MyError plot_draw_close(CoreJobWS& coreJob)
{
    JobWS& job = toJobWS(coreJob);
    Draw::PathComponent*& path = plot_path_cursor(job);

    *path++ = Draw::DrawOp_CloseWithLine;
    return nullptr;
}

MyError plot_draw_finish(PlotDrawMode mode,
                         CoreJobWS& coreJob)
{
    JobWS& job = toJobWS(coreJob);
    Draw::PathComponent*& path = plot_path_cursor(job);

    *path = Draw::DrawOp_End;

    switch (mode) {
    case PlotDrawMode_Stroke:
        return plot_strokeit(plot_path(job), job);
    case PlotDrawMode_FillStroke:
        return plot_fillit(plot_path(job), job);
    case PlotDrawMode_FillOnly:
        return plot_fillonly(plot_path(job), job);
    }

    return nullptr;
}

MyError plot_fillclipbox(CoreJobWS& coreJob)
{
    if ((coreJob.disabled & Disabled_NullClip) != 0u)
        return nullptr;

    return vdu_counted_string(plot_clgstring);
}
