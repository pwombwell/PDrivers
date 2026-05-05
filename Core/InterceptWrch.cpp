#include "PDriver.h"
#include "InterceptWrch.h"

#include "Colour.h"
#include "Constants.h"
#include "Device.h"
#include "Job.h"
#include "MsgCode.h"
#include "OS.h"
#include "VDU5.h"
#include "Workspace.h"
#include "cmhg.h"

#include "RLib/Geometry.h"
#include "RLib/OS/Error.h"
#include "RLib/KernelWrapper.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* Helpers from other core modules. */
extern void broadcast_vdu5_changed(CoreWS& ws, uint32_t code);

typedef MyError (*PlotFn)(const Point<OS::Unit>& p0, // new point
                          const Point<OS::Unit>& p1, // old point
                          const Point<OS::Unit>& p2, // older point
                          CoreJobWS& coreJob);

typedef struct PlotDispatch {
    PlotFn fn;
    const OS::ErrorNumber* err;
} PlotDispatch;

VectorReturn InterceptWrch::finish(VectorReturn ret)
{
    MyError escapeErr = m_ws.m_escapeState.disableAndCheck();
    if (escapeErr)
        ret = escapeErr;

    if (ret.isError())
        ret = m_job.makePersistentError(ret.toError());

    return ret;
}

static uint8_t *wrch_queue_end(CoreJobWS& job)
{
    return job.wrch().queueEnd();
}

static uint32_t wrch_queue_word(const CoreJobWS& job,
                                size_t offset_from_end)
{
    return job.wrch().queueWord(offset_from_end);
}

static void wrch_autoadvance(CoreJobWS& job)
{
    const Offset<OS::Unit>& advance(job.vdu5().autoAdvance());
    job.oldpoint += advance;
}

static void wrch_updatecursor(CoreJobWS& job, Offset<OS::Unit> advance)
{
    job.oldpoint += advance;
}

static OS::Unit wrch_horizontalcharorg(const CoreJobWS& job)
{
    if ((job.m_cursorControl & 0x02u) == 0)
        return job.graphicswindow.x0;

    return job.graphicswindow.x1 - job.vdu5().charSize().width;
}

static OS::Unit wrch_verticalcharorg(const CoreJobWS& job)
{
    int32_t pixel = 1;
    pixel <<= job.screenVars.curryeig;

    if ((job.m_cursorControl & 0x04u) == 0)
        return job.graphicswindow.y1 - OS::Unit(pixel);

    return job.graphicswindow.y0 - OS::Unit(pixel) + job.vdu5().charSize().height;
}

static void wrch_tabcursor_common(CoreJobWS& job,
                                  uint32_t tabX,
                                  uint32_t tabY)
{
    Point<OS::Unit> point(wrch_horizontalcharorg(job), wrch_verticalcharorg(job));

    const Offset<OS::Unit>& charAdvance(job.vdu5().charAdvance());
    const Offset<OS::Unit>& lineAdvance(job.vdu5().lineAdvance());

    point.x += charAdvance.dx * tabX + lineAdvance.dx * tabY;
    point.y += charAdvance.dy * tabX + lineAdvance.dy * tabY;

    job.oldpoint = point;
}

VectorReturn InterceptWrch::passDownChar(uint8_t ch)
{
    // Pass directly to the remaining WrchV chain with passthrough set so our
    // interceptor declines the recursive call.
    m_ws.m_interceptMgr.addPassthrough(Passthrough_Wrch);

    m_regs[0] = ch;

    return VectorReturn::pass();
}

VectorReturn InterceptWrch::passDownString(const uint8_t *str, uint32_t length)
{
    for (uint32_t i = 0; i < length; ++i) {
        VectorReturn ret = passDownChar(str[i]);
        if (ret.isError())
            return ret;
    }

    return VectorReturn::pass();
}

MyError InterceptWrch::noPrinterControl()
{
    return m_ws.messages.lookupError(ErrorBlock_PrintCantPrinterVDU);
}

MyError InterceptWrch::noVDU4Chars()
{
    return m_ws.messages.lookupError(ErrorBlock_PrintCantVDU4);
}

static void wrch_enable(CoreJobWS& coreJob)
{
    coreJob.disabled = Disabled(coreJob.disabled & ~Disabled_VDU);
}

static void wrch_disable(CoreJobWS& coreJob)
{
    coreJob.disabled = Disabled(coreJob.disabled | Disabled_VDU);
}

static void wrch_backspace(CoreJobWS& coreJob)
{
    const Offset<OS::Unit>& advance(coreJob.vdu5().charAdvance());
    wrch_updatecursor(coreJob, -advance);
}

static void wrch_forwardspace(CoreJobWS& coreJob)
{
    const Offset<OS::Unit>& advance(coreJob.vdu5().charAdvance());
    wrch_updatecursor(coreJob, advance);
}

static void wrch_linefeed(CoreJobWS& coreJob)
{
    const Offset<OS::Unit>& advance(coreJob.vdu5().lineAdvance());
    wrch_updatecursor(coreJob, advance);
}

static void wrch_lineback(CoreJobWS& coreJob)
{
    const Offset<OS::Unit>& advance(coreJob.vdu5().lineAdvance());
    wrch_updatecursor(coreJob, -advance);
}

MyError InterceptWrch::clearBox()
{
    uint32_t rgb = 0;
    if (!gcol_lookupbg(Disabled_None, &rgb, m_job))
        return nullptr;

    MyError err = colour_setrealrgb(rgb, m_job);
    if (err)
        return err;

    return plot_fillclipbox(m_job);
}

static void wrch_carriagereturn(CoreJobWS& job)
{
    uint8_t m_cursorControl = job.m_cursorControl;
    if ((m_cursorControl & 0x08u) != 0u) {
        job.oldpoint.y = wrch_verticalcharorg(job);
    } else {
        job.oldpoint.x = wrch_horizontalcharorg(job);
    }
}

static void wrch_setgcol(CoreJobWS& job, uint8_t colour, uint8_t action)
{
    job.screenVars.setGCOLandColour(OS::GCOLAction(action), colour);
}

MyError InterceptWrch::noModeChanges()
{
    return m_ws.messages.lookupError(ErrorBlock_PrintCantModeChange);
}

MyError InterceptWrch::cannotHandleVDU23()
{
    return m_ws.messages.lookupError(ErrorBlock_PrintCantThisVDU23);
}

MyError InterceptWrch::deleteChar()
{
    uint32_t rgb = 0;
    if (!gcol_lookupbg(Disabled_None, &rgb, m_job))
        return nullptr;

    MyError err = colour_setrealrgb(rgb, m_job);
    if (err)
        return err;

    const Offset<OS::Unit>& advance(m_job.vdu5().autoAdvance());

    m_job.oldpoint -= advance;
    Point<OS::Unit> p(m_job.oldpoint);

    int32_t pixel = 1;
    pixel <<= m_job.screenVars.curryeig;
    p.y += pixel;

    p += m_job.origin;
    p -= m_job.usersoffset;

    return vdu5_delete(p, m_ws);
}

VectorReturn InterceptWrch::passControlSeq(uint8_t initial)
{
    VectorReturn ret = passDownChar(initial);
    if (ret.isError())
        return ret;

    static const uint8_t wrch_lengths[] = {
        0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 2, 5, 0, 0, 1, 9, 8, 5, 0, 0, 4, 4, 0, 2
    };

    if (initial < ARRAY_SIZE(wrch_lengths)) {
        uint8_t length = wrch_lengths[initial];
        if (length != 0u) {
            const uint8_t *queue_end = wrch_queue_end(m_job);
            const uint8_t *start = queue_end - length;
            ret = passDownString(start, length);
        }
    }

    return ret;
}

MyError InterceptWrch::setCursorControl()
{
    MyError err = vdu5_flush(m_job);
    if (err)
        return err;

    uint8_t *queue_end = wrch_queue_end(m_job);
    uint8_t m_cursorControl = m_job.m_cursorControl;
    m_cursorControl &= queue_end[-7];
    m_cursorControl ^= queue_end[-8];
    m_cursorControl |= 0x40u;
    m_job.m_cursorControl = m_cursorControl;

    m_job.vdu5().adjustAdvances(m_cursorControl);

    return nullptr;
}

MyError InterceptWrch::setFGTint()
{
    uint8_t *queue_end = wrch_queue_end(m_job);
    ((uint8_t *)&m_job.screenVars.fggcol)[0] = queue_end[-7];
    return nullptr;
}

MyError InterceptWrch::setBGTint()
{
    uint8_t *queue_end = wrch_queue_end(m_job);
    ((uint8_t *)&m_job.screenVars.bggcol)[0] = queue_end[-7];
    return nullptr;
}

MyError InterceptWrch::changeCharSize()
{
    MyError err = vdu5_flush(m_job);
    if (err)
        return err;

    uint32_t word0 = wrch_queue_word(m_job, 8);
    uint32_t word1 = wrch_queue_word(m_job, 4);

    uint32_t x = word0 >> 16;
    uint32_t y = word1 & 0xFFFFu;

    x <<= m_job.screenVars.currxeig;
    y <<= m_job.screenVars.curryeig;

    uint8_t *queue_end = wrch_queue_end(m_job);
    uint8_t flags = queue_end[-7];

    if ((flags & 0x02u) != 0) {
        m_job.vdu5().charSize().width = x;
        m_job.vdu5().charSize().height = y;
    }
    if ((flags & 0x04u) != 0) {
        m_job.vdu5().charSpacing().dx = x;
        m_job.vdu5().charSpacing().dy = y;
    }

    m_job.vdu5().adjustAdvances(m_job.m_cursorControl);

    return nullptr;
}

VectorReturn InterceptWrch::vdu23_17()
{
    uint8_t* queue_end = wrch_queue_end(m_job);
    uint8_t reason = queue_end[-8];

    switch (reason) {
    case 0:
    case 1:
    case 5:
        return VectorReturn::pass();
    case 2:
        return setFGTint();
    case 3:
        return setBGTint();
    case 4:
        return passControlSeq(23);
    case 6:
        return passControlSeq(23);
    case 7:
        return changeCharSize();
    default:
        return cannotHandleVDU23();
    }
}

#if DoFontSpriteVdu
/* Font/sprite VDU 23 handlers are not yet fully ported. */
#endif

VectorReturn InterceptWrch::vdu23()
{
    uint8_t* queue_end = wrch_queue_end(m_job);
    uint8_t reason = queue_end[-9];

    if (reason >= 32u) {
        broadcast_vdu5_changed(m_ws, reason);
        return passControlSeq(23);
    }

    switch (reason) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
        return passControlSeq(23);
    case 6:
        m_job.setDotPattern(queue_end - 8);
        return VectorReturn::pass();
    case 7:
    case 8:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 28:
    case 29:
    case 30:
    case 31:
        return cannotHandleVDU23();
    case 16:
        return setCursorControl();
    case 17:
        return vdu23_17();
#if DoFontSpriteVdu
    case 25:
    case 26:
    case 27:
        /* TODO: font/sprite VDU 23 handlers. */
        return cannotHandleVDU23();
#else
    case 25:
    case 26:
    case 27:
        return m_ws.messages.lookupError(ErrorBlock_PrintCantFontSpriteVDU);
#endif
    default:
        return cannotHandleVDU23();
    }
}

MyError InterceptWrch::setClipBox()
{
    Rect<OS::Unit> box;

    uint32_t lower = wrch_queue_word(m_job, 8);
    box.x0 = (int16_t)(lower & 0xFFFFu);
    box.y0 = (int16_t)(lower >> 16);

    uint32_t upper = wrch_queue_word(m_job, 4);
    box.x1 = (int16_t)(upper & 0xFFFFu);
    box.y1 = (int16_t)(upper >> 16);

    box.offsetBy(m_job.origin);

    // Convert to inclusive/exclusive
    box.x1++;
    box.y1++;

    m_job.graphicswindow = box;

    box.offsetBy(-m_job.usersoffset);

    return pagebox_setnewbox(box, m_job);
}

#if DoFontSpriteVdu
static MyError wrch_plot_fontcall_dispatch(const Point<OS::Unit>& p0,
                                           const Point<OS::Unit>& p1,
                                           const Point<OS::Unit>& p2,
                                           CoreJobWS& coreJob)
{
    (void)p2;
    (void)p1;
    (void)p0;
    uint8_t *queue_end = wrch_queue_end(coreJob);
    uint8_t plot_type = queue_end[-5];
    coreJob.doingfontplot = (uint8_t)(plot_type | 0x10u);
    coreJob.fontplotseqlen = 0;
    return nullptr;
}

static MyError wrch_plot_spritecall_dispatch(const Point<OS::Unit>& p0,
                                             const Point<OS::Unit>& p1,
                                             const Point<OS::Unit>& p2,
                                             CoreJobWS& coreJob)
{
    (void)p0;
    (void)p1;
    (void)p2;

    CoreWS& ws = CoreWS::instance();
    uint8_t *queue_end = wrch_queue_end(coreJob);
    uint8_t plot_type = queue_end[-5];
    uint8_t action = (uint8_t)(plot_type & 0x03u);
    if (action == 0u) {
        return nullptr;
    }

    uint32_t r0 = coreJob.currentsprite[0];
    uint32_t r1 = coreJob.currentsprite[1];
    uint32_t r2 = coreJob.currentsprite[2];

    if (r0 == 0u) {
        return ws.messages.lookupError(ErrorBlock_PrintNoCurrentSprite);
    }

    if (action == 2u) {
        return nullptr;
    }

    uint32_t r5;
    if (action > 2u) {
        r0 += SpriteReason_PlotMask;
        r5 = (uint32_t)coreJob.screenVars.bgmode;
    } else {
        r0 += SpriteReason_PutSprite;
        r5 = (uint32_t)coreJob.screenVars.fgmode;
    }

    uint32_t r3 = 0;
    uint32_t r4 = 0;
    uint32_t r6 = 0;
    uint32_t r7 = 0;
    return XOS_SpriteOp(&r0, &r1, &r2, &r3, &r4, &r5, &r6, &r7);
}
#endif

static MyError wrch_plot_dispatch_error(CoreWS& ws,
                                        const OS::ErrorNumber& err_block)
{
    return ws.messages.lookupError(OS::ErrorView(err_block));
}

#define ERRVIEW_PTR(e) ((const OS::ErrorNumber*)(const void*)&(e))

static const PlotDispatch wrch_plot_dispatch[] = {
    { plot_linebothends, nullptr },        /* 0 */
    { plot_linestartonly, nullptr },       /* 1 */
#if RealDottedLines
    { plot_dottedbothends, nullptr },      /* 2 */
    { plot_dottedstartonly, nullptr },     /* 3 */
#else
    { plot_linebothends, nullptr },        /* 2 */
    { plot_linestartonly, nullptr },       /* 3 */
#endif
    { plot_lineendonly, nullptr },         /* 4 */
    { plot_linenoends, nullptr },          /* 5 */
#if RealDottedLines
    { plot_dottedendonly, nullptr },       /* 6 */
    { plot_dottednoends, nullptr },        /* 7 */
#else
    { plot_lineendonly, nullptr },         /* 6 */
    { plot_linenoends, nullptr },          /* 7 */
#endif
    { plot_point, nullptr },               /* 8 */
    { nullptr, ERRVIEW_PTR(ErrorBlock_PrintCantHorizFill) }, /* 9 */
    { plot_triangle, nullptr },            /* 10 */
    { nullptr, ERRVIEW_PTR(ErrorBlock_PrintCantHorizFill) }, /* 11 */
    { plot_rectangle, nullptr },           /* 12 */
    { nullptr, ERRVIEW_PTR(ErrorBlock_PrintCantHorizFill) }, /* 13 */
    { plot_parallelogram, nullptr },       /* 14 */
    { nullptr, ERRVIEW_PTR(ErrorBlock_PrintCantHorizFill) }, /* 15 */
    { nullptr, ERRVIEW_PTR(ErrorBlock_PrintCantFloodFill) }, /* 16 */
    { nullptr, ERRVIEW_PTR(ErrorBlock_PrintCantFloodFill) }, /* 17 */
    { plot_strokecircle, nullptr },        /* 18 */
    { plot_fillcircle, nullptr },          /* 19 */
    { plot_strokearc, nullptr },           /* 20 */
    { plot_fillchord, nullptr },           /* 21 */
    { plot_fillsector, nullptr },          /* 22 */
    { nullptr, ERRVIEW_PTR(ErrorBlock_PrintCantCopyMove) },  /* 23 */
    { plot_strokeellipse, nullptr },       /* 24 */
    { plot_fillellipse, nullptr },         /* 25 */
#if DoFontSpriteVdu
    { wrch_plot_fontcall_dispatch, nullptr }, /* 26 */
#else
    { nullptr, ERRVIEW_PTR(ErrorBlock_PrintCantFontSpriteVDU) }, /* 26 */
#endif
    { nullptr, ERRVIEW_PTR(ErrorBlock_PrintCantUndefPlot) }, /* 27 */
    { nullptr, ERRVIEW_PTR(ErrorBlock_PrintCantUndefPlot) }, /* 28 */
#if DoFontSpriteVdu
    { wrch_plot_spritecall_dispatch, nullptr }, /* 29 */
#else
    { nullptr, ERRVIEW_PTR(ErrorBlock_PrintCantFontSpriteVDU) }, /* 29 */
#endif
    { nullptr, ERRVIEW_PTR(ErrorBlock_PrintCantUndefPlot) }, /* 30 */
    { nullptr, ERRVIEW_PTR(ErrorBlock_PrintCantUndefPlot) }  /* 31 */
};

MyError InterceptWrch::plot()
{
    uint8_t *queue_end = wrch_queue_end(m_job);
    uint8_t plot_type = queue_end[-5];
    uint32_t rgb = 0;
    int want_background = 0;
    MyError err = nullptr;

    if (plot_type == 0xB8u || plot_type == 0xBCu) {
        plot_type &= (uint8_t)~0xF8u;
    }

    uint32_t group = (uint32_t)plot_type >> 3;
    if (group >= ARRAY_SIZE(wrch_plot_dispatch))
        return wrch_plot_dispatch_error(m_ws, ErrorBlock_PrintCantUndefPlot.errnum);

    PlotDispatch dispatch = wrch_plot_dispatch[group];
    if (dispatch.err != nullptr)
        return wrch_plot_dispatch_error(m_ws, *dispatch.err);

    uint32_t word = wrch_queue_word(m_job, 4);

    Point<OS::Unit> p0;

    if ((plot_type & 0x04u) == 0) {
        // Move relative.
        p0 = m_job.oldpoint;
        p0 += Offset<OS::Unit>(int16_t(word & 0xFFFFu), int16_t(word >> 16));
    } else {
        p0 = Point<OS::Unit>(int16_t(word & 0xFFFFu), int16_t(word >> 16));
        p0 += m_job.origin;
    }

    m_job.thispoint = p0;

    Point<OS::Unit> p1 = m_job.oldpoint;
    Point<OS::Unit> p2 = m_job.olderpoint;

    p0 -= m_job.usersoffset;
    p1 -= m_job.usersoffset;
    p2 -= m_job.usersoffset;

    uint32_t colour_type = plot_type & 0x03u;
    if (colour_type == 0|| colour_type == 2)
        goto normal_return;

    want_background = (colour_type == 3);
    if (!gcol_lookupfgorbg(want_background, Disabled_None, &rgb, m_job))
        goto normal_return;

    err = colour_setrealrgb(rgb, m_job);
    if (err)
        return err;

    err = dispatch.fn(p0, p1, p2, m_job);
    if (err)
        return err;

normal_return:
#if DoFontSpriteVdu
    if (m_job.doingfontplot == 0u)
#endif
    {
        m_job.oldestpoint = m_job.olderpoint;
        m_job.olderpoint = m_job.oldpoint;
        m_job.oldpoint = m_job.thispoint;
    }

    return nullptr;
}

VectorReturn InterceptWrch::controlSequence(uint8_t initial)
{
    CoreJobWS& job = m_job;

    MyError err = vdu5_flush(m_job);
    if (err)
        return err;

    if ((job.disabled & Disabled_VDU) != 0u && initial != 6u)
        return VectorReturn::pass();

    switch (initial) {
    case 0:
        return VectorReturn::pass();
    case 1:
    case 2:
        return noPrinterControl();
    case 3:
        return VectorReturn::pass();
    case 4:
        return noVDU4Chars();
    case 5:
        return VectorReturn::pass();
    case 6:
        wrch_enable(job);
        return VectorReturn::pass();
    case 7:
        return passControlSeq(initial);
    case 8:
        wrch_backspace(job);
        return VectorReturn::pass();
    case 9:
        wrch_forwardspace(job);
        return VectorReturn::pass();
    case 10:
        wrch_linefeed(job);
        return VectorReturn::pass();
    case 11:
        wrch_lineback(job);
        return VectorReturn::pass();
    case 12:
        return clearBox();
    case 13:
        wrch_carriagereturn(job);
        return VectorReturn::pass();
    case 14:
    case 15:
        return VectorReturn::pass();
    case 16:
        return clearBox();
    case 17:
        return VectorReturn::pass();
    case 18: {
        uint8_t *queue_end = wrch_queue_end(job);
        wrch_setgcol(job, queue_end[-1], queue_end[-2]);
        return VectorReturn::pass();
    }
    case 19:
    case 20:
        return passControlSeq(initial);
    case 21:
        wrch_disable(m_job);
        return VectorReturn::pass();
    case 22:
        return noModeChanges();
    case 23:
        return vdu23();
    case 24:
        return setClipBox();
    case 25:
        return plot();
    case 26:
        return wrch_defaultbox(m_job);
    case 27:
    case 28:
        return VectorReturn::pass();
    case 29: {
        uint32_t word = wrch_queue_word(job, 4);
        job.origin = Offset<OS::Unit>(int16_t(word & 0xFFFFu), int16_t(word >> 16));
        return VectorReturn::pass();
    }
    case 30:
        wrch_tabcursor_common(job, 0, 0);
        return VectorReturn::pass();
    case 31: {
        uint8_t *queue_end = wrch_queue_end(job);
        uint8_t tabX = queue_end[-2];
        uint8_t tabY = queue_end[-1];
        wrch_tabcursor_common(job, tabX, tabY);
        return VectorReturn::pass();
    }
    case 32:
        return deleteChar();
    default:
        return VectorReturn::pass();
    }
}

MyError wrch_defaultbox(CoreJobWS& job)
{
    // Set origin and cursors to (0,0)
    job.origin      = Offset<OS::Unit>();
    job.thispoint   = Point<OS::Unit>();
    job.oldpoint    = Point<OS::Unit>();
    job.olderpoint  = Point<OS::Unit>();
    job.oldestpoint = Point<OS::Unit>();

    // Set 'graphicswindow' from user's box.
    Rect<OS::Unit> clip(job.usersoffset, job.usersbox);

    int32_t xShift = (int32_t)job.screenVars.currxeig;
    int32_t yShift = (int32_t)job.screenVars.curryeig;

    // Adjust to pixel boundaries to ensure clip boxes have the tiling
    // properties an application might expect.
    clip.x0.value = (clip.x0.value >> xShift) << xShift;
    clip.y0.value = (clip.y0.value >> yShift) << yShift;
    clip.x1.value = (clip.x1.value >> xShift) << xShift;
    clip.y1.value = (clip.y1.value >> yShift) << yShift;

    // Record clip box for benefit of graphics home position.
    job.graphicswindow = clip;

    return pagebox_setmaxbox(job);
}

MyError wrch_selectsprite(const Sprite::Selector& sprite, CoreJobWS& job)
{
    (void)sprite;
#if DoFontSpriteVdu
    /* TODO: still to be implemented. */
    return nullptr;
#else
    CoreWS& ws(CoreWS::instance());

    return ws.messages.lookupError(ErrorBlock_PrintCantFontSpriteVDU);
#endif
}

VectorReturn InterceptWrch::handler(OS::Regs& regs, CoreWS& ws)
{
    // Is this VDU output we're not interested in?
    if (ws.m_interceptMgr.hasPassthrough(Passthrough_Wrch))
        return VectorReturn::pass();

    // intercept but do precisely nothing if it is a counting pass
    if (ws.m_countingPass)
        return VectorReturn::pass();

    // Pick up current job's workspace, check for persistent errors, enable
    // ESCAPEs and pass through recursive OS_WriteC's

    InterceptWrch self(ws, *ws.currentJob(), regs);

    return self.handler();
}

VectorReturn InterceptWrch::handler()
{
    // Continuation of `interceptwrch`, but as a class member.
    VectorReturn ret = m_job.checkPersistentError();
    if (ret.isError())
        return ret;

    ret = m_ws.m_escapeState.enable();
    if (ret.isError())
        return finish(ret);

#if DoFontSpriteVdu
        LDRB    R2,doingfontplot
        CMP     R2,#0                           ;NB clears V
        BNE     interceptwrch_fontplot
#endif

    // Note 'ch' can apparently be > 0xff, which Acorn's printer
    // drivers do not handle.
    uint8_t c = (uint8_t)m_regs[0];

    // Are we queueing VDU characters?
    if (m_job.wrch().queueActive()) {
        // Yes. Add it to the queue, then deal with the complete sequence if it's
        // finished.
        uint8_t pos = m_job.wrch().appendQueued(c);

        if (pos == 0) {
            uint8_t initial = m_job.wrch().initialChar();
            ret = controlSequence(initial);
        }

        return finish(ret);
    }

    // No. Start by separating out control characters and DELETEs.
    if (c < 32) {
        static const uint8_t wrch_lengths[] = {
            0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 1, 2, 5, 0, 0, 1, 9, 8, 5, 0, 0, 4, 4, 0, 2
        };
        uint8_t length = wrch_lengths[c];
        if (length != 0u) {
            m_job.wrch().beginSequence(c, length);
            return finish(VectorReturn::pass());
        }
        ret = controlSequence(c);
        return finish(ret);
    }

    if (c == 127) {
        ret = controlSequence(32);
        return finish(ret);
    }

    /* This is a text character. First set the correct colour. */
    uint32_t rgb = 0;
    if (!gcol_lookupfg(Disabled_None, &rgb, m_job)) {
        wrch_autoadvance(m_job);
        return finish(VectorReturn::pass());
    }

    ret = colour_setrealrgb(rgb, m_job);
    if (ret.isError())
        return finish(ret);

    // Then calculate the top left corner of the character - one screen pixel up
    // from the indicated position, and adjusted by graphics origin and user's
    // box origin.
    int32_t pixel = 1;
    pixel <<= m_job.screenVars.curryeig;

    Point<OS::Unit> pos = m_job.oldpoint;
    pos.y += pixel;
    pos += m_job.origin;
    pos -= m_job.usersoffset;

    ret = vdu5_char(c, pos, m_ws);
    if (ret.isError())
        return finish(ret);

    wrch_autoadvance(m_job);

    return finish(VectorReturn::pass());
}

// cmhg entry point.
int wrch_vector_handler(_kernel_swi_regs* kernelRegs, void* pw)
{
    OS::KernelRegsWrapper regs(kernelRegs);

    return InterceptWrch::handler(regs, CoreWS::instance()).toCMHG();
}
