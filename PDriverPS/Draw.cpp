#include "Core/PDriver.h"

#include "Colour.h"
#include "JobWS.h"
#include "Output.h"
#include "PDriverPS.h"
#include "Private.h"

#include "Core/Job.h"
#include "Core/MsgCode.h"
#include "Core/OS.h"
#include "Core/Workspace.h"

#include "RLib/Constants/Draw.h"
#include "RLib/OS/Error.h"
#include "RLibX/Draw.h"
#include "RLibX/Utils/ScopedPtr.h"

#include <stddef.h>

/* Draw call handling routines for the PostScript printer driver. */

static inline CoreWS& ws_from_ps(PDriverWS& psWS)
{
    return *(CoreWS *)&psWS;
}

static bool draw_has_subpath(Draw::DrawOp open)
{
    return open >= Draw::DrawOp_Move;
}

static bool draw_should_output_subpath(Draw::DrawOp open)
{
    return open == Draw::DrawOp_Move;
}

#if PSDebugEscapes
static MyError readescapestate(int *escaped) {
    MyError err = XOS_ReadEscapeState(escaped);
    if (!err) {
        volatile uint32_t *debug_ptr = (volatile uint32_t *)(uintptr_t)0x100000;
        *debug_ptr = (*escaped) ? 1u : 0u;
    }
    return err;
}
#endif

static MyError draw_check_escape(void) {
    int escaped = 0;
#if PSDebugEscapes
    MyError err = readescapestate(&escaped);
#else
    MyError err = XOS_ReadEscapeState(&escaped);
#endif
    if (err)
        return err;

    if (escaped)
        return ErrorBlock_Escape;

    return nullptr;
}

static MyError draw_outputcoords(Draw::Point p, Output& output)
{
    return output_coordpair(p, output);
}

static MyError draw_moveto(Draw::Point p, Output& output)
{
    MyError err = draw_outputcoords(p, output);
    if (err) {
        return err;
    }
    if ((err = output.str("M\n")) != nullptr)
        return err;
    return draw_check_escape();
}

static MyError draw_lineto(Draw::Point p, Output& output)
{
    MyError err = draw_outputcoords(p, output);
    if (err) {
        return err;
    }
    if ((err = output.str("L\n")) != nullptr)
        return err;
    return draw_check_escape();
}

static MyError draw_curveto(Draw::Point p, Output& output)
{
    MyError err = draw_outputcoords(p, output);
    if (err)
        return err;

    if ((err = output.str("B\n")) != nullptr)
        return err;

    return draw_check_escape();
}

static MyError draw_bezierto(Draw::Path& path, Output& output)
{
    MyError err = draw_outputcoords(path.readPoint(), output);
    if (err)
        return err;

    err = draw_outputcoords(path.readPoint(), output);
    if (err)
        return err;

    return draw_curveto(path.readPoint(), output);
}

static MyError draw_closepath(Output& output)
{
    MyError err = output.str("Cl\n");
    if (err) {
        return err;
    }
    return draw_check_escape();
}

static MyError draw_closeandfillpath(Output& output)
{
    MyError err = output.str("Cl fill\n");
    if (err) {
        return err;
    }
    return draw_check_escape();
}

/* Output the dash pattern pointed to by dash. */
static MyError draw_outputdashpattern(const Draw::DashPattern* dash,
                                      Output& output)
{
    if (dash == nullptr) {
        return nullptr;
    }

    MyError err = output.str("[ ");
    if (err) {
        return err;
    }

    uint32_t count = dash->elementCount();
    for (uint32_t i = 0; i < count; ++i) {
        if ((err = output.num(dash->element(i).raw())) != nullptr)
            return err;
    }

    if ((err = output.str("] ")) != nullptr)
        return err;

    if ((err = output.num(dash->startOffset().raw())) != nullptr)
        return err;

    return output.str("setdash\n");
}

/* Output the cap and join specification. */
static MyError draw_outputcapandjoin(const Draw::CapJoin* capJoin,
                                     Output& output)
{
    MyError err = output_coordpair((int32_t)capJoin->joinStyle(),
                                   (int32_t)capJoin->startCapStyle(),
                                   output);
    if (err) {
        return err;
    }
    if ((err = output.num(capJoin->miterLimit().raw())) != nullptr)
        return err;
    return output.str(" CJ\n");
}

/* Output the Draw transformation (matrix then origin-usersoffset) and flatness.
 * A "gsave" is emitted first.
 */
static MyError draw_outputtransformandflatness(const Draw::Matrix* matrix,
                                               Draw::Unit flatness,
                                               Output& output,
                                               JobWS& job,
                                               CoreWS& ws)
{
    MyError err;

#if PSCoordSpeedUps
    if ((err = ensure_OScoords(output, job)) != nullptr)
        return err;
    if ((err = colour_ensure(job)) != nullptr)
        return err;
#endif

    if ((err = output_gsave(job)) != nullptr)
        return err;

    Draw::Matrix defaultMatrix = Draw::Matrix::identity();
    const Draw::Matrix& drawMatrix = matrix ? *matrix : defaultMatrix;

    if ((err = output.writeCoordSpace(drawMatrix.a.raw())) != nullptr)
        return err;
    if ((err = output.writeCoordSpace(drawMatrix.b.raw())) != nullptr)
        return err;
    if ((err = output.writeCoordSpace(drawMatrix.c.raw())) != nullptr)
        return err;
    if ((err = output.writeCoordSpace(drawMatrix.d.raw())) != nullptr)
        return err;
    if ((err = output.writeCoordSpace(drawMatrix.e.raw())) != nullptr)
        return err;
    if ((err = output.writeCoordSpace(drawMatrix.f.raw())) != nullptr)
        return err;

    Offset<OS::Unit> t = job.origin - job.usersoffset;
    if ((err = output_coordpair(t, output)) != nullptr)
        return err;
    if ((err = output.str("T DM\n")) != nullptr)
        return err;

#if !PSFreeFlatness
    if (flatness == Draw::Unit::zero()) {
        flatness = Draw::Unit::fromRaw(0x200);
    }
    if ((err = output.write(flatness.raw())) != nullptr)
        return err;
    if ((err = output.str("DF\n")) != nullptr)
        return err;
#else
    (void)flatness;
#endif

    return nullptr;
}

/* Ignore special segments, close open subpaths, and treat gaps as lines. */
static MyError draw_outputpathforfill(Draw::Path path,
                                      PDriverWS& psWS,
                                      Output& output)
{
    Draw::Path p = path;
    Draw::DrawOp open = Draw::DrawOp_End;

    for (;;) {
        Draw::DrawOp op = p.readOp();

        if (op > Draw::DrawOp_Line)
            return psWS.messages.lookupError(ErrorBlock_BadPathElement);

        switch (op) {
        case Draw::DrawOp_End:
            if (draw_should_output_subpath(open)) {
                MyError err = draw_closepath(output);
                if (err) {
                    return err;
                }
            }
            return nullptr;
        case Draw::DrawOp_Continue:
            p.continueAt();
            break;
        case Draw::DrawOp_Move:
        case Draw::DrawOp_MoveInternal: {
            if (draw_should_output_subpath(open)) {
                MyError err = draw_closepath(output);
                if (err) {
                    return err;
                }
            }

            Draw::Point point = p.readPoint();

            open = op;
            if (draw_should_output_subpath(open)) {
                MyError err = draw_moveto(point, output);
                if (err) {
                    return err;
                }
            }
            break;
        }
        case Draw::DrawOp_CloseWithGap:
        case Draw::DrawOp_CloseWithLine: {
            if (draw_should_output_subpath(open)) {
                MyError err = draw_closepath(output);
                if (err) {
                    return err;
                }
            }
            open = Draw::DrawOp_End;
            break;
        }
        case Draw::DrawOp_Bezier: {
            if (!draw_has_subpath(open)) {
                return psWS.messages.lookupError(ErrorBlock_BadPathSequence);
            }
            if (open > Draw::DrawOp_Move) {
                p.skipBezier();
                break;
            }

            MyError err = draw_bezierto(p, output);
            if (err) {
                return err;
            }
            break;
        }
        case Draw::DrawOp_Gap:
        case Draw::DrawOp_Line: {
            if (!draw_has_subpath(open)) {
                return psWS.messages.lookupError(ErrorBlock_BadPathSequence);
            }
            Draw::Point point = p.readPoint();
            if (draw_should_output_subpath(open)) {
                MyError err = draw_lineto(point, output);
                if (err) {
                    return err;
                }
            }
            break;
        }
        default:
            return psWS.messages.lookupError(ErrorBlock_BadPathElement);
        }
    }
}

/* As draw_outputpathforfill, but fill each subpath as it completes. */
static MyError draw_outputbysubpaths(Draw::Path path,
                                     PDriverWS& psWS,
                                     Output& output)
{
    Draw::Path p = path;
    Draw::DrawOp open = Draw::DrawOp_End;

    for (;;) {
        Draw::DrawOp op = p.readOp();

        if (op > Draw::DrawOp_Line) {
            return psWS.messages.lookupError(ErrorBlock_BadPathElement);
        }

        switch (op) {
        case Draw::DrawOp_End:
            if (draw_should_output_subpath(open)) {
                MyError err = draw_closeandfillpath(output);
                if (err) {
                    return err;
                }
            }
            return nullptr;
        case Draw::DrawOp_Continue:
            p.continueAt();
            break;
        case Draw::DrawOp_Move:
        case Draw::DrawOp_MoveInternal: {
            if (draw_should_output_subpath(open)) {
                MyError err = draw_closeandfillpath(output);
                if (err) {
                    return err;
                }
            }
            Draw::Point point = p.readPoint();
            open = op;
            if (draw_should_output_subpath(open)) {
                MyError err = draw_moveto(point, output);
                if (err) {
                    return err;
                }
            }
            break;
        }
        case Draw::DrawOp_CloseWithGap:
        case Draw::DrawOp_CloseWithLine: {
            if (draw_should_output_subpath(open)) {
                MyError err = draw_closeandfillpath(output);
                if (err) {
                    return err;
                }
            }
            open = Draw::DrawOp_End;
            break;
        }
        case Draw::DrawOp_Bezier: {
            if (!draw_has_subpath(open)) {
                return psWS.messages.lookupError(ErrorBlock_BadPathSequence);
            }
            if (open > Draw::DrawOp_Move) {
                p.skipBezier();
                break;
            }

            MyError err = draw_bezierto(p, output);
            if (err) {
                return err;
            }
            break;
        }
        case Draw::DrawOp_Gap:
        case Draw::DrawOp_Line: {
            if (!draw_has_subpath(open)) {
                return psWS.messages.lookupError(ErrorBlock_BadPathSequence);
            }
            Draw::Point point = p.readPoint();
            if (draw_should_output_subpath(open)) {
                MyError err = draw_lineto(point, output);
                if (err) {
                    return err;
                }
            }
            break;
        }
        default:
            return psWS.messages.lookupError(ErrorBlock_BadPathElement);
        }
    }
}

/* Output a path for stroking.
 * Closed strokes close open components; open strokes do not.
 * Gaps are treated as moves, which can require splitting a component
 * into multiple PostScript path components.
 */
static MyError draw_outputpathforstroke(Draw::Path path,
                                        bool closeOpen,
                                        PDriverWS& psWS,
                                        Output& output)
{
    Draw::Path componentEnd = path;

    for (;;) {
        Draw::Path componentStart;
        Draw::Path gap;
        bool closeComponent = false;

        for (;;) {
            Draw::Path element = componentEnd;
            Draw::DrawOp op = componentEnd.readOp();

            if (op > Draw::DrawOp_Line)
                return psWS.messages.lookupError(ErrorBlock_BadPathElement);

            switch (op) {
            case Draw::DrawOp_End:
                if (componentStart.isNull())
                    return nullptr;
                componentEnd = element;
                closeComponent = closeOpen;
                goto componentFound;

            case Draw::DrawOp_Continue:
                componentEnd.continueAt();
                break;

            case Draw::DrawOp_Move:
            case Draw::DrawOp_MoveInternal:
                if (!componentStart.isNull()) {
                    componentEnd = element;
                    closeComponent = closeOpen;
                    goto componentFound;
                }
                componentStart = element;
                componentEnd.skipPoint();
                gap = Draw::Path();
                break;

            case Draw::DrawOp_CloseWithGap:
                if (componentStart.isNull())
                    return psWS.messages.lookupError(ErrorBlock_BadPathSequence);
                gap = element;
                closeComponent = false;
                goto componentFound;

            case Draw::DrawOp_CloseWithLine:
                if (componentStart.isNull())
                    return psWS.messages.lookupError(ErrorBlock_BadPathSequence);
                closeComponent = true;
                goto componentFound;

            case Draw::DrawOp_Bezier:
                if (componentStart.isNull())
                    return psWS.messages.lookupError(ErrorBlock_BadPathSequence);
                componentEnd.skipBezier();
                break;

            case Draw::DrawOp_Gap:
                if (componentStart.isNull())
                    return psWS.messages.lookupError(ErrorBlock_BadPathSequence);
                gap = element;
                componentEnd.skipPoint();
                break;

            case Draw::DrawOp_Line:
                if (componentStart.isNull())
                    return psWS.messages.lookupError(ErrorBlock_BadPathSequence);
                componentEnd.skipPoint();
                break;

            default:
                return psWS.messages.lookupError(ErrorBlock_BadPathElement);
            }
        }

componentFound: {
            Draw::Path p = gap.isNull() ? componentStart : gap;

            for (;;) {
                Draw::DrawOp op = p.readOp();

                if (op > Draw::DrawOp_Line)
                    return psWS.messages.lookupError(ErrorBlock_BadPathElement);

                switch (op) {
                case Draw::DrawOp_Continue:
                    p.continueAt();
                    break;

                case Draw::DrawOp_Move:
                case Draw::DrawOp_MoveInternal: {
                    Draw::Point point = p.readPoint();
                    MyError err = (closeComponent && !gap.isNull())
                        ? draw_lineto(point, output)
                        : draw_moveto(point, output);
                    if (err)
                        return err;
                    break;
                }

                case Draw::DrawOp_CloseWithGap:
                case Draw::DrawOp_CloseWithLine:
                    break;

                case Draw::DrawOp_Bezier: {
                    MyError err = draw_bezierto(p, output);
                    if (err)
                        return err;
                    break;
                }

                case Draw::DrawOp_Gap: {
                    MyError err = draw_moveto(p.readPoint(), output);
                    if (err)
                        return err;
                    break;
                }

                case Draw::DrawOp_Line: {
                    MyError err = draw_lineto(p.readPoint(), output);
                    if (err)
                        return err;
                    break;
                }

                case Draw::DrawOp_End:
                    p = componentEnd;
                    break;

                default:
                    return psWS.messages.lookupError(ErrorBlock_BadPathElement);
                }

                if (p == componentEnd) {
                    if (!gap.isNull()) {
                        p = componentStart;
                    } else {
                        if (closeComponent) {
                            MyError err = draw_closepath(output);
                            if (err)
                                return err;
                        }
                        break;
                    }
                }

                if (p == gap)
                    break;
            }
        }
    }
}

static MyError draw_outputpathforopenstroke(Draw::Path path,
                                            PDriverWS& psWS,
                                            Output& output)
{
    return draw_outputpathforstroke(path, false, psWS, output);
}

static MyError draw_outputpathforclosedstroke(Draw::Path path,
                                              PDriverWS& psWS,
                                              Output& output)
{
    return draw_outputpathforstroke(path, true, psWS, output);
}

MyError draw_boundaryonly(Draw::Path path,
                                  const Draw::Matrix* matrix,
                                  Draw::Unit flatness,
                                  CoreJobWS& coreJob)
{
    CoreWS& ws = CoreWS::instance();
    JobWS& job(toJobWS(coreJob));
    PDriverWS& psWS = *(PDriverWS *)&ws;
    Output& output(job.output());
    MyError err;
    if ((err = draw_outputtransformandflatness(matrix, flatness, output, job, ws)) != nullptr)
        return err;
    if ((err = draw_outputpathforclosedstroke(path, psWS, output)) != nullptr)
        return err;
    if ((err = output.str("0 LW St ")) != nullptr)
        return err;
 
    return output_grestore(job);
}

MyError draw_interiornobdry(Draw::Path path,
                                    uint32_t flags,
                                    const Draw::Matrix* matrix,
                                    Draw::Unit flatness,
                                    CoreJobWS& coreJob)
{
    return draw_interior(path, flags, matrix, flatness, coreJob);
}

MyError draw_interior(Draw::Path path,
                              uint32_t flags,
                              const Draw::Matrix* matrix,
                              Draw::Unit flatness,
                              CoreJobWS& coreJob)
{
    CoreWS& ws = CoreWS::instance();
    JobWS& job = toJobWS(coreJob);
    PDriverWS& psWS = *(PDriverWS *)&ws;
    Output& output(job.output());
    MyError err = draw_outputtransformandflatness(matrix, flatness, output, job, ws);
    if (err) {
        return err;
    }
    if ((err = draw_outputpathforfill(path, psWS, output)) != nullptr)
        return err;
    if ((flags & 2u) != 0) {
        if ((err = output.str("eo")) != nullptr)
            return err;
    }
    if ((err = output.str("fill ")) != nullptr)
        return err;
    return output_grestore(job);
}

MyError draw_interiorwithbdry(Draw::Path path,
                                      uint32_t flags,
                                      const Draw::Matrix* matrix,
                                      Draw::Unit flatness,
                                      CoreJobWS& coreJob)
{
    MyError err = draw_interior(path, flags, matrix, flatness, coreJob);
    if (err) {
        return err;
    }
    return draw_boundaryonly(path, matrix, flatness, coreJob);
}

static bool draw_stroke_is_easy(const Draw::CapJoin* capJoin)
{
    return capJoin->startCapStyle() <= 2 &&
           capJoin->startCapStyle() == capJoin->endCapStyle();
}

/* If both caps are the same and neither is triangular, we can output a
 * PostScript stroke directly. Otherwise, use Draw_ProcessPath to create
 * the outline and fill it.
 */
MyError draw_strokenobdry(Draw::Path path,
                                  const Draw::Matrix* matrix,
                                  Draw::Unit flatness,
                                  Draw::Unit thickness,
                                  const Draw::CapJoin* capJoin,
                                  const Draw::DashPattern* dash,
                                  CoreJobWS& coreJob)
{
    return draw_stroke(path, matrix, flatness, thickness, capJoin, dash,
                       coreJob);
}

MyError draw_strokewithbdry(Draw::Path path,
                                    const Draw::Matrix* matrix,
                                    Draw::Unit flatness,
                                    Draw::Unit thickness,
                                    const Draw::CapJoin* capJoin,
                                    const Draw::DashPattern* dash,
                                    CoreJobWS& coreJob)
{
    return draw_stroke(path, matrix, flatness, thickness, capJoin, dash,
                       coreJob);
}

// `draw_stroke_easy`
// This is a (comparatively) easy case - we simply need to output the path
// correctly and tell PostScript to stroke it.
static MyError draw_stroke_easy(Draw::Path path,
                                const Draw::Matrix* matrix,
                                Draw::Unit flatness,
                                Draw::Unit thickness,
                                const Draw::CapJoin* capJoin,
                                const Draw::DashPattern* dash,
                                PDriverWS& psWS,
                                Output& output,
                                JobWS& job,
                                CoreWS& ws)
{
    MyError err;

    if ((err = draw_outputtransformandflatness(matrix, flatness, output, job, ws)) != nullptr)
        return err;
    if ((err = draw_outputpathforopenstroke(path, psWS, output)) != nullptr)
        return err;
    if ((err = draw_outputdashpattern(dash, output)) != nullptr)
        return err;
    if ((err = draw_outputcapandjoin(capJoin, output)) != nullptr)
        return err;
    if ((err = output.num(thickness.raw())) != nullptr)
        return err;
    if ((err = output.str(" LW St ")) != nullptr)
        return err;

    return output_grestore(job);
}

MyError draw_stroke(Draw::Path path,
                            const Draw::Matrix* matrix,
                            Draw::Unit flatness,
                            Draw::Unit thickness,
                            const Draw::CapJoin* capJoin,
                            const Draw::DashPattern* dashPattern,
                            CoreJobWS& coreJob)
{
    CoreWS& ws = CoreWS::instance();
    JobWS& job = toJobWS(coreJob);
    PDriverWS& psWS = *(PDriverWS *)&ws;
    Output& output(job.output());

    if (draw_stroke_is_easy(capJoin))
        return draw_stroke_easy(path, matrix, flatness, thickness,
                                capJoin, dashPattern, psWS, output, job, ws);

#if PSFreeFlatness
    if (flatness == Draw::Unit::zero()) {
        flatness = Draw::Unit::fromRaw(0x200);
    }
    flatness /= 4;
    if (flatness < Draw::Unit::fromRaw(2)) {
        flatness = Draw::Unit::fromRaw(2);
    }
#endif

    uint32_t size;
    MyError err = Draw::processPathSpecial(path,
                                    ProcessPath_Flatten + ProcessPath_Thicken,
                                    0, // Don't transform path
                                    flatness.raw(),
                                    thickness.raw(),
                                    capJoin,
                                    dashPattern,
                                    DrawSpec_Count,
                                    size);
    if (err) {
        return err;
    }

    // Use Draw_ProcessPath to find out how big a buffer is required for the
    // outline. Then get that size of chunk from the RMA. Then do a real
    // Draw_ProcessPath into it. (We use Draw_ProcessPath rather than
    // Draw_StrokePath to avoid the re-flattening operation after thickening,
    // as this operation simply loses accuracy and uses extra workspace...)
    ScopedPtr<char> flattened;
    if (!flattened.allocate(size))
        return MyError::OOM();

    // Put header into RMA block to make it a Draw output buffer.
    Draw::Path flattenedPath((Draw::PathComponent*)(void*)flattened.get());
    flattenedPath.initialiseOutputBuffer(size);

    if ((err = Draw::processPath(path,
                                 ProcessPath_Flatten + ProcessPath_Thicken,
                                 nullptr,
                                 flatness.raw(),
                                 thickness.raw(),
                                 capJoin,
                                 dashPattern,
                                 flattenedPath)) != nullptr)
    {
        return err;
    }

    if ((err = draw_outputtransformandflatness(matrix, flatness, output, job, ws)) != nullptr)
        return err;

    if ((err = draw_outputbysubpaths(flattenedPath, psWS, output)) != nullptr)
        return err;

    return output_grestore(job);
}

MyError draw_thinstroke(Draw::Path path,
                                const Draw::Matrix* matrix,
                                Draw::Unit flatness,
                                const Draw::DashPattern* dash,
                                CoreJobWS& coreJob)
{
    CoreWS& ws = CoreWS::instance();
    JobWS& job = toJobWS(coreJob);
    PDriverWS& psWS = *(PDriverWS *)&ws;
    Output& output(job.output());
    MyError err;

    if ((err = draw_outputtransformandflatness(matrix, flatness, output, job, ws)) != nullptr)
        return err;
    if ((err = draw_outputpathforopenstroke(path, psWS, output)) != nullptr)
        return err;
    if ((err = draw_outputdashpattern(dash, output)) != nullptr)
        return err;
    if ((err = output.str("0 LW St ")) != nullptr)
        return err;

    return output_grestore(job);
}
