#ifndef PDRIVERPDF_PRIVATE_H
#define PDRIVERPDF_PRIVATE_H

#include <stdint.h>

#include "Types.h"

#include "RLib/Geometry.h"

class PDriverWS;
class JobWS;
class Output;

using namespace riscos::Geometry;

/* Private subroutines only needed for the PDF printer driver. */

/* Output a string of printable characters as a PostScript string, taking care:
 *   (a) not to produce lines of more than 72 characters.
 *   (b) to output characters outside the range ASCII 32-126 as octal escape
 *       sequences.
 *   (c) to output "(", ")" and "\\" as escape sequences.
 * Entry: str points to first character.
 *        len is number of characters.
 *        handle is the job's file handle.
 *        String is guaranteed not to contain control characters (but may
 *          include DELETEs, which will be output as octal escape sequences).
 */
MyError output_PSstring(const uint8_t* str, uint32_t len,
                                 Output& output);
MyError output_PSstringBackwards(const uint8_t* str, uint32_t len,
                                          Output& output);

/* Ensure that we are using the OS co-ordinate system. */
MyError ensure_OScoords(Output& output, JobWS& job);

/* Ensure that we are using the text co-ordinate system with the correct
 * current font factors.
 */
MyError ensure_textcoords(Output& output, JobWS& job);

/* Output save/restore and update the colour control information accordingly. */
MyError output_save(JobWS& job);
MyError output_gsave(JobWS& job);
MyError output_restore(JobWS& job);
MyError output_grestore(JobWS& job);

/* Send the string pointed to by s and containing len characters, but only
 * outputting characters between min and max (inclusive).
 */
MyError output_stringN(const char *s, int32_t len, uint8_t min, uint8_t max,
                                Output& output);

/* Put an immediate string through OS_GSTrans and send the results. */
MyError output_immgstring(const char *s, Output& output);

/* Put the string pointed to by s through OS_GSTrans and send the results. */
MyError output_gstring(const char *s, Output& output);

/* Output a byte buffer verbatim (binary-safe). */
MyError output_bytes(const uint8_t *data,
                              uint32_t length,
                              Output& output);

/* Output a coordinate pair (x, y) in decimal format. */
MyError output_coordpair(int32_t x, int32_t y, Output& output);

/* Output the RGB value in the top 24 bits of bbGGRR00 in decimal format. */
MyError output_rgbvalue(uint32_t bbGGRR00, Output& output);

/* Read the value of a system variable and print it to the output stream. */
MyError output_variable(const char *name, Output& output);

/* ---- PDF writer helpers ---- */
MyError pdf_begin_document(JobWS& jobWS, const char *title);
MyError pdf_end_document(JobWS& jobWS);
MyError pdf_begin_page(const Rect<PDF::Point>& mediaBox,
                       JobWS& jobWS);
MyError pdf_end_page(JobWS& jobWS);

/* Arithmetic helpers used by PageBox. */
void arith_dpmult(int32_t a, int32_t b, int32_t *lo, int32_t *hi);
void arith_dpdivmod(int32_t dividend_lo, int32_t dividend_hi, uint32_t divisor,
                    int32_t *quotient, uint32_t *remainder);

#endif
