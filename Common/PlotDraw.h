#ifndef COMMON_PLOTDRAW_H
#define COMMON_PLOTDRAW_H

// Most PDrivers convert VDU Plot calls to vector paths. This abstracts the maths to do that.
// Drivers that include PlotDraw.cpp must implement the new plot_draw_*() functions (as well
// as the remaining plot_*() calls).

// All the dotted variants are untested. They are based on the original unused
// PDriverDP implementation, which is conditionally assembled off. I've
// implemented them here so that feature could be extended to all PDrivers.

enum PlotDrawMode {
    PlotDrawMode_Stroke,
    PlotDrawMode_FillStroke,
    PlotDrawMode_FillOnly
};

#include "Core/Device.h"

MyError plot_draw_start(PlotDrawMode mode, CoreJobWS& coreJob);
MyError plot_draw_move(const Draw::Point& point, CoreJobWS& coreJob);
MyError plot_draw_line(const Draw::Point& point, CoreJobWS& coreJob);
MyError plot_draw_bezier(const Draw::Point& control1,
                         const Draw::Point& control2,
                         const Draw::Point& endPoint,
                         CoreJobWS& coreJob);
MyError plot_draw_close(CoreJobWS& coreJob);
MyError plot_draw_finish(PlotDrawMode mode, CoreJobWS& coreJob);

// Implements (in Device.h order):
//  plot_linebothends
//  plot_linestartonly
//  plot_lineendonly
//  plot_linenoends
//  plot_line
//  #if defined(RealDottedLines) && RealDottedLines
//      plot_dottedbothends
//      plot_dottedstartonly
//      plot_dottedendonly
//      plot_dottednoends
//      plot_dotted
//  #endif
//  plot_point
//  plot_triangle
//  plot_rectangle
//  plot_parallelogram
//  plot_strokecircle
//  plot_fillcircle
//  plot_strokearc
//  plot_fillchord
//  plot_fillsector
//  plot_strokeellipse
//  plot_fillellipse

#endif
