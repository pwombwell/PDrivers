#ifndef CORE_PDRIVER_H
#define CORE_PDRIVER_H

#include "Config.h"
#include "ErrorBlocks.h"

#include "RLib/RLib.h"
#include "RLib/Geometry.h"
#include "RLib/OS.h"
#include "RLib/Draw.h"
#include "RLibX/OS.h"

class CoreJobWS;
class PDriverInfo;
struct PDriverPageSize;
struct SetPDriverInfo;

using namespace riscos::Geometry;

// Entry points for redirected PDriver SWIs.
// FIXME: Why not a namespace?
class PDriver {
public:
    //  SWI PDriver_Info implementation
    //
    //    Entry: -
    //
    //    Exit:  R0 (top 16 bits) identifies general type of printer driven.
    //           R0 (bottom 16 bits) = 100 * version number of printer driver.
    //           R1 = X pixel resolution of printer driven (pixels/inch).
    //           R2 = Y pixel resolution of printer driven (pixels/inch).
    //           R3 = features word:
    //                Bit 0 set => colour
    //                If bit 0 set, bit 1 set => full colour range not available.
    //                Bit 2 set => only a discrete set of colours supported.
    //                Bit 8 set => cannot handle filled shapes well.
    //                Bit 9 set => cannot handle thick lines well.
    //                Bit 10 set => cannot handle overwriting well.
    //                Bit 11 set => can handle transformed sprite plotting.
    //                Bit 12 set => can handle new Font Manager features (transforms, encodings etc..).
    //                Bit 13 set => can handle new DrawPage features (flag byte in R0)
    //                Bit 24 set => supports the PDriver_ScreenDump call.
    //                Bit 25 set => supports arbitrary transformations (else only
    //                              axis-preserving ones).
    //                Bit 26 set => supports the PDriver_InsertIllustration call.
    //                Bit 27 set => supports the PDriver_MiscOp call.
    //                Bit 28 set => supports the PDriver_SetDevice call.
    //                Bit 29 set => supports the PDriver_DeclareFont call.
    //
    //           R4 -> adjectival description of printers supported (a maximum of
    //                20 characters ???, excluding the zero-termination).
    //           R5 = X halftone resolution (repeats/inch). If no halftoning is
    //                done, this is equal to the value returned in R1.
    //           R6 = Y halftone resolution (repeats/inch). If no halftoning is
    //                done, this is equal to the value returned in R2.
    //           R7 identifies a specific configured printer and is zero unless it
    //              has been changed via PDriver_SetInfo.
    //
    //  Some of these values can be changed by the PDriver_SetInfo call. If
    //  PDriver_Info is called while a print job is selected, the values returned
    //  are those that were in effect when that print job was started (i.e. when
    //  it was first selected using PDriver_SelectJob). If PDriver_Info is called
    //  when no print job is active, the values returned are the current ones.
    //
    //  Initially, these values should be correct for some standard printer within
    //  the class of printers supported by the printer driver.
    static const PDriverInfo& info(uint32_t& version);

    //  SWI PDriver_SetInfo implementation
    //
    //    Entry: R1 = X pixel resolution of printer driven (pixels/inch).
    //           R2 = Y pixel resolution of printer driven (pixels/inch).
    //           R3 (bit 0) is zero to set monochrome, 1 to set colour. The other
    //                bits of R3 are currently ignored.
    //           R4 = pointer to printer name
    //           R5 = X halftone resolution (repeats/inch). If no halftoning is
    //                to be done, this should be equal to the value in R1.
    //           R6 = Y halftone resolution (repeats/inch). If no halftoning is
    //                to be done, this should be equal to the value in R2.
    //           R7 identifies a specific configured printer.
    //
    //    Exit:  -
    //
    //  This call should be used to reconfigure the printer driver for a specific
    //  printer within the general class of printers it supports (e.g. an Apple
    //  LaserWriter within the class of PostScript printers). Note that it only
    //  affects print jobs started after it is called - existing print jobs use
    //  whatever values were in effect when they were started.
    static MyError setInfo(SetPDriverInfo& info);

    // SWI PDriver_CheckFeatures implementation
    //
    //   Entry: R0 = features word mask.
    //          R1 = features word value.
    //
    //   Exit:  If the features word that PDriver_Info would return in R3
    //          satisfies ((features word) AND R0) = (R1 AND R0), return is
    //          normal with all registers preserved. Otherwise a suitable error
    //          is generated, if appropriate - e.g. no error will be generated
    //          if the printer driver has the ability to support arbitrary
    //          rotations and your features word value merely requests axis-
    //          preserving ones.
    static MyError checkFeatures(OS::Regs& regs);

    static const PDriverPageSize& pageSize();
    static void setPageSize(const PDriverPageSize& pageSize);

    // SWI PDriver_SelectJob implementation
    //
    //   Entry: R0 = file handle for print job to be selected, or zero to cease
    //               having any print job selected.
    //          R1 is zero or points to a title string for the job. This title
    //               string is terminated by any character outside the range
    //               ASCII 32-126.
    //
    //   Exit:  R0 = file handle for print job that was previously active, or zero
    //               if no print job was active.
    //
    // A print job is identified by a file handle, which must be that of a file
    // that is open for output. The printer output for the job concerned is sent
    // to this file.
    //
    // Calling PDriver_SelectJob with R0=0 will cause the current print job (if
    // any) to be suspended, and the printer driver will cease intercepting any
    // plotting calls.
    //   Calling PDriver_SelectJob with R0 containing a file handle will cause the
    // current print job (if any) to be suspended, and a print job with the given
    // file handle to be selected. If a print job with this file handle already
    // exists, it is resumed; otherwise a new print job is started. The printer
    // driver will start to intercept plotting calls if it is not already doing
    // so.
    //   Note that this call never ends a print job - to do so, use one of the
    // calls PDriver_EndJob and PDriver_AbortJob.
    //
    // The title string pointed to by R1 is treated by different printer drivers
    // in different ways. It is only ever used if a new print job is being
    // started, not when an old one is being resumed. Typical uses are:
    //   (a) A simple printer driver might ignore it.
    //   (b) The PostScript printer driver adds a line "%%Title: " followed by the
    //       given title string to the generated PostScript header.
    //   (c) Printer drivers whose output is destined for an expensive central
    //       printer in a large organisation might use it when generating a cover
    //       sheet for the document.
    //
    // Printer drivers may also use the following OS variables when creating
    // cover sheets, etc.:
    //   PDriver$For     - indicates who the output is intended to go to
    //   PDriver$Address - indicates where to send the output.
    // These variables should not contain characters outside the range ASCII
    // 32-126.
    static MyError selectJob(FileHandle fileHandle, const char* title,
                             FileHandle& previousHandle);

    // SWI PDriver_CurrentJob implementation
    //
    //   Entry: -
    //
    //   Exit:  R0 = file handle for print job that is currently active, or zero
    //               if no print job is active.
    static FileHandle currentJob();

    static MyError fontSWI(OS::Regs& regs);













    // SWI PDriver_EndJob implementation
    //
    //   Entry: R0 = file handle for print job to be ended.
    //
    //   Exit:  -
    //
    // This call should be used to end a print job normally. This may result in
    // further printer output - e.g. the PostScript printer driver will produce
    // the standard trailer comments.
    //   If the print job being ended is the currently active one, there will be
    // no current print job after this call (so plotting calls will not be
    // intercepted).
    //   If the print job being ended is not currently active, it will be ended
    // without altering which print job is currently active or whether plotting
    // calls are being intercepted.
    static MyError endJob(FileHandle fileHandle);

    // SWI PDriver_AbortJob implementation
    //
    //   Entry: R0 = file handle for print job to be aborted.
    //
    //   Exit:  -
    //
    // This call should be used to end a print job abnormally - it is generally
    // called after fatal errors while printing. In general, it will not try to
    // produce any further printer output; if it does do so, it will take care to
    // handle possible further errors (particularly filing system errors) itself.
    //   If the print job being aborted is the currently active one, there will be
    // no current print job after this call (so plotting calls will not be
    // intercepted).
    //   If the print job being aborted is not currently active, it will be
    // aborted without altering which print job is currently active or whether
    // plotting calls are being intercepted.
    static MyError abortJob(FileHandle fileHandle);

    // SWI PDriver_Reset implementation
    //
    //   Entry: -
    //
    //   Exit:  -
    //
    // This aborts all print jobs known to the printer driver, leaving the printer
    // driver with no active or suspended print jobs and ensuring that plotting
    // calls are not being intercepted.
    //   Normal applications don't make this call, but it can be useful as an
    // emergency recovery measure!
    static MyError reset();

    // SWI PDriver_GiveRectangle implementation
    //
    //   Entry: R0 = rectangle identification word. This word is reported back to
    //               the application when it is requested to plot all or part of
    //               this rectangle.
    //          R1 -> 4 word block, containing rectangle to be plotted. Units are
    //               OS units.
    //          R2 -> 4 word block, containing dimensionless transformation to be
    //               applied to the specified rectangle before printing it. The
    //               entries are given as fixed point numbers with 16 binary
    //               places, so the transformation is:
    //                 x' = (x * R2!0 + y * R2!8)/2^16
    //                 y' = (x * R2!4 + y * R2!12)/2^16
    //          R3 -> 2 word block, containing the position where the bottom left
    //               corner of the rectangle is to be plotted on the printed page.
    //               Units are 1/72000 inch.
    //          R4 = background colour for rectangle, in form &BBGGRRXX.
    //
    //   Exit:  -
    //
    // This SWI allows an application to specify a rectangle from its workspace to
    // be printed, how it is to be transformed and where it is to appear on the
    // printed page. An application should make one or more calls to SWI
    // PDriver_GiveRectangle before a call to SWI PDriver_DrawPage and the
    // subsequent calls to SWI PDriver_GetRectangle. (Multiple calls allow the
    // application to print multiple rectangles from its workspace to one printed
    // page - e.g. for "two up" printing).
    //   The printer driver may subsequently ask the application to plot the
    // specified rectangles or parts thereof in any order that is compatible with
    // the order in which the PDriver_GiveRectangle calls were issued. An
    // application should not make any assumptions about this order or whether the
    // rectangles it specifies will be split. (A common reason why a printer
    // driver might split a rectangle is that it prints the page in strips to
    // avoid using excessively large page buffers.)
    //   Assuming that a non-zero number of copies is requested and that none of
    // the requested rectangles go outside the area available for printing, it is
    // certain to ask the application to plot everything requested at least once.
    // It may ask for some areas to be plotted more than once, even if only one
    // copy is being printed, and it may ask for areas marginally outside the
    // requested rectangles to be plotted (this can typically happen if the
    // boundaries of the requested rectangles are not on exact device pixel
    // boundaries).
    //   If SWI PDriver_GiveRectangle is used to specify a set of rectangles that
    // overlap on the output page, the rectangles will be printed in the order of
    // the SWI PDriver_GiveRectangle calls. For appropriate printers (i.e. most
    // printers, but not e.g. XY plotters), this means that rectangles supplied
    // via later PDriver_GiveRectangle calls will overwrite rectangles supplied
    // via earlier calls.
    static MyError giveRectangle(uint32_t id,
                                 const Rect<OS::Unit>& box,
                                 const Draw::DimensionlessTransform& transform,
                                 const Point<OS::Millipoint>& bottomLeft,
                                 uint32_t bgColour);

    // SWI PDriver_DrawPage implementation
    //
    //   Entry: R0 bits 0..23  = number of copies to print.
    //          R0 bits 24..31 = flags
    //               bit 24 = 1 if application can deal with pre-scan of rectangles, 0 if not
    //               bits 25..31 reserved (set to 0)
    //          R1 -> 4 word block, to receive the rectangle to print.
    //          R2 is zero or contains the page's sequence number within the
    //               document being printed (i.e. 1-n for an n-page document).
    //          R3 is zero or points to a string, terminated by a character in
    //               the ASCII range 33-126 (note spaces are not allowed), which
    //               gives the 'real' page number. (Examples: "23", "viii", "A-1")
    //
    //   Exit:  R0 is zero if and only if no more plotting is required.
    //          else number of copies still requiring printing (and bit 24 set if 'counting pass')
    //          If R0 is non-zero, the area pointed to by R1 has been filled in
    //               with the rectangle that needs to be plotted, and R2 contains
    //               the rectangle identification word for the user-specified
    //               rectangle that this is a part of.
    //          If R0 is zero, the contents of R2 and the area pointed to by R1
    //               are undefined.
    //
    // If the pre-scan flag is set on entry, the drivers may choose to pre-scan all
    // previously given rectangles, before the main printing pass. This is intended
    // to allow the printing requirements to be assessed before the main printing task
    // begins. Currently, this feature is used to calculate memory requirements for
    // JPEG printing, but may be used for other items in the future. The pre-scan only
    // occurs if the flag is set on entry, so that there is no risk of confusing old
    // applications. During the pre-scan, the application can plot as normal, so the
    // new feature is relatively transparent (there may be minor considerations, such
    // as calculations of how much of print job is complete needing changes).
    //
    // The information passed in R2 and R3 is not particularly important, though
    // it helps to make output produced by the PostScript printer driver conform
    // better to Adobe's structuring conventions. If non-zero values are supplied,
    // they should be correct. Note that R2 is NOT the sequence number of the page
    // in the print job, but in the document.
    //   An example: if a document consists of 11 pages, numbered "" (the title
    // page), "i"-"iii" and "1"-"7", and the application is requested to print the
    // entire preface part, it should use R2 = 2, 3, 4 and R3 -> "i", "ii", "iii"
    // for the three pages.
    static MyError drawPage(uint32_t& copiesRemaining,
                            Rect<OS::Unit>& rect,
                            uint32_t sequenceNumber,
                            const char* realPageNumber,
                            uint32_t& rectId);

    // SWI PDriver_GetRectangle implementation
    //
    //   Entry: R1 -> 4 word block, to receive the rectangle to print.
    //
    //   Exit:  R0 = number of copies still requiring printing (bit 24 set if 'counting pass' rectangle)
    //               Or, this is zero if and only if no more plotting is required.
    //          If R0 is non-zero, the area pointed to by R1 has been filled in
    //               with the rectangle that needs to be plotted, and R2 contains
    //               the rectangle identification word for the user-specified
    //               rectangle that this is a part of.
    //          If R0 is zero, the contents of R2 and the area pointed to by R1
    //               are undefined.
    static MyError getRectangle(Rect<OS::Unit>& rect,
                                uint32_t& copiesRemaining,
                                uint32_t& rectId);
    
    static MyError cancelJob(OS::Regs& regs);
    static MyError screenDump(OS::Regs& regs);

    // SWI PDriver_EnumerateJobs implementation
    //
    //   Entry: R0 = 0 or file handle of previous print job.
    //
    //   Exit:  R0 = file handle of next print job or 0
    static MyError enumerateJobs(FileHandle& prevJobHandle,
                                 CoreJobWS*& nextJob);

    static MyError setPrinter(OS::Regs& regs);

    // SWI PDriver_CancelJobWithError implementation
    //
    //   Entry: R0 = file handle for job to be cancelled.
    //          R1 points to an error block.
    //
    //   Exit:  -
    //
    // This call causes subsequent attempts to output to the print job associated
    // with the given file handle to do nothing other than generate the specified
    // error. An application is expected to respond to that future error by
    // aborting the print job. This does not return nullptr, unless the job
    // passed in on entry is unknown to the printer driver.
    static MyError cancelJobWithError(FileHandle toCancel, MyError withError);

    // SWI PDriver_SelectIllustration implementation
    //
    //   Entry: R0 = file handle for print job to be selected, or zero to cease
    //               having any print job selected.
    //          R1 is zero or points to a title string for the job. This title
    //               string is terminated by any character outside the range
    //               ASCII 32-126.
    //
    //   Exit:  R0 = file handle for print job that was previously active, or zero
    //               if no print job was active.
    //
    // This SWI is exactly like SWI PDriver_SelectJob below, except that if it
    // starts a new job, the job it starts is forced to contain exactly one page
    // by the printer-independent code, and the printer-dependent code may treat
    // it differently. For instance, the PostScript printer driver takes advantage
    // of the guarantee of precisely one page to make the output into an
    // Encapsulated PostScript file.
    //   Any particular printer-specific code is quite at liberty to treat this
    // call identically with PDriver_SelectJob.
    static MyError selectIllustration(FileHandle handle,
                                      const char* title,
                                      FileHandle& previous);

    //   Each type of printer needs its own illustration insertion subroutine.
    // This routine should be implemented in the following file. Note that this
    // file is required even if the printer driver does not support illustration
    // insertion: in this case, it should just generate an error.
    //   This file needs to implement the following routines:
    //
    // picture_insert  - printer specific code to insert an illustration file.
    //                   Entry: R0 = file handle for file containing
    //                             illustration. Should be open for input.
    //                          R1 points to Draw module path to be used as a
    //                             clipping path, or contains zero if no clipping
    //                             is required.
    //                          R2,R3 contain the co-ordinates of where the
    //                             bottom left corner of the illustration is to
    //                             go.
    //                          R4,R5 contain the co-ordinates of where the
    //                             bottom right corner of the illustration is to
    //                             go.
    //                          R6,R7 contain the co-ordinates of where the top
    //                             left corner of the illustration is to go.
    //                   Exit:  All registers preserved (except R0 on an error).

    static MyError insertIllustration(FileHandle handle,
                                      const Draw::Path* clipPath,
                                      const Point<OS::Unit>& bottomLeft,
                                      const Point<OS::Unit>& bottomRight,
                                      const Point<OS::Unit>& topLeft);

    static MyError declareFont(uint32_t fontHandle, const char* name,
                               uint32_t flags);

    static MyError badSWI(OS::Regs& regs);
    static MyError miscOp(OS::Regs& regs);
    static MyError setDriver(const OS::Regs& regs);
    static MyError JPEGSWI(OS::Regs& regs);
};

#endif
