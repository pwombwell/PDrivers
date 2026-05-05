#include "Core/Device.h"

/*
 * Core.Device
 *
 * Printer dependent subroutines.
 *
 * This C file mirrors Core/Device.s. The actual implementations live in the
 * backend-specific modules (for example under PDriverPS/ or PDriverDP/), with
 * the shared, backend-agnostic entry point declarations in Core/Device.h.
 *
 * General comments about interceptions:
 *
 * The printer driver always intercepts the sprite vector to keep track of
 * where plotting output is currently supposed to be directed. (It assumes
 * that output is directed to the screen when it is initialised, and tracks
 * the current state from then on.) Printer specific code should not try to
 * do anything special with this interception.
 *
 * When inside a print job, it is also usually intercepting WrchV, ColourV,
 * DrawV and SpriteV (a second time), and is being told about various Font
 * Manager calls via the PDriver_FontSWI interface. There are four standard
 * exceptions to this rule:
 * a) While the Wimp error handler is in use (tracked via Service_WimpReportError),
 *    all of these cease to be intercepted.
 * b) While output is redirected to a sprite and that sprite is not the one
 *    registered as belonging to the current print job (via the jobspriteparams
 *    variables in the job's workspace), all of these cease to be intercepted.
 * c) When inside any routine that may produce output, all OS_Wrch calls are
 *    intercepted, but then immediately passed through to the previous owner
 *    of the vector (by setting the passthrough_wrch bit of the passthrough byte).
 *    This is a precaution against accidental recursive invocation of the
 *    printer driver if printing to the "vdu:" or "rawvdu:" devices. This applies
 *    to all the calls described as having R10 equal to the print job's file
 *    handle on entry.
 * d) While processing Font_Paint, the Font Manager is instructed to stop
 *    calling PDriver_FontSWI. This affects entry to the font_xxxx routines.
 *
 * The printer specific code may wish to stop interception of one or more of
 * these calls. There are two main techniques:
 * - Set one or more of passthrough_wrch, passthrough_col, passthrough_draw,
 *   passthrough_spr in the global passthrough byte. This avoids claiming the
 *   call, but cannot stop PDriver_FontSWI and may be less efficient if many
 *   calls are made.
 * - Modify the shouldintercept byte (intercept_wrch/intercept_col/intercept_draw/
 *   intercept_spr/intercept_font) and call changeintercept with R3 set.
 *   changeintercept preserves registers/flags, updates shouldintercept, and
 *   adjusts interceptions as required.
 *
 * A note about ESCAPEs:
 *
 * The printer-independent shell handles ESCAPE enabling/acknowledgement and
 * generates the Escape error. The printer-dependent code relies on the filing
 * system to check for ESCAPEs during I/O, but long-running operations should
 * poll OS_ReadEscapeState and return with V set if an ESCAPE occurs.
 * This should only be done in routines protected by top-level ESCAPE handling
 * (i.e. those entered with R10 equal to the job's file handle).
 */
