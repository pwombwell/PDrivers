#include "Core/PDriver.h"

#include "JobWS.h"
#include "Output.h"
#include "PDriverPDF.h"
#include "Private.h"

#include "Core/Constants.h"
#include "Core/Device.h"
#include "Core/Job.h"
#include "Core/OS.h"
#include "Core/Workspace.h"

#include <stddef.h>

static MyError jpeg_output_realspace(Output& output,
                                     int32_t numerator,
                                     int32_t denominator)
{
    MyError err = output.writeReal(numerator, denominator);
    if (err)
        return err;

    return output.str(' ');
}

template<uint8_t T>
MyError jpeg_output_fixed_space(const Base::Fixed<T>& v, Output& output)
{
    return jpeg_output_realspace(output, v.raw(), 1u<<T);
}

MyError jpeg_ctrans_handle(ColourTrans::Table32K& outTableDescriptor,
                           CoreJobWS& coreJob)
{
    return nullptr;
}

MyError jpeg_plotscaled(const uint8_t *jpeg,
                        uint32_t length,
                        uint32_t flags,
                        OS::Unit x,
                        OS::Unit y,
                        const Sprite::ScaleFactor *scale,
                        CoreJobWS& coreJob)
{
    MyError err;
    JobWS& job = toJobWS(coreJob);
    PDriverWS& ws(PDriverWS::instance());

    if (ws.m_countingPass)
        return nullptr;

    JPEG::Info info;
    if ((err = info.parse(jpeg, length)) != nullptr)
        return err;

    PDF::ImageRecord* imageRecord;
    imageRecord = job.m_pdfDocument.registerImage(jpeg, length, info);
    if (!imageRecord)
        return MyError::OOM();

    int32_t xmag = 1;
    int32_t ymag = 1;
    int32_t xdiv = 1;
    int32_t ydiv = 1;
    if (scale) {
        xmag = (int32_t)scale->multiplierX;
        ymag = (int32_t)scale->multiplierY;
        xdiv = (int32_t)scale->divisorX;
        ydiv = (int32_t)scale->divisorY;
    }

    int64_t scale_x = (int64_t)info.width() * xmag * (1 << job.screenVars.currxeig);
    int64_t scale_y = (int64_t)info.height() * ymag * (1 << job.screenVars.curryeig);
    scale_x = xdiv ? (scale_x / xdiv) : scale_x;
    scale_y = ydiv ? (scale_y / ydiv) : scale_y;

    int32_t draw_w = (int32_t)scale_x;
    int32_t draw_h = (int32_t)scale_y;
    int32_t draw_x = x.raw();
    int32_t draw_y = y.raw();

    Output& output(job.output());

    if ((err = output.str("q\n")) != nullptr)
        return err;
    if ((err = output.numSpace(draw_w)) != nullptr)
        return err;
    if ((err = output.numSpace(0)) != nullptr)
        return err;
    if ((err = output.numSpace(0)) != nullptr)
        return err;
    if ((err = output.numSpace(draw_h)) != nullptr)
        return err;
    if ((err = output.numSpace(draw_x)) != nullptr)
        return err;
    if ((err = output.numSpace(draw_y)) != nullptr)
        return err;
    if ((err = output.str("cm\n")) != nullptr)
        return err;

    if ((err = output.str("/Im")) != nullptr)
        return err;
    if ((err = output.num((int32_t)imageRecord->objectId().value())) != nullptr)
        return err;
    if ((err = output.str(" Do\nQ\n")) != nullptr)
        return err;

    return nullptr;
}

static MyError commonPrologue(const uint8_t* jpeg,
                              uint32_t length,
                              PDF::ImageRecord*& imageRecord,
                              JobWS& job)
{
    MyError err;
    Output& output(job.output());

    JPEG::Info info;
    if ((err = info.parse(jpeg, length)) != nullptr)
        return err;

    imageRecord = job.m_pdfDocument.registerImage(jpeg, length, info);
    if (!imageRecord)
        return MyError::OOM();

    if ((err = output.str("q\n")) != nullptr)
        return err;

    return nullptr;
}

static MyError commonEpilogue(PDF::ImageRecord& record, Output& output)
{
    MyError err;

    if ((err = output.str("cm\n")) != nullptr)
        return err;
    if ((err = output.str("/Im")) != nullptr)
        return err;
    if ((err = output.num(record.objectId().value())) != nullptr)
        return err;
    if ((err = output.str(" Do\nQ\n")) != nullptr)
        return err;

    return nullptr;
}

// Device.h entry point
MyError jpeg_plottransformed(const uint8_t* jpeg, uint32_t length,
                             const Draw::Matrix& matrix, CoreJobWS& coreJob)
{
    MyError err;
    JobWS& job = toJobWS(coreJob);
    PDriverWS& ws(PDriverWS::instance());
    PDF::ImageRecord* imageRecord;

    if (ws.m_countingPass)
        return nullptr;

    Output& output(job.output());

    if ((err = commonPrologue(jpeg, length, imageRecord, job)) != nullptr)
        return err;

    if ((err = jpeg_output_fixed_space(matrix.a, output)) != nullptr)
        return err;
    if ((err = jpeg_output_fixed_space(matrix.b, output)) != nullptr)
        return err;
    if ((err = jpeg_output_fixed_space(matrix.c, output)) != nullptr)
        return err;
    if ((err = jpeg_output_fixed_space(matrix.d, output)) != nullptr)
        return err;
    if ((err = jpeg_output_fixed_space(matrix.e, output)) != nullptr)
        return err;
    if ((err = jpeg_output_fixed_space(matrix.f, output)) != nullptr)
        return err;
    if ((err = output.str("cm\n")) != nullptr)
        return err;

    uint32_t dpiX = imageRecord->dpiX();
    uint32_t dpiY = imageRecord->dpiY();
    if (dpiX == 0u)
        dpiX = 180u;
    if (dpiY == 0u)
        dpiY = 180u;

    if ((err = jpeg_output_realspace(output, 180, dpiX)) != nullptr)
        return err;
    if ((err = output.numSpace(0)) != nullptr)
        return err;
    if ((err = output.numSpace(0)) != nullptr)
        return err;
    if ((err = jpeg_output_realspace(output, 180, dpiY)) != nullptr)
        return err;
    if ((err = output.numSpace(0)) != nullptr)
        return err;
    if ((err = output.numSpace(0)) != nullptr)
        return err;
    if ((err = output.str("cm\n")) != nullptr)
        return err;

    if ((err = output.numSpace(imageRecord->width())) != nullptr)
        return err;
    if ((err = output.numSpace(0)) != nullptr)
        return err;
    if ((err = output.numSpace(0)) != nullptr)
        return err;
    if ((err = output.numSpace(imageRecord->height())) != nullptr)
        return err;
    if ((err = output.numSpace(0)) != nullptr)
        return err;
    if ((err = output.numSpace(0)) != nullptr)
        return err;

    return commonEpilogue(*imageRecord, output);
}

// Device.h entry point
MyError jpeg_plottransformed(const uint8_t* jpeg, uint32_t length,
                             const Sprite::Coords& coords, CoreJobWS& coreJob)
{
    MyError err;
    JobWS& job = toJobWS(coreJob);
    PDriverWS& ws(PDriverWS::instance());
    PDF::ImageRecord* imageRecord;

    if (ws.m_countingPass)
        return nullptr;

    Output& output(job.output());

    if ((err = commonPrologue(jpeg, length, imageRecord, job)) != nullptr)
        return err;

    Draw::Unit a = coords[1].x - coords[0].x; // x1 - x0;
    Draw::Unit b = coords[1].y - coords[0].y; // y1 - y0;
    Draw::Unit c = coords[3].x - coords[0].x; // x3 - x0;
    Draw::Unit d = coords[3].y - coords[0].y; // y3 - y0;
    Draw::Unit e = coords[0].x;               // x0;
    Draw::Unit f = coords[0].y;               // y0;

    if ((err = jpeg_output_fixed_space(a, output)) != nullptr)
        return err;
    if ((err = jpeg_output_fixed_space(b, output)) != nullptr)
        return err;
    if ((err = jpeg_output_fixed_space(c, output)) != nullptr)
        return err;
    if ((err = jpeg_output_fixed_space(d, output)) != nullptr)
        return err;
    if ((err = jpeg_output_fixed_space(e, output)) != nullptr)
        return err;
    if ((err = jpeg_output_fixed_space(f, output)) != nullptr)
        return err;

    return commonEpilogue(*imageRecord, output);
}
