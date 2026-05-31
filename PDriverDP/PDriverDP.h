#ifndef PDRIVERDP_PDRIVERDP_H
#define PDRIVERDP_PDRIVERDP_H

/* PDriverDP settings mirrored from PDriverDP.s */

#ifndef PrinterType
#define PrinterType "bit image"
#endif
#ifndef PrinterNumber
#define PrinterNumber 7
#endif
#ifndef DirSuffix
#define DirSuffix "DP"
#endif

/* Is 1bpp possible on this driver? */
#define MonoBufferOK 1

/* If MonoBufferOK then use job_output_bpp not job_use_1bpp. */
#define NbppBufferOK 1

/* If colours returned by colourto256pixval are 00LLMMCC (TRUE)
 * or scaled to halftone.
 */
#define PDumperColours 1

/* If multiple passes of one strip are allowed. */
#define MultiplePasses 1

/* Handle real page sizes. */
#define RealPageSize 1

/* Whether this is a development version. */
#define BeingDeveloped 0

/* Whether various checks are made that are not necessary in (debugged)
 * final code.
 */
#define DevelopmentChecks 0

/* An assembly time variable to switch between using file names based on
 * "<PDriver$Dir>." (old style) and ones based on "PDriver:" (new style).
 */
#define UsePDriverPath 1

/* This module uses the shared printer driver Messages file only. */
// #define PrivMessages "..."

/* Build flags mirrored from PDriverDP.Macros. */
#define Libra1 1
#define GammaCorrect 0
#define MaxColour 255

/* Include the UpCall_PDumperAction generation code. */
#define MakeUpCallsAtEntry 1
#define MakeUpCallsAtExit 1

#endif
