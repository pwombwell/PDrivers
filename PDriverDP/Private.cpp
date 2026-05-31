#include "kernel.h"
#include "Core/PDriver.h"
#include "Private.h"

#include "PDriverDP.h"
#include "GlobalWS.h"
#include "JobWS.h"

#include "Core/Constants.h"
#include "Core/InterceptManager.h"
#include "Core/Job.h"
#include "Core/MsgCode.h"
#include "Core/Workspace.h"

#include "RLib/Constants/Sprite.h"
#include "RLib/OS/Error.h"
#include "RLibX/Sprite.h"

#include <string.h>
#include <swis.h>

Module* Module::create(void* pw)
{
    return new PDriverWS(pw);
}

// Subroutine to pass the character in R0 through to the real VDU drivers
MyError vdu_char(uint8_t ch)
{
    ScopedPassthrough scopedPT(CoreWS::instance().m_interceptMgr, Passthrough_Wrch);
    MyError err = XOS_WriteC(ch);
    return err;
}

MyError vdu_stringN(const uint8_t* data, uint32_t len)
{
    ScopedPassthrough scopedPT(CoreWS::instance().m_interceptMgr, Passthrough_Wrch);
    MyError err = XOS_WriteN(data, len);
    return err;
}

MyError vdu_pair(uint32_t value) {
    MyError err = vdu_char((uint8_t)(value & 0xFFu));
    if (err) {
        return err;
    }
    return vdu_char((uint8_t)((value >> 8) & 0xFFu));
}

MyError vdu_counted_string(const uint8_t* data)
{
    if (!data) {
        return nullptr;
    }
    uint32_t len = data[0];
    return vdu_stringN(data + 1, len);
}

/* ---- Coordinate scaling ---- */

int32_t Divide(int32_t value, int32_t divisor) {
    if (divisor == 0) {
        return 0;
    }
    int32_t sign = (value < 0) ? -1 : 1;
    int64_t abs_val = (value < 0) ? -(int64_t)value : (int64_t)value;
    abs_val += (divisor >> 1);
    int64_t q = abs_val / divisor;
    return (int32_t)(sign * q);
}

static int32_t scale_value(int32_t value, int32_t scale) {
    int64_t product = (int64_t)value * (int64_t)scale;
    int64_t scaled = (product >> 16);
    scaled <<= bufferpix_l2size;
    return (int32_t)scaled;
}

int32_t XScale(int32_t value, JobWS& job) {
    int32_t scaled = scale_value(value, (int32_t)job.output().currentXScale());
    return Divide(scaled, 180);
}

int32_t YScale(int32_t value, JobWS& job) {
    int32_t scaled = scale_value(value, (int32_t)job.output().currentYScale());
    return Divide(scaled, 180);
}

static Draw::Unit scale(Draw::Unit v, Base::Fixed<16> scale) {
    Draw::Unit t(v.mul64(scale).toFixed());
    t /= 180;
    return t;
}

Draw::Unit XScale(Draw::Unit v, JobWS& job) {
    return scale(v, job.output().currentXScaleNEW());
}

Draw::Unit YScale(Draw::Unit v, JobWS& job) {
    return scale(v, job.output().currentYScaleNEW());
}

void Scale(Draw::Point& p, JobWS& job) {
    p.x = scale(p.x, job.output().currentXScaleNEW());
    p.y = scale(p.y, job.output().currentYScaleNEW());
}

/* ---- PDumper helpers ---- */

static MyError call_pdumper_entry(PDumper* pdumper,
                                  uint32_t reason,
                                  PDumperCall& args)
{
    if (!pdumper || !pdumper->branch_table) {
        return nullptr;
    }
    PDumperEntry entry = (PDumperEntry)pdumper->branch_table;

    // FIXME: This should call an asm wrapper that is almost identical to
    // _kernel_swi(), but simplified.
    return entry(pdumper->workspace, reason, args);
}

MyError CallPDumper(CoreWS& ws,
                    uint32_t reason,
                    PDumperCall& args)
{
    MyError err;
    PDriverWS *dp_ws = (PDriverWS *)&ws;

    ScopedEscapeEnable esc(ws.m_escapeState);
    if ((err = esc.enable()) != nullptr)
        return err;

    PDumper* pdumper = dp_ws->printer_pdumper_pointer;

#if Libra1
    uint32_t strip_type = 0;
    if (reason == PDumperReason_SetDriver) {
        const uint8_t *cfg = (const uint8_t *)(uintptr_t)args.r4;
        strip_type = cfg ? (uint32_t)cfg[4] : 0u;
    } else {
        strip_type = args.r2 & 0xFFu;
    }
    uint32_t mask = 1u << strip_type;
    if ((pdumper->striptypemask & mask) == 0)
        return PDumper::lookupError(ErrorBlock_PDumperBadStrip, "PDriverDP");
#endif

#if MakeUpCallsAtEntry
    {
        uint32_t r0 = UpCall_PDumperAction;
        uint32_t r1 = 0;
        XOS_UpCall(r0, r1, nullptr);
    }
#endif

    err = call_pdumper_entry(pdumper, reason, args);

#if MakeUpCallsAtExit
    {
        uint32_t r0 = UpCall_PDumperAction;
        uint32_t r1 = 1u;
        if (err) {
            r1 |= 2u;
        }
        XOS_UpCall(r0, r1, nullptr);
        if (r1 & 2u) {
            /* Preserve error if signalled. */
        }
    }
#endif
    if (err)
        return err;

    return esc.disableAndCheck();
}

MyError CallPDumperForJob(JobWS& job,
                          uint32_t reason,
                          PDumperCall& args)
{
    MyError err;

    ScopedEscapeEnable esc(CoreWS::instance().m_escapeState);
    if ((err = esc.enable()) != nullptr)
        return err;

    PDumper* pdumper = job.config().pdumperPointer();

#if Libra1
    if (pdumper) {
        uint32_t strip_type = 0;
        if (reason == PDumperReason_SetDriver) {
            const uint8_t *cfg = (const uint8_t *)(uintptr_t)args.r4;
            strip_type = cfg ? (uint32_t)cfg[4] : 0u;
        } else {
            strip_type = args.r2 & 0xFFu;
        }
        uint32_t mask = 1u << strip_type;
        if ((pdumper->striptypemask & mask) == 0u) {
            return PDumper::lookupError(ErrorBlock_PDumperBadStrip, "PDriverDP");
        }
    }
#endif

#if MakeUpCallsAtEntry
    {
        uint32_t r0 = UpCall_PDumperAction;
        uint32_t r1 = 0;
        XOS_UpCall(r0, r1, nullptr);
    }
#endif

    err = call_pdumper_entry(pdumper, reason, args);

#if MakeUpCallsAtExit
    {
        uint32_t r0 = UpCall_PDumperAction;
        uint32_t r1 = 1;
        if (err) {
            r1 |= 2;
        }
        XOS_UpCall(r0, r1, nullptr);
        if (r1 & 2) {
            /* Preserve error if signalled. */
        }
    }
#endif
    if (err)
        return err;
    
    return esc.disableAndCheck();
}

/* ---- Sprite helpers ---- */

// The original falls into pass_spriteop, which sets Passthrough_Sprite.
Sprite::Selector myspriteop(JobWS& job, DP::BufferSprite& buffer)
{
    return buffer.selector(job.output().spriteArea());
}

// swap output to the sprite at the start of the job_currentbuffer sprite area
MyError redirect_output(JobWS& job, CoreWS& ws)
{
    MyError err;
    Sprite::Area* spriteArea = job.output().spriteArea();

    // need to get redirection params in sprite pointer form for recognition by main code
    DP::BufferSprite* spriteId = job.output().currentBuffer();

    Sprite::ResolvedSelector resolved;
    Sprite::Selector selector(spriteArea, spriteId->name());

    {
        ScopedPassthrough scopedPT(ws.m_interceptMgr, Passthrough_Sprite);
        if ((err = selector.resolve(resolved)) != nullptr)
            return err;

        // swap output to sprite area
        
        // ...but first save the OS_SpriteOp regs (that I'm about to make) for interception code to spot.
        job.jobspriteparams = Sprite::VDUContext(SpriteReason_SwitchOutputToSprite, resolved,
                                                 job.output().vduSaveArea());

        if ((err = Sprite::switchOutputToSprite(resolved,
                                                job.output().vduSaveArea(),
                                                job.output().savedVduState())) != nullptr)
            return err;
        
        // jobspriteparams contains the current VDUContext, for inteception to detect.
        // job_savedVDUstate contains the previous VDUContext, for restore_output_state to use.
    }

    if ((err = vdu_char(5)) != nullptr)
        return err;

    // we must invalidate ColourTrans's cache, as output bitmap is changing
    // (also done in PDriverDP.ManageJob: restore_output_state)

    return ColourTrans::invalidateCache();
}

// Swap output away from our sprite.
MyError restore_output_state(JobWS& job)
{
    MyError err;

    if (job.output().savedVduState().isUnset())
        return nullptr;

    {
        ScopedPassthrough scopedPT(CoreWS::instance().m_interceptMgr, Passthrough_Sprite);
        job.output().savedVduState().restore();
    }

    job.output().savedVduState().unset();

    // we must invalidate ColourTrans's cache, as output bitmap is changing
    // (also done in PDriverDP.Private2: redirect_output)
    return ColourTrans::invalidateCache();
}

/* ---- Geometry helpers ---- */
void transform_point(const Geometry::LinearTransform& matrix,
                     OS::Unit x, OS::Unit y,
                     OS::Millipoint& outX, OS::Millipoint& outY)
{
    int64_t tX = (int64_t)matrix.a.raw() * x.value + (int64_t)matrix.c.raw() * y.value;
    int64_t tY = (int64_t)matrix.b.raw() * x.value + (int64_t)matrix.d.raw() * y.value;

    // Convert from OS units * 2^16 to 1/72000 inch: multiply by 400 / 2^16.
    outX = OS::Millipoint(int32_t((tX * 400) >> 16));
    outY = OS::Millipoint(int32_t((tY * 400) >> 16));
}

void update_limits(int32_t *limits, int32_t x, int32_t y) {
    if (!limits) {
        return;
    }
    if (x < limits[0]) {
        limits[0] = x;
    }
    if (x > limits[2]) {
        limits[2] = x;
    }
    if (y < limits[1]) {
        limits[1] = y;
    }
    if (y > limits[3]) {
        limits[3] = y;
    }
}

/* ---- Arithmetic helpers ---- */

void full_signed_multiply(int32_t a, int32_t b, int32_t *lo, int32_t *hi) {
    int64_t result = (int64_t)a * (int64_t)b;
    if (lo) {
        *lo = (int32_t)result;
    }
    if (hi) {
        *hi = (int32_t)(result >> 32);
    }
}

void full_multiply(uint32_t a, uint32_t b, uint32_t *lo, uint32_t *hi) {
    int64_t result = (int64_t)(int32_t)a * (int64_t)(uint32_t)b;
    if (lo) {
        *lo = (uint32_t)result;
    }
    if (hi) {
        *hi = (uint32_t)((uint64_t)result >> 32);
    }
}

void extended_divide(int32_t dividend_hi, int32_t dividend_lo,
                     uint32_t divisor, int32_t *quotient,
                     uint32_t *remainder) {
    if (divisor == 0) {
        if (quotient) {
            *quotient = 0;
        }
        if (remainder) {
            *remainder = 0;
        }
        return;
    }
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

// return r0*2^16/r1
int32_t divide_and_scale(int32_t value, int32_t divisor) {
    if (divisor == 0)
        return 0;

    int32_t sign = (value < 0) ? -1 : 1;
    int64_t abs_val = (value < 0) ? -(int64_t)value : (int64_t)value;
    int64_t numerator = abs_val << 16;
    numerator += (divisor >> 1);
    int64_t q = numerator / divisor;
    return (int32_t)(sign * q);
}

// return r0*2^16/r1
Base::Fixed<16> divideAndScale(const OS::Unit64& num, const OS::Unit64& divisor)
{
    int64_t d = divisor.value < 0 ? -divisor.value : divisor.value;
    if (d == 0)
        return Base::Fixed<16>::zero();

    int32_t sign = (num.value < 0) ? -1 : 1;
    int64_t numerator = ((num.value < 0) ? -num.value : num.value) << 16;
    int64_t q = (numerator + (d >> 1)) / d;
    if (sign < 0)
        q = -q;

    return Base::Fixed<16>::fromRaw((int32_t)q);
}

/* ---- Square roots ---- */
//   SquareRoot - Calculate the square root of a 32-bit number
//     Adapted for use on radius calculations: given a 32-bit number,
//     return the square root of input*2^16
//       (i.e. answer scaled up by 8 places, i.e. scaled for Draw coord)
Draw::Unit scaleAndSqrt(const OS::Unit64& v)
{
    return Draw::Unit::fromRaw((v << 16).sqrt().raw());
}

uint32_t SquareRoot(uint32_t value) {
    uint64_t scaled = (uint64_t)value << 16;
    return BigSquareRoot((uint32_t)(scaled >> 32),
                         (uint32_t)(scaled & 0xFFFFFFFFu));
}

// BigSquareRoot - Calculate the square root of a 64-bit number
uint32_t BigSquareRoot(uint32_t hi, uint32_t lo) {
    uint64_t value = ((uint64_t)hi << 32) | lo;
    uint64_t res = 0;
    uint64_t bit = 1ull << 62;
    while (bit > value) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (value >= res + bit) {
            value -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return (uint32_t)res;
}

/* ---- Rotation helpers ---- */

static MyError resolveBufferHeader(JobWS& job,
                                   const DP::BufferSprite& buffer,
                                   Sprite::ResolvedSelector& resolved,
                                   const Sprite::Header*& header,
                                   CoreWS& ws)
{
    ScopedPassthrough scopedPT(ws.m_interceptMgr, Passthrough_Sprite);
    MyError err = buffer.selector(job.output().spriteArea()).resolve(resolved);
    if (err != nullptr)
        return err;

    header = resolved.header();
    return nullptr;
}

static MyError flipCurrentBuffer(JobWS& job, uint32_t reason, CoreWS& ws)
{
    if (job.output().currentBuffer() == nullptr)
        return nullptr;

    Sprite::Selector selector = myspriteop(job, *job.output().currentBuffer());

    ScopedPassthrough scopedPT(ws.m_interceptMgr, Passthrough_Sprite);
    return _swix(OS_SpriteOp,
                 _INR(0, 2),
                 selector.areaValue() | reason,
                 selector.area().ptr(),
                 selector.id().name());
}

static uint32_t outputPixelBits(const JobWS& job)
{
#if Libra1
    if (job.config().stripType() == 4)
        return 16;
    if (job.config().stripType() == 5)
        return 32;
#endif

    return 8;
}

enum FastRotateKind {
    FastRotate_None,
    FastRotate_8bpp90,
    FastRotate_8bpp270,
    FastRotate_32bpp90,
    FastRotate_32bpp270
};

static FastRotateKind classifyFastRotate(const JobWS& job,
                                         const Sprite::HeaderView& sourceHeader,
                                         const Sprite::HeaderView& destinationHeader)
{
    uint8_t rotationId = job.output().rotationId() & 7;
    if (rotationId != 1 && rotationId != 7)
        return FastRotate_None;

    uint32_t pixelBits = outputPixelBits(job);
    uint32_t sourceWidth = sourceHeader.width((uint8_t)pixelBits);
    uint32_t sourceHeight = sourceHeader.height();
    uint32_t destinationWidth = destinationHeader.width((uint8_t)pixelBits);
    uint32_t destinationHeight = destinationHeader.height();

    if (destinationWidth != sourceHeight || destinationHeight != sourceWidth)
        return FastRotate_None;
    if ((sourceHeight & 3) != 0)
        return FastRotate_None;

    switch (job.config().stripType()) {
    case 1:
    case 3:
        return rotationId == 1 ? FastRotate_8bpp90 : FastRotate_8bpp270;

    case 5:
        return rotationId == 1 ? FastRotate_32bpp90 : FastRotate_32bpp270;

    default:
        return FastRotate_None;
    }
}

static MyError copyPixelRotated90(const Sprite::HeaderView& sourceHeader,
                                           const Sprite::HeaderView& destinationHeader,
                                           uint32_t pixelBytes,
                                           uint32_t pixelBits)
{
    uint32_t sourceWidth = sourceHeader.width((uint8_t)pixelBits);
    uint32_t sourceHeight = sourceHeader.height();
    uint32_t destinationWidth = destinationHeader.width((uint8_t)pixelBits);
    uint32_t destinationHeight = destinationHeader.height();

    if (destinationWidth != sourceHeight || destinationHeight != sourceWidth)
        return ErrorBlock_PrintBadTransform;

    const uint8_t* sourceBase = sourceHeader.imagePtr();
    uint8_t* destinationBase = destinationHeader.imagePtr();
    uint32_t sourceRowBytes = (uint32_t)sourceHeader.rowBytes();
    uint32_t destinationRowBytes = (uint32_t)destinationHeader.rowBytes();

    for (uint32_t sourceRow = 0; sourceRow < sourceHeight; ++sourceRow) {
        const uint8_t* sourcePixel = sourceBase + sourceRow * sourceRowBytes;

        for (uint32_t sourceColumn = 0; sourceColumn < sourceWidth; ++sourceColumn) {
            const uint8_t* from = sourcePixel + sourceColumn * pixelBytes;
            uint8_t* to = destinationBase +
                          (sourceWidth - 1 - sourceColumn) * destinationRowBytes +
                          sourceRow * pixelBytes;

            for (uint32_t byteIndex = 0; byteIndex < pixelBytes; ++byteIndex)
                to[byteIndex] = from[byteIndex];
        }
    }

    return nullptr;
}

static MyError copyPixelRotated90Fast32(const Sprite::HeaderView& sourceHeader,
                                        const Sprite::HeaderView& destinationHeader)
{
    uint32_t sourceWidth = sourceHeader.width(32);
    uint32_t sourceHeight = sourceHeader.height();
    uint32_t destinationWidth = destinationHeader.width(32);
    uint32_t destinationHeight = destinationHeader.height();

    if (destinationWidth != sourceHeight || destinationHeight != sourceWidth)
        return ErrorBlock_PrintBadTransform;

    const uint32_t* sourceBase = (const uint32_t*)sourceHeader.imagePtr();
    uint32_t* destinationBase = (uint32_t*)destinationHeader.imagePtr();
    uint32_t sourceRowWords = (uint32_t)sourceHeader.rowBytes() >> 2;
    uint32_t destinationRowWords = (uint32_t)destinationHeader.rowBytes() >> 2;

    for (uint32_t sourceRow = 0; sourceRow < sourceHeight; ++sourceRow) {
        const uint32_t* sourcePixel = sourceBase + sourceRow * sourceRowWords;

        for (uint32_t sourceColumn = 0; sourceColumn < sourceWidth; ++sourceColumn) {
            uint32_t* to = destinationBase +
                           (sourceWidth - 1 - sourceColumn) * destinationRowWords +
                           sourceRow;
            *to = sourcePixel[sourceColumn];
        }
    }

    return nullptr;
}

static MyError copyPixelRotated270Fast32(const Sprite::HeaderView& sourceHeader,
                                                  const Sprite::HeaderView& destinationHeader)
{
    uint32_t sourceWidth = sourceHeader.width(32);
    uint32_t sourceHeight = sourceHeader.height();
    uint32_t destinationWidth = destinationHeader.width(32);
    uint32_t destinationHeight = destinationHeader.height();

    if (destinationWidth != sourceHeight || destinationHeight != sourceWidth)
        return ErrorBlock_PrintBadTransform;

    const uint32_t* sourceBase = (const uint32_t*)sourceHeader.imagePtr();
    uint32_t* destinationBase = (uint32_t*)destinationHeader.imagePtr();
    uint32_t sourceRowWords = (uint32_t)sourceHeader.rowBytes() >> 2;
    uint32_t destinationRowWords = (uint32_t)destinationHeader.rowBytes() >> 2;

    for (uint32_t sourceRow = 0; sourceRow < sourceHeight; ++sourceRow) {
        const uint32_t* sourcePixel = sourceBase + sourceRow * sourceRowWords;

        for (uint32_t sourceColumn = 0; sourceColumn < sourceWidth; ++sourceColumn) {
            uint32_t* to = destinationBase +
                           sourceColumn * destinationRowWords +
                           (sourceHeight - 1 - sourceRow);
            *to = sourcePixel[sourceColumn];
        }
    }

    return nullptr;
}

static MyError copyPixelRotated90Fast8(const Sprite::HeaderView& sourceHeader,
                                                const Sprite::HeaderView& destinationHeader)
{
    uint32_t sourceWidth = sourceHeader.width(8);
    uint32_t sourceHeight = sourceHeader.height();
    uint32_t destinationWidth = destinationHeader.width(8);
    uint32_t destinationHeight = destinationHeader.height();

    if (destinationWidth != sourceHeight || destinationHeight != sourceWidth)
        return ErrorBlock_PrintBadTransform;
    if ((sourceHeight & 3) != 0)
        return copyPixelRotated90(sourceHeader, destinationHeader, 1, 8);

    const uint8_t* sourceBase = sourceHeader.imagePtr();
    uint8_t* destinationBase = destinationHeader.imagePtr();
    uint32_t sourceRowBytes = (uint32_t)sourceHeader.rowBytes();
    uint32_t destinationRowBytes = (uint32_t)destinationHeader.rowBytes();

    for (uint32_t sourceRow = 0; sourceRow < sourceHeight; sourceRow += 4) {
        for (uint32_t sourceColumn = 0; sourceColumn < sourceWidth; ++sourceColumn) {
            uint32_t word = (uint32_t)sourceBase[(sourceRow + 0) * sourceRowBytes + sourceColumn] |
                            ((uint32_t)sourceBase[(sourceRow + 1) * sourceRowBytes + sourceColumn] << 8) |
                            ((uint32_t)sourceBase[(sourceRow + 2) * sourceRowBytes + sourceColumn] << 16) |
                            ((uint32_t)sourceBase[(sourceRow + 3) * sourceRowBytes + sourceColumn] << 24);

            *(uint32_t*)(destinationBase +
                         (sourceWidth - 1 - sourceColumn) * destinationRowBytes +
                         sourceRow) = word;
        }
    }

    return nullptr;
}

static MyError copyPixelRotated270Fast8(const Sprite::HeaderView& sourceHeader,
                                                 const Sprite::HeaderView& destinationHeader)
{
    uint32_t sourceWidth = sourceHeader.width(8);
    uint32_t sourceHeight = sourceHeader.height();
    uint32_t destinationWidth = destinationHeader.width(8);
    uint32_t destinationHeight = destinationHeader.height();

    if (destinationWidth != sourceHeight || destinationHeight != sourceWidth)
        return ErrorBlock_PrintBadTransform;
    if ((sourceHeight & 3) != 0)
        return copyPixelRotated90(sourceHeader, destinationHeader, 1, 8);

    const uint8_t* sourceBase = sourceHeader.imagePtr();
    uint8_t* destinationBase = destinationHeader.imagePtr();
    uint32_t sourceRowBytes = (uint32_t)sourceHeader.rowBytes();
    uint32_t destinationRowBytes = (uint32_t)destinationHeader.rowBytes();

    for (uint32_t sourceRow = 0; sourceRow < sourceHeight; sourceRow += 4) {
        uint32_t destinationColumn = destinationRowBytes - 4 - sourceRow;

        for (uint32_t sourceColumn = 0; sourceColumn < sourceWidth; ++sourceColumn) {
            uint32_t word = ((uint32_t)sourceBase[(sourceRow + 3) * sourceRowBytes + sourceColumn]) |
                            ((uint32_t)sourceBase[(sourceRow + 2) * sourceRowBytes + sourceColumn] << 8) |
                            ((uint32_t)sourceBase[(sourceRow + 1) * sourceRowBytes + sourceColumn] << 16) |
                            ((uint32_t)sourceBase[(sourceRow + 0) * sourceRowBytes + sourceColumn] << 24);

            *(uint32_t*)(destinationBase +
                         sourceColumn * destinationRowBytes +
                         destinationColumn) = word;
        }
    }

    return nullptr;
}

MyError flip_buffer(JobWS& job, CoreWS& ws)
{
    if (!job.output().bufferMarked())
        return nullptr;

    MyError err = restore_output_state(job);
    if (err != nullptr)
        return err;

    // FIXME: Check these flip the right way.
    uint8_t rotationId = job.output().rotationId();
    if (!!(rotationId & DP::OutputState::RotationId_NegativeXScale)) {
        if ((err = flipCurrentBuffer(job, SpriteReason_FlipAboutYAxis, ws)) != nullptr)
            return err;
    }

    if (!!(rotationId & DP::OutputState::RotationId_NegativeYScale)) {
        if ((err = flipCurrentBuffer(job, SpriteReason_FlipAboutXAxis, ws)) != nullptr)
            return err;
    }

    return redirect_output(job, ws);
}

static MyError copySpriteWithRotateGeneric(JobWS& job,
                                           const DP::BufferSprite& source,
                                           const DP::BufferSprite& destination,
                                           CoreWS& ws)
{
    Sprite::ResolvedSelector sourceResolved;
    Sprite::ResolvedSelector destinationResolved;
    const Sprite::Header* sourceHeaderPtr = nullptr;
    const Sprite::Header* destinationHeaderPtr = nullptr;

    MyError err = resolveBufferHeader(job, source, sourceResolved, sourceHeaderPtr, ws);
    if (err != nullptr)
        return err;

    err = resolveBufferHeader(job, destination, destinationResolved, destinationHeaderPtr, ws);
    if (err != nullptr)
        return err;

    uint32_t pixelBits = outputPixelBits(job);
    uint32_t pixelBytes = pixelBits >> 3;
    if (pixelBytes == 0)
        return ErrorBlock_PrintBadTransform;

    Sprite::HeaderView sourceHeader(*sourceHeaderPtr);
    Sprite::HeaderView destinationHeader(*destinationHeaderPtr);
    err = copyPixelRotated90(sourceHeader, destinationHeader, pixelBytes, pixelBits);
    if (err != nullptr)
        return err;

    return nullptr;
}

static MyError copySpriteWithRotateFast(JobWS& job,
                                        const DP::BufferSprite& source,
                                        const DP::BufferSprite& destination,
                                        FastRotateKind fastRotateKind,
                                        CoreWS& ws)
{
    Sprite::ResolvedSelector sourceResolved;
    Sprite::ResolvedSelector destinationResolved;
    const Sprite::Header* sourceHeaderPtr = nullptr;
    const Sprite::Header* destinationHeaderPtr = nullptr;

    MyError err = resolveBufferHeader(job, source, sourceResolved, sourceHeaderPtr, ws);
    if (err != nullptr)
        return err;

    err = resolveBufferHeader(job, destination, destinationResolved, destinationHeaderPtr, ws);
    if (err != nullptr)
        return err;

    Sprite::HeaderView sourceHeader(*sourceHeaderPtr);
    Sprite::HeaderView destinationHeader(*destinationHeaderPtr);

    switch (fastRotateKind) {
    case FastRotate_8bpp90:
        return copyPixelRotated90Fast8(sourceHeader, destinationHeader);

    case FastRotate_8bpp270:
        return copyPixelRotated270Fast8(sourceHeader, destinationHeader);

    case FastRotate_32bpp90:
        return copyPixelRotated90Fast32(sourceHeader, destinationHeader);

    case FastRotate_32bpp270:
        return copyPixelRotated270Fast32(sourceHeader, destinationHeader);

    default:
        return ErrorBlock_PrintBadTransform;
    }
}

static FastRotateKind fastRotateKind(JobWS& job,
                                     const DP::BufferSprite& source,
                                     const DP::BufferSprite& destination,
                                     CoreWS& ws)
{
    Sprite::ResolvedSelector sourceResolved;
    Sprite::ResolvedSelector destinationResolved;
    const Sprite::Header* sourceHeaderPtr = nullptr;
    const Sprite::Header* destinationHeaderPtr = nullptr;

    MyError err = resolveBufferHeader(job, source, sourceResolved, sourceHeaderPtr, ws);
    if (err != nullptr)
        return FastRotate_None;

    err = resolveBufferHeader(job, destination, destinationResolved, destinationHeaderPtr, ws);
    if (err != nullptr)
        return FastRotate_None;

    return classifyFastRotate(job,
                              Sprite::HeaderView(*sourceHeaderPtr),
                              Sprite::HeaderView(*destinationHeaderPtr));
}

MyError copysprite_withrotate(JobWS& job,
                              const DP::BufferSprite& source,
                              const DP::BufferSprite& destination,
                              CoreWS& ws)
{
    return copySpriteWithRotateGeneric(job, source, destination, ws);
}

// `do_copysprite_withrotate`
MyError do_copysprite_withrotate(JobWS& job,
                                 const DP::BufferSprite& source,
                                 const DP::BufferSprite& destination,
                                 CoreWS& ws)
{
    FastRotateKind kind = fastRotateKind(job, source, destination, ws);
    if (kind != FastRotate_None)
        return copySpriteWithRotateFast(job, source, destination, kind, ws);

    return copySpriteWithRotateGeneric(job, source, destination, ws);
}

bool copysprite_withrotate_uses_fast_path(JobWS& job,
                                          const DP::BufferSprite& source,
                                          const DP::BufferSprite& destination,
                                          CoreWS& ws)
{
    return fastRotateKind(job, source, destination, ws) != FastRotate_None;
}
