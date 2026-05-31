#include "kernel.h"
#include "Core/PDriver.h"

#include "Colour.h"
#include "GlobalWS.h"
#include "JobWS.h"
#include "Private.h"

#include "Core/Constants.h"
#include "Core/Device.h"
#include "Core/Job.h"
#include "Core/OS.h"
#include "Core/Workspace.h"

#include "RLib/OS/Error.h"

#include "swis.h"

#ifndef JPEG_Info
#define JPEG_Info 0x00049980
#endif
#ifndef JPEG_PlotTransformed
#define JPEG_PlotTransformed 0x00049984
#endif

static int32_t jpegDivideOrIdentity(int32_t dividend,
                                    int32_t divisor)
{
    if (divisor == 0)
        return dividend;

    return Divide(dividend, divisor);
}

static void jpegScaleCoords(JobWS& job,
                            int32_t& x,
                            int32_t& y)
{
    x = (XScale(x << 8, job) - job.output().currentOffset().dx.raw()) >> 8;
    y = (YScale(y << 8, job) - job.output().currentOffset().dy.raw()) >> 8;
}

static Base::Fixed<16> jpegAxisScale(const JobWS& job, bool xAxis)
{
    Base::Fixed<16> scale = xAxis
        ? job.output().currentXScaleNEW()
        : job.output().currentYScaleNEW();

    int64_t raw = (int64_t)scale.raw() * 2;
    raw = raw >= 0 ? (raw + 90) / 180 : -((-raw + 90) / 180);
    return Base::Fixed<16>::fromRaw((int32_t)raw);
}

static Point<Draw::Unit> jpegOffsetCoords(Point<Draw::Unit> p, JobWS& job)
{
    Point<Draw::Unit> point(XScale(p.x, job), YScale(p.y, job));

    point -= job.output().currentOffset();

    return point;
}

static Sprite::ScaleFactor jpegResolveScaleBlock(const Sprite::ScaleFactor* scale)
{
    if (scale == nullptr || scale == (const Sprite::ScaleFactor *)(intptr_t)-1) {
        Sprite::ScaleFactor unit = { 1, 1, 1, 1 };
        return unit;
    }

    return *scale;
}

static void jpegBuildScaledDestination(JobWS& job,
                                       uint32_t widthPixels,
                                       uint32_t heightPixels,
                                       int32_t x,
                                       int32_t y,
                                       const Sprite::ScaleFactor& scale,
                                       int32_t* destCoordsOut)
{
    uint32_t eigMask = (1u << job.screenVars.currxeig) - 1u;
    int32_t right = x + jpegDivideOrIdentity((int32_t)widthPixels *
                                             (int32_t)scale.multiplierX *
                                             (((int32_t)scale.divisorX & (int32_t)eigMask) == 0 ?
                                                 1 :
                                                 (1 << job.screenVars.currxeig)),
                                             (((int32_t)scale.divisorX & (int32_t)eigMask) == 0) ?
                                                 ((int32_t)scale.divisorX >> job.screenVars.currxeig) :
                                                 (int32_t)scale.divisorX);

    eigMask = (1u << job.screenVars.curryeig) - 1u;
    int32_t top = y + jpegDivideOrIdentity((int32_t)heightPixels *
                                           (int32_t)scale.multiplierY *
                                           (((int32_t)scale.divisorY & (int32_t)eigMask) == 0 ?
                                               1 :
                                               (1 << job.screenVars.curryeig)),
                                           (((int32_t)scale.divisorY & (int32_t)eigMask) == 0) ?
                                               ((int32_t)scale.divisorY >> job.screenVars.curryeig) :
                                               (int32_t)scale.divisorY);

    jpegScaleCoords(job, right, top);
    right >>= bufferpix_l2size;
    right <<= (8 + bufferpix_l2size);
    top >>= bufferpix_l2size;
    top <<= (8 + bufferpix_l2size);

    destCoordsOut[2] = right;
    destCoordsOut[4] = right;
    destCoordsOut[1] = top;
    destCoordsOut[3] = top;

    jpegScaleCoords(job, x, y);
    x >>= bufferpix_l2size;
    x <<= (8 + bufferpix_l2size);
    y >>= bufferpix_l2size;
    y <<= (8 + bufferpix_l2size);

    destCoordsOut[0] = x;
    destCoordsOut[6] = x;
    destCoordsOut[7] = y;
    destCoordsOut[5] = y;
}

static bool jpegNeedsColourTrans(const JobWS& job)
{
    return job.config().stripType() <= StripType_256;
}

MyError jpeg_ctrans_handle_for_strip_type(GreyTable32K& outTableDescriptor,
                                          CoreJobWS& coreJob,
                                          StripType type)
{
    JobWS& job(toJobWS(coreJob));
    if (type <= StripType_Greyscale)
        type = StripType_Greyscale;
    else if (type != StripType_256)
        return nullptr;

    if (job.colourTrans32K.bytes() == nullptr) {
        job.colourTrans32K.init32K(32*1024);
        job.colourTransTableType = StripType_Monochrome;
    }

    if (job.colourTransTableType != type) {
        uint8_t* entries = job.colourTrans32K.bytes();
        for (int32_t value = (32 * 1024) - 1; value >= 0; --value) {
            uint32_t r = (uint32_t)((value >> 0) & 0x1Fu);
            uint32_t g = (uint32_t)((value >> 5) & 0x1Fu);
            uint32_t b = (uint32_t)((value >> 10) & 0x1Fu);

            r = (r << 3) | (r >> 2);
            g = (g << 3) | (g >> 2);
            b = (b << 3) | (b >> 2);

            uint32_t bbGGRR00 = (b << 24) | (g << 16) | (r << 8);
            if (type == StripType_Greyscale) {
                uint32_t grey = (r * 5u) + (g * 9u) + (b * 2u);
                grey >>= 4;
                bbGGRR00 = (grey << 24) | (grey << 16) | (grey << 8);
            }

            entries[value] = colour_rgbtopixval(bbGGRR00, job);
        }

        job.colourTransTableType = type;
    }

    outTableDescriptor = job.colourTrans32K; // Copy 12 bytes.

    return nullptr;
}

MyError jpeg_ctrans_handle(ColourTrans::Table32K& outTableDescriptor,
                           CoreJobWS& coreJob)
{
    JobWS& job(toJobWS(coreJob));
    MyError err = jpeg_ctrans_handle_for_strip_type(job.colourTrans32K,
                                                    coreJob,
                                                    job.config().stripType());
    if (err != nullptr)
        return err;

    outTableDescriptor = job.colourTrans32K;
    return nullptr;
}

MyError jpeg_plotscaled(const uint8_t* jpeg,
                        uint32_t length,
                        uint32_t flags,
                        OS::Unit x,
                        OS::Unit y,
                        const Sprite::ScaleFactor* scale,
                        CoreJobWS& coreJob)
{
    MyError err;
    JobWS& job(toJobWS(coreJob));
    PDriverWS& ws(PDriverWS::instance());

    JPEG::Info info;
    if ((err = info.parse(jpeg, length)) != nullptr)
        return err;

    if (ws.m_countingPass) {
        if (info.workspace() > ws.jpeg_maxmemory)
            ws.jpeg_maxmemory = info.workspace();
        return nullptr;
    }

    if ((job.disabled & Disabled_NullClip) != 0u)
        return nullptr;

    ws.jpeg_ctransflag = jpegNeedsColourTrans(job);
    if ((err = XJPEG_PDriverIntercept(ws.jpeg_ctransflag ? 2u : 0u)) != nullptr)
        return err;

    int32_t adjustedCoords[8];
    jpegBuildScaledDestination(job,
                               info.width(), info.height(),
                               x.raw(),
                               y.raw(),
                               jpegResolveScaleBlock(scale),
                               adjustedCoords);

    err = _swix(JPEG_PlotTransformed,
                _INR(0, 3),
                jpeg,
                1u,
                adjustedCoords,
                length);
    if (err != nullptr)
        return err;

    err = XJPEG_PDriverIntercept(1);
    ws.jpeg_ctransflag = false;
    return err;
}

static bool skip(const JPEG::Info& info, CoreJobWS& coreJob)
{
    PDriverWS& ws(PDriverWS::instance());

    if (ws.m_countingPass) {
        if (info.workspace() > ws.jpeg_maxmemory)
            ws.jpeg_maxmemory = info.workspace();
        return true;
    }

    if (!!(coreJob.disabled & Disabled_NullClip))
        return true;

    return false;
}

static MyError commonPrologue(JobWS& job)
{
    MyError err;
    PDriverWS& ws(PDriverWS::instance());

    ws.jpeg_ctransflag = jpegNeedsColourTrans(job);
    if ((err = XJPEG_PDriverIntercept(ws.jpeg_ctransflag ? 2u : 0u)) != nullptr)
        return err;

    return nullptr;
}

// Device.h entry point.
MyError jpeg_plottransformed(const uint8_t* jpeg, uint32_t length,
                             const Draw::Matrix& matrix, CoreJobWS& coreJob)
{
    MyError err;
    JobWS& job(toJobWS(coreJob));
    PDriverWS& ws(PDriverWS::instance());

    JPEG::Info info;
    if ((err = info.parse(jpeg, length)) != nullptr)
        return err;

    if (skip(info, job))
        return nullptr;

    if ((err = commonPrologue(job)) != nullptr)
        return err;

    Draw::Matrix adjustedMatrix = matrix;

    // OS to pixels. Multiply by bufferpix_l2size. We copy the ARM original
    // that does this after the scaling, so loses precision.
    adjustedMatrix.scaleX(jpegAxisScale(job, true));
    adjustedMatrix.scaleY(jpegAxisScale(job, false));

    adjustedMatrix.translate(-job.output().currentOffset());

    err = _swix(JPEG_PlotTransformed,
                _INR(0, 3),
                jpeg,
                0, // matrix
                &adjustedMatrix,
                length);
    if (err != nullptr)
        return err;

    err = XJPEG_PDriverIntercept(1);
    ws.jpeg_ctransflag = false;
    return err;
}

// Device.h entry point.
MyError jpeg_plottransformed(const uint8_t* jpeg, uint32_t length,
                             const Sprite::Coords& coords,
                             CoreJobWS& coreJob)
{
    MyError err;
    JobWS& job(toJobWS(coreJob));
    PDriverWS& ws(PDriverWS::instance());

    JPEG::Info info;
    if ((err = info.parse(jpeg, length)) != nullptr)
        return err;

    if (skip(info, job))
        return nullptr;

    if ((err = commonPrologue(job)) != nullptr)
        return err;

    Point<Draw::Unit> scaledCoords[4] = {
        jpegOffsetCoords(coords[0], job),
        jpegOffsetCoords(coords[1], job),
        jpegOffsetCoords(coords[2], job),
        jpegOffsetCoords(coords[3], job)
    };

    err = _swix(JPEG_PlotTransformed,
                _INR(0, 3),
                jpeg,
                1u<<1, // coords
                &scaledCoords,
                length);
    if (err != nullptr)
        return err;

    err = XJPEG_PDriverIntercept(1);
    ws.jpeg_ctransflag = false;
    return err;
}
