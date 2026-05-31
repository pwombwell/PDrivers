#ifndef PDRIVERDP_PRIVATE_H
#define PDRIVERDP_PRIVATE_H

#include "Constants.h"

#include "RLib/Draw.h"
#include "RLib/Geometry.h"
#include "RLibX/Sprite.h"

class CoreWS;
class CoreJobWS;
class JobWS;

inline OS::Unit toOSUnit(Draw::Unit v) {
    return OS::Unit(v.raw() >> 8);
}

inline Offset<OS::Unit> toOSUnit(Offset<Draw::Unit> offset) {
    return Offset<OS::Unit>(toOSUnit(offset.dx), toOSUnit(offset.dy));
}

inline void alignToPixel(OS::Unit& u) {
    u.value &= ~((1 << bufferpix_l2size) - 1);
}

inline void alignToPixel(Offset<OS::Unit>& offset) {
    alignToPixel(offset.dx);
    alignToPixel(offset.dy);
}

namespace DP { class BufferSprite; }

/* Private helper routines for the bitmap (DP) printer driver. */

/* PDumper call context (mirrors register usage in assembler). */
struct PDumperCall {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
};

typedef MyError (*PDumperEntry)(void* workspace,
                                uint32_t reason,
                                PDumperCall& call);

/* Common VDU output helpers (passthrough aware). */
MyError vdu_char(uint8_t ch);
MyError vdu_stringN(const uint8_t *data, uint32_t len);
MyError vdu_pair(uint32_t value);
MyError vdu_counted_string(const uint8_t *data);

/* Coordinate scaling helpers. */
int32_t XScale(int32_t value, JobWS& job);
int32_t YScale(int32_t value, JobWS& job);
int32_t Divide(int32_t value, int32_t divisor);

Draw::Unit XScale(Draw::Unit v, JobWS& job);
Draw::Unit YScale(Draw::Unit v, JobWS& job);

void Scale(Draw::Point& p, JobWS& job);

/* Colour / sprite helpers shared across DP files. */
MyError setBackgroundColour(uint32_t bbGGRR00, JobWS& job);

/* PDumper call helpers. */
MyError CallPDumper(CoreWS& ws,
                    uint32_t reason,
                    PDumperCall& call);
MyError CallPDumperForJob(JobWS& job,
                          uint32_t reason,
                          PDumperCall& call);

/* Sprite redirection helpers. */
MyError redirect_output(JobWS& job, CoreWS& ws);
MyError restore_output_state(JobWS& job);

// Combine Sprite::Id with Sprite::Header.
// Retained .s name for clarity, despite purpose evolving lots.
Sprite::Selector myspriteop(JobWS& job, DP::BufferSprite& buffer);

/* Geometry helpers used by sprite transformation. */
void transform_point(const Geometry::LinearTransform& matrix,
                     OS::Unit x, OS::Unit y,
                     OS::Millipoint& outX, OS::Millipoint& outY);
void update_limits(int32_t *limits, int32_t x, int32_t y);

/* 64-bit arithmetic helpers. */
void full_signed_multiply(int32_t a, int32_t b, int32_t *lo, int32_t *hi);
void full_multiply(uint32_t a, uint32_t b, uint32_t *lo, uint32_t *hi);
void extended_divide(int32_t dividend_hi, int32_t dividend_lo,
                     uint32_t divisor, int32_t *quotient,
                     uint32_t *remainder);

/* Misc helpers. */
int32_t divide_and_scale(int32_t value, int32_t divisor);
Base::Fixed<16> divideAndScale(const OS::Unit64& a, const OS::Unit64& b);

Draw::Unit scaleAndSqrt(const OS::Unit64& v);

uint32_t SquareRoot(uint32_t value);
uint32_t BigSquareRoot(uint32_t hi, uint32_t lo);

/* Rotation helpers (still provided in assembler if not converted). */
MyError flip_buffer(JobWS& job, CoreWS& ws);
MyError copysprite_withrotate(JobWS& job,
                              const DP::BufferSprite& source,
                              const DP::BufferSprite& destination,
                              CoreWS& ws);
MyError do_copysprite_withrotate(JobWS& job,
                                 const DP::BufferSprite& source,
                                 const DP::BufferSprite& destination,
                                 CoreWS& ws);
bool copysprite_withrotate_uses_fast_path(JobWS& job,
                                          const DP::BufferSprite& source,
                                          const DP::BufferSprite& destination,
                                          CoreWS& ws);

/* External constants (from system headers / assembler). */
extern const uint32_t PDumperReason_StartJob;
extern const uint32_t PDumperReason_StartPage;
extern const uint32_t PDumperReason_EndPage;
extern const uint32_t PDumperReason_OutputDump;
extern const uint32_t PDumperReason_ColourSet;
extern const uint32_t PDumperReason_SetDriver;
extern const uint32_t PDumperReason_MiscOp;
extern const uint32_t PDumperReason_AbortJob;

extern const uint32_t UpCall_PDumperAction;

#ifndef passthrough_wrch
extern const uint32_t passthrough_wrch;
#endif
#ifndef passthrough_spr
extern const uint32_t passthrough_spr;
#endif

#endif
