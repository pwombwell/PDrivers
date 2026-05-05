#include "Core/PDriver.h"

#include "Colour.h"
#include "JobWS.h"
#include "Output.h"
#include "PDriverPS.h"
#include "Private.h"

#include "Core/Job.h"
#include "Common/PlotDraw.h"

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

static MyError ps_fillAndStroke(Output& output,
                                JobWS& job)
{
    MyError err = output_gsave(job);
    if (err != nullptr)
        return err;
    if ((err = output.str("fill\n")) != nullptr)
        return err;
    if ((err = output_grestore(job)) != nullptr)
        return err;

    return output.str("stroke\n");
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
    JobWS& job = toJobWS(coreJob);
    Output& output(job.output());

    switch (mode) {
    case PlotDrawMode_Stroke:
        return output.str("stroke\n");
    case PlotDrawMode_FillStroke:
        return ps_fillAndStroke(output, job);
    case PlotDrawMode_FillOnly:
        return output.str("fill\n");
    }

    return nullptr;
}

MyError plot_fillclipbox(CoreJobWS& coreJob)
{
    JobWS& job = toJobWS(coreJob);
    Output& output(job.output());

#if PSCoordSpeedUps
    MyError err;
    if ((err = colour_ensure(job)) != nullptr)
        return err;
#endif

    return output.str("CP fill\n");
}
