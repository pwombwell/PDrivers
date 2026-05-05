#include "PDriver.h"
#include "InterceptDraw.h"

#include "Colour.h"
#include "Constants.h"
#include "Device.h"
#include "Job.h"
#include "MsgCode.h"
#include "Workspace.h"

#include "RLib/Constants/Draw.h"
#include "RLib/OS/Error.h"

VectorReturn InterceptDraw::pass()
{
    return VectorReturn::pass();
}

VectorReturn InterceptDraw::drawReturn(MyError err)
{
    MyError escapeErr = m_ws.m_escapeState.disableAndCheck();
    if (escapeErr)
        err = escapeErr;

    if (err)
        err = m_job.makePersistentError(err);

    return VectorReturn(err);
}

VectorReturn InterceptDraw::processPath(OS::Regs& regs)
{
    // This is a Draw_ProcessPath call. We can hand it on to the Draw module if
    // it isn't meant to produce screen output. Otherwise, we produce an error.
    uint32_t reason = regs[7];
    uint32_t reasonMinusOne = reason - 1;

    if (reasonMinusOne > 1)
        return pass();

    MyError err = m_job.checkPersistentError();
    if (err)
        return VectorReturn(err);

    err = m_ws.messages.lookupError(ErrorBlock_PrintCantDrawPlot);
    err = m_job.makePersistentError(err);
    return VectorReturn(err);
}

VectorReturn InterceptDraw::fill(OS::Regs& regs)
{
    MyError err = m_job.checkPersistentError();
    if (err)
        return VectorReturn(err);

    if ((err = m_ws.m_escapeState.enable()) != nullptr)
        return VectorReturn(m_job.makePersistentError(err));

    if ((err = vdu5_flush(m_job)) != nullptr)
        return drawReturn(err);

    uint32_t fillStyle = regs[1];

#if PrinterDrawBit
    if (fillStyle & (1u << 16)) {
        fillStyle &= ~(1u << 16);
    } else
#endif
    {
        uint32_t rgb = 0;
        if (!gcol_lookupfg(Disabled_VDU, &rgb, m_job))
            return drawReturn(nullptr);

        err = colour_setrealrgb(rgb, m_job);
        if (err)
            return drawReturn(err);
    }

    uint32_t selector = fillStyle;
    if ((selector & ~0x3Eu) != 0u)
        selector = 0x40u; /* 2_1000000: fake table entry */
    else
        selector &= 0x3Cu;

    Draw::Path path(regs.as<Draw::PathComponent*>(0));
    const Draw::Matrix* matrix = regs.as<const Draw::Matrix*>(2);
    Draw::Unit flatness = Draw::Unit::fromRaw(regs.as<int32_t>(3));

    switch (selector) {
    case 0x00u: /* 2_0000X0 */
        if (fillStyle == 0u)
            err = draw_interior(path, 0x30u, matrix, flatness, m_job);
        break;

    case 0x18u: /* 2_0110X0 */
        err = draw_boundaryonly(path, matrix, flatness, m_job);
        break;

    case 0x20u: /* 2_1000X0 */
        err = draw_interiornobdry(path, fillStyle, matrix, flatness, m_job);
        break;

    case 0x30u: /* 2_1100X0 */
        err = draw_interior(path, fillStyle, matrix, flatness, m_job);
        break;

    case 0x38u: /* 2_1110X0 */
        err = draw_interiorwithbdry(path, fillStyle, matrix, flatness, m_job);
        break;

    case 0x3Cu: /* 2_1111X0 */
        err = plot_fillclipbox(m_job);
        break;

    default:
        err = m_ws.messages.lookupError(ErrorBlock_PrintCantThisFill);
        break;
    }

    return drawReturn(err);
}

VectorReturn InterceptDraw::stroke(OS::Regs& regs)
{
    MyError err = m_job.checkPersistentError();
    if (err)
        return VectorReturn(err);

    if ((err = m_ws.m_escapeState.enable()) != nullptr)
        return VectorReturn(m_job.makePersistentError(err));

    if ((err = vdu5_flush(m_job)) != nullptr)
        return drawReturn(err);

    uint32_t fillStyle = regs[1];

#if PrinterDrawBit
    if (fillStyle & (1u << 16)) {
        fillStyle &= ~(1u << 16);
    } else
#endif
    {
        uint32_t rgb = 0;
        if (!gcol_lookupfg(Disabled_VDU, &rgb, m_job))
            return drawReturn(nullptr);

        err = colour_setrealrgb(rgb, m_job);
        if (err)
            return drawReturn(err);
    }

    Draw::Path path(regs.as<Draw::PathComponent*>(0));
    const Draw::Matrix* matrix = regs.as<const Draw::Matrix*>(2);
    Draw::Unit flatness = Draw::Unit::fromRaw(regs.as<int32_t>(3));
    Draw::Unit thickness = Draw::Unit::fromRaw(regs.as<int32_t>(4));
    const Draw::CapJoin* capJoin = regs.as<const Draw::CapJoin*>(5);
    const Draw::DashPattern* dashPattern = regs.as<const Draw::DashPattern*>(6);

    if (thickness > Draw::Unit::zero()) {
        uint32_t selector = fillStyle;
        if ((selector & ~0x8000003Cu) != 0u)
            selector = 0x40u; /* 2_1000000: fake table entry */
        else
            selector &= 0x3Cu;

        switch (selector) {
        case 0x00u: /* 2_000000 */
            err = draw_stroke(path, matrix, flatness, thickness, capJoin, dashPattern, m_job);
            break;

        case 0x20u: /* 2_100000 */
            err = draw_strokenobdry(path, matrix, flatness, thickness, capJoin, dashPattern, m_job);
            break;

        case 0x30u: /* 2_110000 */
            err = draw_stroke(path, matrix, flatness, thickness, capJoin, dashPattern, m_job);
            break;

        case 0x38u: /* 2_111000 */
            err = draw_strokewithbdry(path, matrix, flatness, thickness, capJoin, dashPattern, m_job);
            break;

        case 0x3Cu: /* 2_111100 */
            err = plot_fillclipbox(m_job);
            break;

        default:
            err = m_ws.messages.lookupError(ErrorBlock_PrintCantThisFill);
            break;
        }
    } else {
        uint32_t selector = fillStyle;
        if ((selector & ~0x8000003Fu) != 0u) {
            selector = 0x04u; /* 2_000100: invalid */
        } else {
            uint32_t tmp = selector & 0x3Fu;
            if (tmp == 0u)
                selector = 0x18u; /* default */
            else
                selector = tmp;

            selector &= 0x0Cu;
        }

        switch (selector) {
        case 0x00u: /* 2_XX00XX */
            break;

        case 0x08u: /* 2_XX10XX */
            err = draw_thinstroke(path, matrix, flatness, dashPattern, m_job);
            break;

        case 0x0Cu: /* 2_XX11XX */
            err = plot_fillclipbox(m_job);
            break;

        default:
            err = m_ws.messages.lookupError(ErrorBlock_PrintCantThisFill);
            break;
        }
    }

    return drawReturn(err);
}

VectorReturn InterceptDraw::intercept(OS::Regs& regs, CoreWS& ws)
{
    /* Is this Draw output we're not interested in? */
    if (ws.m_interceptMgr.hasPassthrough(Passthrough_Draw))
        return VectorReturn::pass();

    /* Intercept but do precisely nothing if it is a counting pass. */
    if (ws.m_countingPass)
        return VectorReturn::claim();

    CoreJobWS* job = ws.currentJob();
    if (job == nullptr)
        return VectorReturn::pass();

    InterceptDraw draw(ws, *job);

    if (regs[8] == DrawReas_Fill)
        return draw.fill(regs);

    if (regs[8] == DrawReas_Stroke)
        return draw.stroke(regs);

    if (regs[8] == DrawReas_ProcessPath)
        return draw.processPath(regs);

    return VectorReturn::pass();
}
