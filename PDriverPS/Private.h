#ifndef PDRIVERPS_PRIVATE_H
#define PDRIVERPS_PRIVATE_H

#include "RLib/Draw.h"
#include "RLib/Geometry.h"
#include "RLibX/OS.h"

/* Private subroutines only needed for the PostScript printer driver. */

class PDriverWS;
class JobWS;
class Output;

using namespace riscos::Geometry;

/* Ensure that we are using the OS co-ordinate system. */
MyError ensure_OScoords(Output& output, JobWS& jobWS);

/* Ensure that we are using the text co-ordinate system with the correct
 * current font factors.
 */
MyError ensure_textcoords(Output& output, JobWS& jobWS);

/* Output save/restore and update the colour control information accordingly. */
MyError output_save(JobWS& jobWS);
MyError output_gsave(JobWS& jobWS);
MyError output_restore(JobWS& jobWS);
MyError output_grestore(JobWS& jobWS);

/* Send the string pointed to by s and terminated by any character not
 * between min and max (inclusive).
 */
MyError output_string(const char* s, uint8_t min, uint8_t max, Output& output);

/* Send the string pointed to by s and containing len characters, but only
 * outputting characters between min and max (inclusive).
 */
MyError output_stringN(const char* s, int32_t len, uint8_t min, uint8_t max,
                       Output& output);

/* Put an immediate string through OS_GSTrans and send the results. */
MyError output_immgstring(const char* s, Output& output);

/* Put the string pointed to by s through OS_GSTrans and send the results. */
MyError output_gstring(const char* s, Output& output);

/* Output a coordinate pair (x, y) in decimal format. */
MyError output_coordpair(int32_t x, int32_t y, Output& output);
MyError output_coordpair(OS::Millipoint x, OS::Millipoint y, Output& output);

/* Output a coordinate pair (x, y) in decimal format. */
MyError output_coordpair(Point<OS::Unit> value, Output& output);
MyError output_coordpair(Offset<OS::Unit> value, Output& output);
MyError output_coordpair(Size<OS::Unit> value, Output& output);

MyError output_coordpair(Point<Draw::Unit> value, Output& output);
MyError output_coordpair(Point<OS::Millipoint> value, Output& output);
MyError output_coordpair(Offset<OS::Millipoint> value, Output& output);

/* Output the RGB value in the top 24 bits of bbGGRR00 in decimal format. */
MyError output_rgbvalue(uint32_t bbGGRR00, Output& output);

/* Read the value of a system variable and print it to the output stream. */
MyError output_variable(const char* name, Output& output);

/* Arithmetic helpers used by PageBox. */
void arith_dpmult(int32_t a, int32_t b, int32_t *lo, int32_t *hi);
void arith_dpdivmod(int32_t dividend_lo, int32_t dividend_hi, uint32_t divisor,
                    int32_t *quotient, uint32_t *remainder);

#endif
