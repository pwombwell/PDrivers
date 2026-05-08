#include "Core/PDriver.h"

#define PDRIVERPS_PRIVATE_NO_COMPAT_MACROS
#include "Private.h"

#include "GlobalWS.h"
#include "JobWS.h"
#include "Output.h"
#include "PDriverPS.h"

#include "Core/Job.h"
#include "Core/Workspace.h"

#include "RLib/OS/Convert.h"

#include "RLibX/Font.h"
#include "RLibX/OS.h"

#include <stddef.h>

#if Medusa
extern const uint32_t SpriteType_RISCOS5;
extern const uint32_t SpriteType_New8bpp;
#endif

Module* Module::create(void* pw)
{
    return new PDriverWS(pw);
}

#if PSDebugEscapes
static MyError readescapestate(int *escaped) {
    bool escapedBool = false;
    MyError err = OS::xreadEscapeState(escapedBool);
    *escaped = escapedBool;
    if (!err) {
        volatile uint32_t *debug_ptr = (volatile uint32_t *)(uintptr_t)0x100000;
        *debug_ptr = (*escaped) ? 1u : 0u;
    }
    return err;
}
#endif

#if DevelopmentChecks
static _kernel_oserror saveoverflowdisaster = {
    0,
    "DISASTER: Too many 'save's or 'gsave's"
};

static _kernel_oserror restoreunderflowdisaster = {
    0,
    "DISASTER: Too many 'restore's or 'grestore's"
};
#endif

static MyError context_push(JobWS& jobWS)
{
#if DevelopmentChecks
    if (!jobWS.m_context.push())
        return &saveoverflowdisaster;
#else
    jobWS.m_context.push();
#endif

    return nullptr;
}

static MyError context_pop(JobWS& jobWS)
{
#if DevelopmentChecks
    if (!jobWS.m_context.pop())
        return &restoreunderflowdisaster;
#else
    jobWS.m_context.pop();
#endif

    return nullptr;
}

static MyError output_expansionbuffer(PDriverWS& ws,
                                      int32_t len,
                                      Output& output)
{
    return output_stringN((const char*)ws.expansionbuffer, len, 1, 126, output);
}

/* Ensure that we are using the OS co-ordinate system. */
MyError ensure_OScoords(Output& output, JobWS& job)
{
    if (job.coordsystem != 0) {
        job.coordsystem = 0;
        return output_grestore(job);
    }
    return nullptr;
}

/* Ensure that we are using the text co-ordinate system with the correct
 * current font factors.
 */
MyError ensure_textcoords(Output& output, JobWS& jobWS)
{
    int32_t scale_x, scale_y;
    MyError err = Font::xreadScaleFactor(scale_x, scale_y);
    if (err) {
        return err;
    }

    if (jobWS.coordsystem == 1 &&
        jobWS.coordscaleX == scale_x &&
        jobWS.coordscaleY == scale_y) {
        return nullptr;
    }

    if ((err = ensure_OScoords(output, jobWS)) != nullptr)
        return err;

    jobWS.coordsystem = 1;
    jobWS.coordscaleX = scale_x;
    jobWS.coordscaleY = scale_y;

    if ((err = output_gsave(jobWS)) != nullptr)
        return err;

    if ((err = output_coordpair(scale_x, scale_y, output)) != nullptr)
        return err;
    return output.str("TS\n");
}

// Output a "save" and update the (font and) colour control information accordingly.
MyError output_save(JobWS& job)
{
    MyError err = job.output().str("S\n");
    if (err)
        return err;

    return context_push(job);
}

MyError output_gsave(JobWS& job)
{
    MyError err = job.output().str("GS\n");
    if (err)
        return err;

    return context_push(job);
}

MyError output_restore(JobWS& job)
{
    MyError err = job.output().str("R\n");
    if (err)
        return err;

    return context_pop(job);
}

MyError output_grestore(JobWS& job)
{
    MyError err = job.output().str("GR\n");
    if (err)
        return err;

    return context_pop(job);
}

MyError output_string(const char* s, uint8_t min, uint8_t max, Output& output)
{
    return output.str(s, min, max);
}

// Send the string pointed to by `s` and containing `len` characters to the file
// handle, but only outputting characters between `min` and `max`
// (inclusive).
MyError output_stringN(const char* s, int32_t len, uint8_t min, uint8_t max,
                       Output& output)
{
    MyError err;
    const uint8_t range = max - min;

    for (int32_t i = 0; i < len; ++i) {
        char c = s[i];

        if (uint8_t(c) - min > range)
            continue;

        if ((err = output.byte(c)) != nullptr)
            return err;
    }

    return nullptr;
}

MyError output_immgstring(const char* s, Output& output)
{
    return output_gstring(s, output);
}

MyError output_gstring(const char* s, Output& output)
{
    PDriverWS& ws(PDriverWS::instance());

    size_t size = sizeof(ws.expansionbuffer);
    uint32_t written;
    MyError err = OS::gsTrans(s, (char*)ws.expansionbuffer, size, written);
    if (err)
        return err;

    if (written > 0)
        written -= 1;

    return output_expansionbuffer(ws, written, output);
}

MyError output_coordpair(int32_t x, int32_t y, Output& output)
{
    return output.writeCoordPair(x, y);
}

MyError output_coordpair(OS::Millipoint x, OS::Millipoint y, Output& output)
{
    return output.writeCoordPair(x, y);
}

MyError output_coordpair(Point<OS::Unit> value, Output& output)
{
    return output.writeCoordPair(value);
}

MyError output_coordpair(Offset<OS::Unit> value, Output& output)
{
    return output.writeCoordPair(value);
}

MyError output_coordpair(Size<OS::Unit> value, Output& output)
{
    return output.writeCoordPair(value);
}

MyError output_coordpair(Point<Draw::Unit> value, Output& output)
{
    return output.writeCoordPair(value);
}

MyError output_coordpair(Point<OS::Millipoint> point, Output& output)
{
    return output.writeCoordPair(point);
}

MyError output_coordpair(Offset<OS::Millipoint> offset, Output& output)
{
    return output.writeCoordPair(offset);
}

MyError output_rgbvalue(uint32_t bbGGRR00, Output& output)
{
    MyError err;

    if ((err = output.numSpace((bbGGRR00 >> 8) & 0xff)) != nullptr)
        return err;
    if ((err = output.numSpace((bbGGRR00 >> 16) & 0xff)) != nullptr)
        return err;

    return output.numSpace((bbGGRR00 >> 24) & 0xff);
}

/* Read the value of a system variable and print it to the output stream.
 * The value must fit in expansionbuffer and will be truncated if it doesn't.
 */
MyError output_variable(const char* name, Output& output)
{
    PDriverWS& ws(PDriverWS::instance());
    int32_t len = sizeof(ws.expansionbuffer);
    int32_t used, context;
    OS::VarType type(OS::VarType_String);
    MyError err = OS::xreadVarVal(name, ws.expansionbuffer, len, 0, type, used, context, type);
    if (err)
        return err;

    return output_stringN(ws.expansionbuffer, used, 32, 126, output);
}

/* Subroutine to multiply two single precision signed numbers together and
 * get a double precision result.
 */
void arith_dpmult(int32_t a, int32_t b, int32_t *lo, int32_t *hi) {
    int64_t result = (int64_t)a * (int64_t)b;
    if (lo) {
        *lo = (int32_t)result;
    }
    if (hi) {
        *hi = (int32_t)(result >> 32);
    }
}

/* Subroutine to divide a double precision signed number by a single precision
 * unsigned number, yielding a single precision signed result. Quotient and
 * remainder are calculated on "round to minus infinity" rules.
 */
void arith_dpdivmod(int32_t dividend_lo, int32_t dividend_hi, uint32_t divisor,
                    int32_t *quotient, uint32_t *remainder) {
    int64_t dividend = ((int64_t)dividend_hi << 32) | (uint32_t)dividend_lo;
    int64_t q = dividend / (int64_t)divisor;
    int64_t r = dividend % (int64_t)divisor;

    if (r < 0) {
        r += divisor;
        q -= 1;
    }

    if (quotient) {
        *quotient = (int32_t)q;
    }
    if (remainder) {
        *remainder = (uint32_t)r;
    }
}
