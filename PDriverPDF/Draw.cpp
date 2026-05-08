#include "Core/PDriver.h"

#include "Colour.h"
#include "Output.h"
#include "PDriverPDF.h"
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
#include <stdint.h>

/* Draw call handling routines for the PDF printer driver. */

static MyError pdf_setlinewidth(Output& output, Draw::Unit width)
{
    MyError err = output.numSpace(width);
    if (err)
        return err;

    return output.str("w\n");
}

static MyError pdf_stroke(Output& output)
{
    return output.str("S\n");
}

static MyError pdf_fill(Output& output)
{
    return output.str("f\n");
}

static MyError pdf_fill_eo(Output& output)
{
    return output.str("f*\n");
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

static MyError draw_outputcoords(Output& output, Point<Draw::Unit> p)
{
    MyError err = output.numSpace(p.x);
    if (err)
        return err;

    return output.numSpace(p.y);
}

static MyError draw_moveto(Output& output, Point<Draw::Unit> p)
{
    MyError err = draw_outputcoords(output, p);
    if (err)
        return err;

    if ((err = output.str("m\n")) != nullptr)
        return err;

    return draw_check_escape();
}

static MyError draw_lineto(Output& output, Point<Draw::Unit> p)
{
    MyError err = draw_outputcoords(output, p);
    if (err)
        return err;

    if ((err = output.str("l\n")) != nullptr)
        return err;

    return draw_check_escape();
}

static MyError draw_curveto(Output& output, Point<Draw::Unit> p)
{
    MyError err;
    
    if ((err = draw_outputcoords(output, p)) != nullptr)
        return err;

    if ((err = output.str("c\n")) != nullptr)
        return err;

    return draw_check_escape();
}

static MyError draw_closepath(Output& output)
{
    MyError err = output.str("h\n");
    if (err)
        return err;

    return draw_check_escape();
}

static MyError draw_closeandfillpath(Output& output)
{
    MyError err = output.str("h\nf\n");
    if (err)
        return err;

    return draw_check_escape();
}

/* Output the dash pattern pointed to by dash. */
static MyError draw_outputdashpattern(Output& output, const Draw::DashPattern* dash)
{
    if (dash == nullptr)
        return nullptr;

    MyError err;
    if ((err = output.str("[ ")) != nullptr)
        return err;

    uint32_t count = dash->elementCount();
    for (uint32_t i = 0; i < count; ++i) {
        if ((err = output.numSpace(dash->element(i))) != nullptr)
            return err;
    }

    if ((err = output.str("] ")) != nullptr)
        return err;

    if ((err = output.numSpace(dash->startOffset())) != nullptr)
        return err;

    return output.str("d\n");
}

/* Output the cap and join specification. */
static MyError draw_outputcapandjoin(Output& output, const Draw::CapJoin* capJoin)
{
    MyError err;
    
    if ((err = output.numSpace((int32_t)capJoin->startCapStyle())) != nullptr)
        return err;
    if ((err = output.str("J\n")) != nullptr)
        return err;
    if ((err = output.numSpace((int32_t)capJoin->joinStyle())) != nullptr)
        return err;
    if ((err = output.str("j\n")) != nullptr)
        return err;
    if ((err = output.numSpace(capJoin->miterLimit())) != nullptr)
        return err;

    return output.str("M\n");
}

/* Output the Draw transformation (matrix then origin-usersoffset) and flatness.
 * A "gsave" is emitted first.
 */
static MyError draw_outputtransformandflatness(const Draw::Matrix* matrix,
                                               Draw::Unit flatness,
                                               Output& output,
                                               JobWS& job,
                                               PDriverWS& ws)
{
    MyError err;

#if PSCoordSpeedUps
    if ((err = ensure_OScoords(output, job)) != nullptr)
        return err;
    if ((err = colour_ensure(job)) != nullptr)
        return err;
    if ((err = output_gsave(job)) != nullptr)
        return err;
#else
    if ((err = output_gsave(job)) != nullptr)
        return err;
#endif

    Draw::Matrix drawMatrix = matrix ? *matrix : Draw::Matrix::identity();
    Offset<OS::Unit> translation = job.origin - job.usersoffset;
    drawMatrix.e += translation.dx;
    drawMatrix.f += translation.dy;

    if (!drawMatrix.isIdentity()) {
        if ((err = output.writeReal(drawMatrix.a.raw(), 65536)) != nullptr)
            return err;
        if ((err = output.str(' ')) != nullptr)
            return err;
        if ((err = output.writeReal(drawMatrix.b.raw(), 65536)) != nullptr)
            return err;
        if ((err = output.str(' ')) != nullptr)
            return err;
        if ((err = output.writeReal(drawMatrix.c.raw(), 65536)) != nullptr)
            return err;
        if ((err = output.str(' ')) != nullptr)
            return err;
        if ((err = output.writeReal(drawMatrix.d.raw(), 65536)) != nullptr)
            return err;
        if ((err = output.str(' ')) != nullptr)
            return err;
        if ((err = output.numSpace(drawMatrix.e)) != nullptr)
            return err;
        if ((err = output.numSpace(drawMatrix.f)) != nullptr)
            return err;
        if ((err = output.str("cm\n")) != nullptr)
            return err;
    }

    (void)flatness;
    (void)ws;

    return nullptr;
}

/* Ignore special segments, close open subpaths, and treat gaps as lines. */
static MyError draw_outputpathforfill(Output& output, Draw::Path path,
                                              PDriverWS& psWS)
{
    MyError err;
    Draw::DrawOp open = Draw::DrawOp_End;
    Draw::Point point;

    for (;;) {
        Draw::DrawOp op = path.readOp();

        if (op > Draw::DrawOp_Line) {
            return psWS.messages.lookupError(ErrorBlock_BadPathElement);
        }

        switch (op) {
        case Draw::DrawOp_End:
            if (open != Draw::DrawOp_Move)
                return nullptr;

            return draw_closepath(output);
        case Draw::DrawOp_Continue:
            path.continueAt();
            break;
        case Draw::DrawOp_Move:
        case Draw::DrawOp_MoveInternal: {
            if (open == Draw::DrawOp_Move && (err = draw_closepath(output)) != nullptr)
                return err;

            point = path.readPoint();
            open = op;
            if (open <= Draw::DrawOp_Move && (err = draw_moveto(output, point)) != nullptr)
                return err;

            break;
        }
        case Draw::DrawOp_CloseWithGap:
        case Draw::DrawOp_CloseWithLine:
            if (open == Draw::DrawOp_Move && (err = draw_closepath(output)) != nullptr)
                return err;

            open = Draw::DrawOp_End;
            break;
        case Draw::DrawOp_Bezier:
            if (open < Draw::DrawOp_Move)
                return psWS.messages.lookupError(ErrorBlock_BadPathSequence);

            if (open > Draw::DrawOp_Move) {
                path.skipBezier();
                break;
            }

            point = path.readPoint();
            if ((err = draw_outputcoords(output, point)) != nullptr)
                return err;

            point = path.readPoint();
            if ((err = draw_outputcoords(output, point)) != nullptr)
                return err;

            point = path.readPoint();
            if ((err = draw_curveto(output, point)) != nullptr)
                return err;
            break;
        case Draw::DrawOp_Gap:
        case Draw::DrawOp_Line:
            if (open < Draw::DrawOp_Move)
                return psWS.messages.lookupError(ErrorBlock_BadPathSequence);

            point = path.readPoint();
            if (open == Draw::DrawOp_Move && (err = draw_lineto(output, point)) != nullptr)
                return err;

            break;

        default:
            return psWS.messages.lookupError(ErrorBlock_BadPathElement);
        }
    }
}

/* As draw_outputpathforfill, but fill each subpath as it completes. */
static MyError draw_outputbysubpaths(Output& output, Draw::Path path,
                                              PDriverWS& psWS)
{
    MyError err;
    Draw::DrawOp open = Draw::DrawOp_End;
    Draw::Point point;

    for (;;) {
        Draw::DrawOp op = path.readOp();

        if (op > Draw::DrawOp_Line) {
            return psWS.messages.lookupError(ErrorBlock_BadPathElement);
        }

        switch (op) {
        case Draw::DrawOp_End:
            if (open == Draw::DrawOp_Move && (err = draw_closeandfillpath(output)) != nullptr)
                return err;

            return nullptr;
        case Draw::DrawOp_Continue:
            path.continueAt();
            break;
        case Draw::DrawOp_Move:
        case Draw::DrawOp_MoveInternal: {
            if (open == Draw::DrawOp_Move && (err = draw_closeandfillpath(output)) != nullptr)
                return err;

            point = path.readPoint();
            open = op;
            if (open <= Draw::DrawOp_Move && (err = draw_moveto(output, point)) != nullptr)
                return err;

            break;
        }
        case Draw::DrawOp_CloseWithGap:
        case Draw::DrawOp_CloseWithLine:
            if (open == Draw::DrawOp_Move && (err = draw_closeandfillpath(output)) != nullptr)
                return err;

            open = Draw::DrawOp_End;
            break;
        case Draw::DrawOp_Bezier:
            if (open < Draw::DrawOp_Move)
                return psWS.messages.lookupError(ErrorBlock_BadPathSequence);

            if (open > Draw::DrawOp_Move) {
                path.skipBezier();
                break;
            }

            point = path.readPoint();
            if ((err = draw_outputcoords(output, point)) != nullptr)
                return err;

            point = path.readPoint();
            if ((err = draw_outputcoords(output, point)) != nullptr)
                return err;

            point = path.readPoint();
            if ((err = draw_curveto(output, point)) != nullptr)
                return err;
            break;
        case Draw::DrawOp_Gap:
        case Draw::DrawOp_Line:
            if (open < Draw::DrawOp_Move)
                return psWS.messages.lookupError(ErrorBlock_BadPathSequence);

            point = path.readPoint();
            if (open == Draw::DrawOp_Move && (err = draw_lineto(output, point)) != nullptr)
                return err;

            break;
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
static MyError draw_outputpathforstroke(Output& output,
                                        Draw::Path path,
                                        bool closeOpen,
                                        PDriverWS& psWS)
{
    MyError err;
    Draw::Path componentEnd = path;
    Draw::Point point;

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
                case Draw::DrawOp_MoveInternal:
                    point = p.readPoint();
                    err = (closeComponent && !gap.isNull())
                        ? draw_lineto(output, point)
                        : draw_moveto(output, point);
                    if (err)
                        return err;
                    break;

                case Draw::DrawOp_CloseWithGap:
                case Draw::DrawOp_CloseWithLine:
                    break;

                case Draw::DrawOp_Bezier: {
                    point = p.readPoint();
                    if ((err = draw_outputcoords(output, point)) != nullptr)
                        return err;

                    point = p.readPoint();
                    if ((err = draw_outputcoords(output, point)) != nullptr)
                        return err;

                    point = p.readPoint();
                    if ((err = draw_curveto(output, point)) != nullptr)
                        return err;
                    break;
                }

                case Draw::DrawOp_Gap:
                    point = p.readPoint();
                    if ((err = draw_moveto(output, point)) != nullptr)
                        return err;
                    break;

                case Draw::DrawOp_Line:
                    point = p.readPoint();
                    if ((err = draw_lineto(output, point)) != nullptr)
                        return err;
                    break;

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

static MyError draw_outputpathforopenstroke(Output& output, Draw::Path path,
                                                    PDriverWS& psWS)
{
    return draw_outputpathforstroke(output, path, false, psWS);
}

static MyError draw_outputpathforclosedstroke(Output& output, Draw::Path path,
                                                      PDriverWS& psWS)
{
    return draw_outputpathforstroke(output, path, true, psWS);
}

MyError draw_boundaryonly(Draw::Path path,
                                  const Draw::Matrix* matrix,
                                  Draw::Unit flatness,
                                  CoreJobWS& coreJob)
{
    CoreWS& ws = CoreWS::instance();
    JobWS& job = toJobWS(coreJob);
    PDriverWS& psWS = *(PDriverWS *)&ws;
    MyError err;
    Output& output(job.output());

    if ((err = draw_outputtransformandflatness(matrix, flatness, output, job, psWS)) != nullptr)
        return err;
    if ((err = draw_outputpathforclosedstroke(output, path, psWS)) != nullptr)
        return err;
    if ((err = pdf_setlinewidth(output, Draw::Unit::zero())) != nullptr)
        return err;
    if ((err = pdf_stroke(output)) != nullptr)
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
    MyError err;
    Output& output(job.output());
    
    if ((err = draw_outputtransformandflatness(matrix, flatness, output, job, psWS)) != nullptr)
        return err;
    if ((err = draw_outputpathforfill(output, path, psWS)) != nullptr)
        return err;

    if ((flags & 2u) != 0) {
        if ((err = pdf_fill_eo(output)) != nullptr)
            return err;
    } else {
        if ((err = pdf_fill(output)) != nullptr)
            return err;
    }
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

static MyError draw_stroke_easy(Draw::Path path,
                                const Draw::Matrix* matrix,
                                Draw::Unit flatness,
                                Draw::Unit thickness,
                                const Draw::CapJoin* capJoin,
                                const Draw::DashPattern* dash,
                                PDriverWS& ws)
{
    MyError err;
    JobWS& job(*(JobWS*)ws.currentJob());

    Output& output(job.output());

    if ((err = draw_outputtransformandflatness(matrix, flatness, output, job, ws)) != nullptr)
        return err;
    if ((err = draw_outputpathforopenstroke(output, path, ws)) != nullptr)
        return err;
    if ((err = draw_outputdashpattern(output, dash)) != nullptr)
        return err;
    if ((err = draw_outputcapandjoin(output, capJoin)) != nullptr)
        return err;
    if ((err = pdf_setlinewidth(output, thickness)) != nullptr)
        return err;
    if ((err = pdf_stroke(output)) != nullptr)
        return err;

    return output_grestore(job);
}

MyError draw_stroke(Draw::Path path,
                            const Draw::Matrix* matrix,
                            Draw::Unit flatness,
                            Draw::Unit thickness,
                            const Draw::CapJoin* capJoin,
                            const Draw::DashPattern* dash,
                            CoreJobWS& coreJob)
{
    CoreWS& ws = CoreWS::instance();
    JobWS& job = toJobWS(coreJob);
    PDriverWS& psWS = *(PDriverWS *)&ws;
    if (draw_stroke_is_easy(capJoin))
        return draw_stroke_easy(path, matrix, flatness, thickness, capJoin, dash, psWS);

#if PSFreeFlatness
    if (flatness == Draw::Unit::zero()) {
        flatness = Draw::Unit::fromInt(2);
    }
    flatness /= 4;
    if (flatness < Draw::Unit::fromRaw(2)) {
        flatness = Draw::Unit::fromRaw(2);
    }
#endif

    uint32_t size = 0;
    MyError err = Draw::processPathSpecial(path,
                                           ProcessPath_Flatten + ProcessPath_Thicken,
                                           nullptr,
                                           (uint32_t)flatness.raw(),
                                           (uint32_t)thickness.raw(),
                                           capJoin,
                                           dash,
                                           DrawSpec_Count,
                                           size);
    if (err) {
        return err;
    }

    ScopedPtr<char> flattened;
    if (!flattened.allocate(size))
        return MyError::OOM();
    Draw::Path flattenedPath((Draw::PathComponent*)(void*)flattened.get());

    flattenedPath.initialiseOutputBuffer(size);

    err = Draw::processPath(path,
                            ProcessPath_Flatten + ProcessPath_Thicken,
                            nullptr,
                            (uint32_t)flatness.raw(),
                            (uint32_t)thickness.raw(),
                            capJoin,
                            dash,
                            flattenedPath);
    if (err)
        return err;

    Output& output(job.output());

    if ((err = draw_outputtransformandflatness(matrix, flatness, output, job, psWS)) != nullptr)
        return err;

    if ((err = draw_outputbysubpaths(output, flattenedPath, psWS)) != nullptr)
        return err;

    if ((err = output_grestore(job)) != nullptr)
        return err;

    return nullptr;
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
    MyError err;

    Output& output(job.output());
    
    if ((err = draw_outputtransformandflatness(matrix, flatness, output, job, psWS)) != nullptr)
        return err;
    if ((err = draw_outputpathforopenstroke(output, path, psWS)) != nullptr)
        return err;
    if ((err = draw_outputdashpattern(output, dash)) != nullptr)
        return err;
    if ((err = pdf_setlinewidth(output, Draw::Unit::zero())) != nullptr)
        return err;
    if ((err = pdf_stroke(output)) != nullptr)
        return err;

    return output_grestore(job);
}
