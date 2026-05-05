#ifndef CORE_CONFIG_H
#define CORE_CONFIG_H

/* Core.PDriver -> C
 * This header captures the core assembly-time flags from Core/PDriver.s.
 * It assumes printer-specific headers define PrinterType, PrinterNumber,
 * VersionNumber, DirSuffix, and BeingDeveloped. Defaults are provided
 * for standalone use.
 */

#define UNUSED(X) (void)(X);

#ifndef PrinterType
#define PrinterType ""
#endif

#ifndef PrinterNumber
#define PrinterNumber 0
#endif

#ifndef VersionNumber
#define VersionNumber 400
#endif

#ifndef DirSuffix
#define DirSuffix ""
#endif

#ifndef BeingDeveloped
#define BeingDeveloped 0
#endif

/* Set this to build in support for Medusa screen modes & sprites. The module
 * thus formed should run on non-Medusa platforms, but only to the extent that
 * the rest of the OS allows (e.g. no truecolour support).
 */
#define Medusa ((VersionNumber) >= 400)

/* Set this to build in support for UCS text strings. */
#define UCSText 1

/* The following conditional assembly flags are to do with whether various
 * features of the full printer driver spec are implemented.
 * N.B. At present these are here mainly for documentation purposes, to
 * describe the plotting calls that could be implemented but haven't been.
 * I do not recommend changing them.
 */

/* VDU dotted lines will be plotted solid. */
#define RealDottedLines 0

/* Font and sprite VDU 23 and 25 sequences not done. */
#define DoFontSpriteVdu 0

/* OS_SpriteOp with reason PaintCharScaled not done. */
#define DoPaintCharSc 0

/* Whether various bug fixes to do with font colours have been applied.
 * (30-08-89: these bug found and fixed.)
 */
#define FontColourFixes 1

/* Bug fix - bitmap printers used to set the background colour and then
 * call Draw to fill background rectangles, which was intercepted and
 * set the colour to the foreground before plotting. This is avoided by
 * using a reserved bit in the draw fill style (bit 16). These switches
 * allow the use of this new bit.
 */
#define PrinterDrawBit 1

/* Whether various errors are returned on vector interception; this flag,
 * when True, allows errors to be returned on ColourV, SpriteV and DrawV.
 * These were removed to make compatiblity with future versions of the OS
 * possible without having to re-issue versions of the printer drivers.
 */
#define VectorErrors 0

/* Apply bug fix for font handles on ColourTrans_SetFontColours ensures
 * that the handle is valid.
 */
#define FontHandleEnsure 1

/* Capture-only diagnostic trace records. These are intended for PDriverCapture
 * and are written into the capture stream for offline comparison; replay skips
 * them as unknown records.
 */
#ifndef CaptureTrace
#define CaptureTrace 0
#endif

/* Host Debug stuff; moved into the Core section so that the Core routines
 * can be debugged.
 */

/* Setting the 'debug' variable to FALSE will turn off all NDR debugging. */
#define debug (BeingDeveloped)
#define hostvdu 0
#define debug_flush 0
#define debug_file "<PDrvDebug>"

/* NDRDebug ensures that the following only take effect if debug is TRUE. */
#define debugCoreFont 0
#define debugMedusa (Medusa && 0)

#endif
