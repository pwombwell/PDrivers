#ifndef CORE_DEVICE_H
#define CORE_DEVICE_H

#include "Config.h"
#include "OS.h"
#include "PDriverInfo.h"

// should probably be able to forward declare these.
#include "RLib/Geometry.h"
#include "RLibX/Sprite.h"
#include "RLib/Constants/Sprite.h"

class CoreWS;
class CoreJobWS;

using namespace Geometry;

#if CaptureTrace
MyError capturetrace_log(const CoreJobWS& job, const char* format, ...);
#endif

/* Backend-agnostic printer device entry points (contracts from Core/Device.s). */

/*
 * The printer-independent shell handles interception policy and ESCAPE state.
 * These device hooks provide backend-specific behaviour only.
 */

/* Configuration routines. */

/*
 * Initialise printer-specific global state.
 *
 * Must initialise global PDriver_Info/PDriver_PageSize values in ws, plus
 * any backend-owned globals.
 */
MyError configure_init(CoreWS& ws);

/* Finalise printer-specific global state (including freeing allocated RMA). */
void configure_finalise(CoreWS& ws);

/*
 * Vet PDriver_SetInfo parameters.
 *
 * regs mirrors r1-r7 from PDriver_SetInfo.
 */
MyError configure_vetinfo(SetPDriverInfo& info);

/*
 * Produce backend-specific PDriver_CheckFeatures errors.
 *
 * mask     = requested feature mask.
 * value    = (requested value) AND mask.
 * features = (current features) AND mask, and differs from value.
 */
MyError configure_makeerror(PDriverInfo::Features mask,
                            PDriverInfo::Features value,
                            PDriverInfo::Features features,
                            CoreWS& ws);

/* Backend-specific PDriver_SetPrinter handling. */
MyError configure_setprinter(CoreWS& ws);

/* Backend-specific PDriver_SetDriver handling. */
MyError configure_setdriver(const OS::Regs& regs, CoreWS& ws);


/* Job management routines. */

/*
 * Allocate printer-specific per-job workspace.
 *
 * jobWS receives the job workspace pointer (CoreJobWS base).
 */
CoreJobWS* managejob_allocate(FileHandle file, bool illustration, CoreWS& ws);

/*
 * Destroy printer-specific per-job workspace.
 *
 * This must release jobWS using the matching backend allocation scheme.
 */
void managejob_destroy(CoreJobWS* jobWS);

/*
 * Initialise a job.
 *
 * title may be nullptr or a control/top-bit terminated title string.
 * If output is redirected to a sprite, allocate sprite/save-area here.
 */
MyError managejob_init(const char* title, CoreJobWS& coreJob);

/*
 * Suspend a job.
 *
 * Must not produce printer output.
 */
MyError managejob_suspend(CoreJobWS& coreJob);

/*
 * Resume a job.
 *
 * Must not produce printer output.
 */
MyError managejob_resume(CoreJobWS& coreJob);

/*
 * Finalise a job.
 *
 * If output is redirected to a sprite, restore original output destination.
 */
MyError managejob_finalise(CoreJobWS& coreJob);

/*
 * Abort a job.
 *
 * Must not produce printer output.
 */
MyError managejob_abort(CoreJobWS& coreJob);


/* Page and box management routines. */

/*
 * Set up page state and rectangle/clip processing for the next page.
 *
 * copies        = requested copies.
 * page_sequence = sequence number or 0.
 * page_number   = optional page-number string (ASCII 33-126 text).
 */
MyError pagebox_setup(int32_t copies,
                      uint32_t page_sequence,
                      const char *page_number,
                      CoreJobWS& coreJob);

/*
 * Return the next box to print, or indicate end-of-page.
 *
 * copies_out == 0 means no more boxes/copies remain for this page.
 * If copies_out != 0, rect/box outputs are valid.
 *
 * On success this also prepares usersbg/usersoffset/usersbox/userstransform/
 * usersbottomleft state for the returned box.
 */
MyError pagebox_nextbox(uint32_t& copies,
                        uint32_t& rectId,
                        Geometry::Rect<OS::Unit>& box,
                        CoreJobWS& coreJob);

/* Return to the maximum clip box for the current print area. */
MyError pagebox_setmaxbox(CoreJobWS& coreJob);

/* Fill current clip box using current usersbg/background colour. */
MyError pagebox_cleartobg(CoreJobWS& coreJob);

/*
 * Set a new clip box.
 *
 * Backend should clip to intersection(new box, current maximum clip box).
 */
MyError pagebox_setnewbox(const Geometry::Rect<OS::Unit>& box, CoreJobWS& coreJob);

/*
 * Box/coordinate conventions (from Device.s):
 * - User coordinates are relative to the lower-left of the user's box.
 * - Boxes are true mathematical edges:
 *   left/bottom inclusive, right/top exclusive (in pixel terms).
 */


/* Colour handling routines. */

/*
 * Set a real RGB colour.
 *
 * bbGGRR00 is true 8-bit-per-channel colour (&BBGGRR00).
 */
MyError colour_setrealrgb(uint32_t bbGGRR00, CoreJobWS& coreJob);

// printer specific routine to convert an RGB
// combination to as close a "pixel value" as possible
// ("pixel values" are values that can be used in
// sprite translation tables).
// Entry: R0 = &BBGGRR00
//        R11 = job's workspace pointer
// Exit:  R0 = pixel value (1 byte)
uint8_t colour_rgbtopixval(uint32_t bbGGRR00, CoreJobWS& coreJob);

// printer specific routine to convert an RGB
// combination to as close a "wide pixel value" as possible
// Entry: R0 = &BBGGRR00
//  Exit: R0 = pixel value (1,2 or 4 byte)
//        R1 = no. of sig bytes in R0
void colour_rgbtopixvalwide(uint32_t bbGGRR00,
                            uint32_t& value,
                            uint8_t& bytes,
                            CoreJobWS& coreJob);


// VDU/OS_Plot routines.

// Plot routine coordinates:
// - Point<OS::Unit> p0 = new graphics cursor
// - Point<OS::Unit> p1 = old graphics cursor
// - Point<OS::Unit> p2 = older graphics cursor
// All coordinates are relative to lower-left of user's box.
//
// Dotted variants use coreJob's dottedpattern/dottedlength.
// 
// These can be implemented with Common/PlotDraw.cpp, which exposes
// a Draw-like API.
MyError plot_linebothends(const Point<OS::Unit>& p0, // new point.
                          const Point<OS::Unit>& p1, // old point.
                          const Point<OS::Unit>& p2, // older point.
                          CoreJobWS& coreJob);
MyError plot_linestartonly(const Point<OS::Unit>& p0, // new point.
                          const Point<OS::Unit>& p1, // old point.
                          const Point<OS::Unit>& p2, // older point.
                          CoreJobWS& coreJob);
MyError plot_lineendonly(const Point<OS::Unit>& p0, // new point.
                          const Point<OS::Unit>& p1, // old point.
                          const Point<OS::Unit>& p2, // older point.
                          CoreJobWS& coreJob);
MyError plot_linenoends(const Point<OS::Unit>& p0, // new point.
                          const Point<OS::Unit>& p1, // old point.
                          const Point<OS::Unit>& p2, // older point.
                          CoreJobWS& coreJob);
MyError plot_line(const Point<OS::Unit>& p0, // new point.
                          const Point<OS::Unit>& p1, // old point.
                          const Point<OS::Unit>& p2, // older point.
                          CoreJobWS& coreJob);

#if defined(RealDottedLines) && RealDottedLines
MyError plot_dottedbothends(const Point<OS::Unit>& p0, // new point.
                          const Point<OS::Unit>& p1, // old point.
                          const Point<OS::Unit>& p2, // older point.
                          CoreJobWS& coreJob);
MyError plot_dottedstartonly(const Point<OS::Unit>& p0, // new point.
                          const Point<OS::Unit>& p1, // old point.
                          const Point<OS::Unit>& p2, // older point.
                          CoreJobWS& coreJob);
MyError plot_dottedendonly(const Point<OS::Unit>& p0, // new point.
                          const Point<OS::Unit>& p1, // old point.
                          const Point<OS::Unit>& p2, // older point.
                          CoreJobWS& coreJob);
MyError plot_dottednoends(const Point<OS::Unit>& p0, // new point.
                          const Point<OS::Unit>& p1, // old point.
                          const Point<OS::Unit>& p2, // older point.
                          CoreJobWS& coreJob);
MyError plot_dotted(const Point<OS::Unit>& p0, // new point.
                          const Point<OS::Unit>& p1, // old point.
                          const Point<OS::Unit>& p2, // older point.
                          CoreJobWS& coreJob);
#endif

MyError plot_point(const Point<OS::Unit>& p0, // new point.
                          const Point<OS::Unit>& p1, // old point.
                          const Point<OS::Unit>& p2, // older point.
                          CoreJobWS& coreJob);
MyError plot_triangle(const Point<OS::Unit>& p0, // new point.
                          const Point<OS::Unit>& p1, // old point.
                          const Point<OS::Unit>& p2, // older point.
                          CoreJobWS& coreJob);
MyError plot_rectangle(const Point<OS::Unit>& p0, // new point.
                          const Point<OS::Unit>& p1, // old point.
                          const Point<OS::Unit>& p2, // older point.
                          CoreJobWS& coreJob);
MyError plot_parallelogram(const Point<OS::Unit>& p0, // new point.
                          const Point<OS::Unit>& p1, // old point.
                          const Point<OS::Unit>& p2, // older point.
                          CoreJobWS& coreJob);
MyError plot_strokecircle(const Point<OS::Unit>& p0, // new point.
                          const Point<OS::Unit>& p1, // old point.
                          const Point<OS::Unit>& p2, // older point.
                          CoreJobWS& coreJob);
MyError plot_fillcircle(const Point<OS::Unit>& p0, // new point.
                          const Point<OS::Unit>& p1, // old point.
                          const Point<OS::Unit>& p2, // older point.
                          CoreJobWS& coreJob);
MyError plot_strokearc(const Point<OS::Unit>& p0, // new point.
                          const Point<OS::Unit>& p1, // old point.
                          const Point<OS::Unit>& p2, // older point.
                          CoreJobWS& coreJob);
MyError plot_fillchord(const Point<OS::Unit>& p0, // new point.
                          const Point<OS::Unit>& p1, // old point.
                          const Point<OS::Unit>& p2, // older point.
                          CoreJobWS& coreJob);
MyError plot_fillsector(const Point<OS::Unit>& p0, // new point.
                          const Point<OS::Unit>& p1, // old point.
                          const Point<OS::Unit>& p2, // older point.
                          CoreJobWS& coreJob);
MyError plot_strokeellipse(const Point<OS::Unit>& p0, // new point.
                          const Point<OS::Unit>& p1, // old point.
                          const Point<OS::Unit>& p2, // older point.
                          CoreJobWS& coreJob);
MyError plot_fillellipse(const Point<OS::Unit>& p0, // new point.
                          const Point<OS::Unit>& p1, // old point.
                          const Point<OS::Unit>& p2, // older point.
                          CoreJobWS& coreJob);

/* Fill the current clipping box. */
MyError plot_fillclipbox(CoreJobWS& coreJob);


/* Draw handling routines. */

/*
 * Draw path contract:
 * - path points to Draw path data in user coordinates.
 * - matrix may be nullptr for identity.
 * - flatness is in user coordinates.
 * - draw_interior* uses bit 1 of flags for even-odd winding rule.
 *
 * Backends must handle origin/usersoffset adjustment for path coordinates.
 */
MyError draw_boundaryonly(Draw::Path path,
                          const Draw::Matrix* matrix,
                          Draw::Unit flatness,
                          CoreJobWS& coreJob);
MyError draw_interiornobdry(Draw::Path path,
                            uint32_t flags,
                            const Draw::Matrix* matrix,
                            Draw::Unit flatness,
                            CoreJobWS& coreJob);
MyError draw_interior(Draw::Path path,
                      uint32_t flags,
                      const Draw::Matrix* matrix,
                      Draw::Unit flatness,
                      CoreJobWS& coreJob);
MyError draw_interiorwithbdry(Draw::Path path,
                              uint32_t flags,
                              const Draw::Matrix* matrix,
                              Draw::Unit flatness,
                              CoreJobWS& coreJob);
MyError draw_strokenobdry(Draw::Path path,
                          const Draw::Matrix* matrix,
                          Draw::Unit flatness,
                          Draw::Unit thickness,
                          const Draw::CapJoin* capJoin,
                          const Draw::DashPattern* dashPattern,
                          CoreJobWS& coreJob);
MyError draw_stroke(Draw::Path path,
                    const Draw::Matrix* matrix,
                    Draw::Unit flatness,
                    Draw::Unit thickness,
                    const Draw::CapJoin* capJoin,
                    const Draw::DashPattern* dashPattern,
                    CoreJobWS& coreJob);
MyError draw_strokewithbdry(Draw::Path path,
                            const Draw::Matrix* matrix,
                            Draw::Unit flatness,
                            Draw::Unit thickness,
                            const Draw::CapJoin* capJoin,
                            const Draw::DashPattern* dashPattern,
                            CoreJobWS& coreJob);
MyError draw_thinstroke(Draw::Path path,
                        const Draw::Matrix* matrix,
                        Draw::Unit flatness,
                        const Draw::DashPattern* dashPattern,
                        CoreJobWS& coreJob);


/* Sprite handling routines. */

/*
 * sprite_put/sprite_mask:
 * - reason selects selector interpretation (system/user area, name/pointer).
 * - point is in OS units relative to lower-left of current rectangle.
 * - gcolAction. Core passes only relevant bits:
 *   bit 3 = use mask, bit 4 = prefer sprite palette over translation table.
 * - scale points to x-mag,y-mag,x-div,y-div or nullptr for no scaling.
 * - table points to translation table or nullptr for none.
 *
 * Backend should preserve tiling behaviour of adjacent sprites as described
 * in Core/Device.s.
 */
struct SpritePlotBlock {
    const uint32_t              reason;
    const Sprite::Selector&     sprite;
    const Sprite::TransformFlag flags;
    Sprite::PlotAction          plotAction;
    const ColourTrans::Table*   colTable;   // Not used for mask plots.

    SpritePlotBlock(uint32_t reason,
                    const Sprite::Selector& sprite,
                    Sprite::TransformFlag flags,
                    Sprite::PlotAction plotAction,
                    const ColourTrans::Table* colTable = nullptr)
        : reason(reason)
        , sprite(sprite)
        , flags(flags)
        , plotAction(plotAction)
        , colTable(intptr_t(colTable) == -1 ? nullptr : colTable)
    {}
};

MyError sprite_put(SpritePlotBlock& block,
                   const Point<OS::Unit>& point,
                   const Sprite::ScaleFactor* scale,
                   CoreJobWS& coreJob);

MyError sprite_mask(SpritePlotBlock& block,
                    const Point<OS::Unit>& point,
                    const Sprite::ScaleFactor* scale,
                    CoreJobWS& coreJob);

/* Transformed sprite handling routines. */

/*
 * Transformed sprite contract:
 * - the overload selects Draw-style matrix or destination coordinates.
 * - flags bit 1 set: sourceRect points to {lowx,lowy,highx,highy}
 *   (low inclusive, high exclusive).
 * - table points to translation table or nullptr for none.
 *
 * Destination-coordinates mode (parallelogram) maps source corners as:
 *   x0,y1 -> X0,Y0
 *   x1,y1 -> X1,Y1
 *   x1,y0 -> X2,Y2
 *   x0,y0 -> X3,Y3
 */
struct TransformedSprite : public SpritePlotBlock {
    const Sprite::PixelRect*    sourceRect; // null if flag not set.

    // Only access one of these, depending on `type`.
    //
    // Whichever one is valid has been offset by the current job's origin/usersoffset.
    Draw::Matrix                matrix;     // if `type == TransformType_Matrix`
    Sprite::Coords              coords;     // if `type == TransformType_Coords`

    bool isMaskPlot() const { return reason == SpriteReason_PlotMaskTransformed; }
    bool isParallelogram() const { return !!(flags & Sprite::TransformFlag_Parallelogram); }
    bool isMatrix() const { return !isParallelogram(); }

    TransformedSprite(uint32_t reason,
                      const Sprite::Selector& sprite,
                      Sprite::TransformFlag flags,
                      Sprite::PlotAction plotAction,
                      const ColourTrans::Table* colTable)
        : SpritePlotBlock(reason, sprite, flags, plotAction, colTable)
        , sourceRect(nullptr)
    {}
};

// Recipients may modify info. It is not accessed on return.
MyError sprite_plottransformed(TransformedSprite& block, CoreJobWS& coreJob);

/* VDU 5 character handling routines. */

/*
 * Handle a VDU 5 character.
 *
 * x,y are the top-left character position in OS units, relative to lower-left
 * of user's box. (y is one screen-pixel row above the graphics cursor.)
 */
MyError vdu5_char(uint8_t ch, Point<OS::Unit> p, CoreWS& ws);

/*
 * Handle VDU 5 DELETE (solid block in background colour at x,y).
 */
MyError vdu5_delete(Point<OS::Unit> p, CoreWS& ws);

/*
 * Flush buffered VDU 5 output.
 *
 * Called before likely output, before VDU controls, and before rectangle
 * boundaries are finalised.
 */
MyError vdu5_flush(CoreJobWS& coreJob);

/*
 * Notify that VDU 5 glyph definitions changed.
 *
 * Low-byte-only code means one character; other values may invalidate all.
 */
void vdu5_changed(CoreJobWS& coreJob, uint32_t code);


/* Font handling routines. */

/* Set background colour using GCOL. */
MyError font_bg(uint32_t gcol, CoreJobWS& coreJob);

/* Set foreground colour using GCOL. */
MyError font_fg(uint32_t gcol, CoreJobWS& coreJob);

/* Set background colour using absolute RGB (&BBGGRR00 style). */
MyError font_absbg(uint32_t bbGGRR00, CoreJobWS& coreJob);

/* Set foreground colour using absolute RGB (&BBGGRR00 style). */
MyError font_absfg(uint32_t bbGGRR00, CoreJobWS& coreJob);

/*
 * Set font colour offset.
 *
 * With GCOL foreground: add offset modulo 16 to first foreground GCOL.
 * With absolute RGB foreground: offset is a hint for intermediates; sign
 * should be ignored.
 */
MyError font_coloffset(int32_t offset, CoreJobWS& coreJob);

/* Save current font colour state for multi-pass painting. */
MyError font_savecolours(CoreJobWS& coreJob);

/*
 * Prepare for Font_Paint sequence.
 *
 * max_chars_minus1 receives chunk-size limit minus one.
 * passes receives pass count (>= 1).
 */
MyError font_stringstart(uint32_t *max_chars_minus1,
                         uint32_t *passes,
                         CoreJobWS& coreJob);


/* Start one painting pass (countdown pass number). */
MyError font_passstart(uint32_t pass, CoreJobWS& coreJob);

/*
 * Paint one printable chunk.
 *
 * chunk points to printable characters only; position is the paint position in
 * millipoints relative to lower-left of user's box.
 */
MyError font_paintchunk(const uint8_t *chunk,
                        const Geometry::Point<OS::Millipoint>& position,
                        uint32_t length,
                        uint32_t pass,
                        CoreJobWS& coreJob);

/* End one painting pass (countdown pass number). */
MyError font_passend(uint32_t pass, CoreJobWS& coreJob);

/* Finish Font_Paint sequence. */
MyError font_stringend(CoreJobWS& coreJob);

/* Lose slaved font(s).
 *
 * handle == 0 means forget all tracked slave fonts for this job.
 */
MyError font_losefont(FontHandle handle, CoreJobWS& coreJob);

/* Soft-reset notification: all fonts were lost; clear any master/slave cache. */
void font_fontslost(CoreJobWS& coreJob);

// Declare a document font; flags: bit0 fail on download failure, bit1 kerned.
MyError font_declare(const char* name, uint32_t flags, CoreJobWS& coreJob);


/* JPEG routines. */

/* Called when JPEG ctrans handle is requested. */
MyError jpeg_ctrans_handle(ColourTrans::Table32K& outTableDescriptor,
                           CoreJobWS& coreJob);

/*
 * Handle intercepted JPEG_PlotScaled calls.
 *
 *   R0 = pointer to JPEG loaded in memory
 *   R1 = x coordinate for plot
 *   R2 = y coordinate for plot
 *   R3 = pointer to scale factors or 0
 *   R4 = length of data in memory (bytes)
 *   R5 = Flags (ignored by this routine)
 *         b0 set: dither output when plotting 24 bit JPEG at 8bpp or below
 */
MyError jpeg_plotscaled(const uint8_t* jpeg, uint32_t len,
                        uint32_t flags,
                        OS::Unit x, OS::Unit y,
                        const Sprite::ScaleFactor* scale,
                        CoreJobWS& coreJob);

/*
 * Handle intercepted JPEG_PlotTransformed calls.
 *
 * The overload selects Draw-style matrix or destination coordinates.
 */
MyError jpeg_plottransformed(const uint8_t *jpeg, uint32_t length,
                             const Draw::Matrix& matrix, CoreJobWS& coreJob);

MyError jpeg_plottransformed(const uint8_t *jpeg, uint32_t length,
                             const Sprite::Coords& coords, CoreJobWS& coreJob);

/* Screen dump handling. */

/* Dump current screen output to file handle. */
MyError screendump_dump(int32_t handle);


/* Picture insertion. */

/*
 * Insert an illustration with destination corner coordinates.
 *
 * handle is the file handle for the illustration source.
 * path points to optional Draw clipping path data (nullptr for none).
 * Destination points are bottom-left, bottom-right, top-left.
 */
MyError picture_insert(FileHandle handle,
                       const Draw::Path* clipPath,
                       const Point<OS::Unit>& bottomLeft,
                       const Point<OS::Unit>& bottomRight,
                       const Point<OS::Unit>& topLeft);

/* Device-specific MiscOp handler. */

/*
 * Decode backend-private MiscOp reasons (bit 31 set in original SWI reason).
 *
 */
MyError miscop_decode(uint32_t reason, OS::Regs& args, CoreWS& ws);

#endif
