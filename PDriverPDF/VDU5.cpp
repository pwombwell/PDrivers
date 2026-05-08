#include "Core/PDriver.h"

#include "Colour.h"
#include "GlobalWS.h"
#include "JobWS.h"
#include "Output.h"
#include "PDriverPDF.h"
#include "Private.h"

#include "Core/Constants.h"
#include "Core/Device.h"
#include "Core/Job.h"
#include "Core/OS.h"
#include "Core/Workspace.h"

#include <string.h>

static int vdu5_glyph_cached(uint8_t c, JobWS& job)
{
    if (c < 32)
        return 0;

    uint32_t index = (c - 32) >> 3;
    uint8_t mask = (uint8_t)(1 << (c & 7));
    return (job.vducharsdefined[index] & mask) != 0;
}

static void vdu5_mark_glyph_cached(uint8_t c, JobWS& job)
{
    if (c < 32)
        return;

    uint32_t index = (c - 32) >> 3;
    uint8_t mask = (uint8_t)(1 << (c & 7));
    job.vducharsdefined[index] |= mask;
}

static MyError vdu5_fill_rect(Rect<OS::Unit> rect, Output& output)
{
    if (rect.width() <= 0 || rect.height() <= 0)
        return nullptr;

    MyError err;
    if ((err = output.numSpace(rect.x0)) != nullptr)
        return err;
    if ((err = output.numSpace(rect.y0)) != nullptr)
        return err;
    if ((err = output.numSpace(rect.width())) != nullptr)
        return err;
    if ((err = output.numSpace(rect.height())) != nullptr)
        return err;

    return output.str("re\nf\n");
}

static MyError vdu5_plotchar(uint8_t c, const Point<OS::Unit>& p, CoreJobWS& coreJob)
{
    JobWS& job(toJobWS(coreJob));
    Output& output(job.output());

    const Size<OS::Unit>& size(job.vdu5().charSize());
    if (size.width == 0 || size.height == 0)
        return nullptr;

    const uint8_t* glyph = job.vduchardata[c];
    MyError err = nullptr;
    if (!vdu5_glyph_cached(c, job)) {
        uint8_t *block = (uint8_t *)PDriverWS::instance().globaltempws.vdu5Plotting.chardefnblock;
        block[0] = c;
        if ((err = XOS_Word(0x0A, block)) != nullptr)
            return err;

        for (int row = 0; row < 8; ++row) {
            job.vduchardata[c][row] = block[8 - row];
        }
        vdu5_mark_glyph_cached(c, job);
    }

    for (int row = 0; row < 8; ++row) {
        uint8_t bits = glyph[row];
        if (bits == 0) {
            continue;
        }

        Rect<OS::Unit> rect;
        rect.y0 = p.y + (size.height * row) / 8;
        rect.y1 = p.y + (size.height * (row + 1)) / 8;

        if (rect.y1 < rect.y0) {
            OS::Unit t = rect.y0;
            rect.y0 = rect.y1;
            rect.y1 = t;
        }
        if (rect.y1 <= rect.y0)
            continue;

        int col = 0;
        while (col < 8) {
            while (col < 8 && ((bits & (uint8_t)(0x80u >> col)) == 0u)) {
                col++;
            }
            if (col >= 8) {
                break;
            }

            int run_start = col;
            while (col < 8 && ((bits & (uint8_t)(0x80u >> col)) != 0u)) {
                col++;
            }
            int run_end = col;

            rect.x0 = p.x + (size.width * run_start) / 8;
            rect.x1 = p.x + (size.width * run_end) / 8;
            if (rect.x1 < rect.x0) {
                OS::Unit t = rect.x0;
                rect.x0 = rect.x1;
                rect.x1 = t;
            }

            err = vdu5_fill_rect(rect, output);
            if (err)
                return err;
        }
    }

    return nullptr;
}

static MyError vdu5_emptybuffer(uint32_t count, CoreJobWS& coreJob)
{
    MyError err;
    JobWS& job(toJobWS(coreJob));
    Output& output(job.output());

    if (count == 0)
        return nullptr;

    const Size<OS::Unit>& size(coreJob.vdu5().charSize());
    if (size.width == 0 || size.height == 0) {
        coreJob.wrch().setTextBufferPos(0);
        return nullptr;
    }

#if PSCoordSpeedUps
    if ((err = ensure_OScoords(output, job)) != nullptr)
        return err;
    if ((err = colour_ensure(job)) != nullptr)
        return err;
#endif

    uint8_t *buf = (uint8_t *)coreJob.textbuffer;

    Point<OS::Unit> p(job.startofvdu5str);
    Offset<OS::Unit> advance(coreJob.vdu5().autoAdvance());

    for (uint32_t i = 0; i < count; ++i) {
        if ((err = vdu5_plotchar(buf[i], p, job)) != nullptr)
            return err;

        p += advance;
    }

    coreJob.wrch().setTextBufferPos(0);

    return nullptr;
}

MyError vdu5_char(uint8_t c, const Point<OS::Unit>& p, CoreJobWS& coreJob)
{
    JobWS& job(toJobWS(coreJob));

    uint32_t pos = coreJob.wrch().textBufferPos();
    if (pos == 0)
        job.startofvdu5str = p;

    uint8_t *buf = (uint8_t *)coreJob.textbuffer;
    buf[pos] = c;
    pos++;
    coreJob.wrch().setTextBufferPos((uint8_t)pos);

    if (pos >= textbufferlen) {
        return vdu5_emptybuffer(pos, job);
    }
    return nullptr;
}

MyError vdu5_delete(const Point<OS::Unit>& p, CoreJobWS& coreJob)
{
    MyError err;
    JobWS& job(toJobWS(coreJob));

    if ((err = vdu5_flush(job)) != nullptr)
        return err;

    Output& output(job.output());

#if PSCoordSpeedUps
    if ((err = ensure_OScoords(output, job)) != nullptr)
        return err;
    if ((err = colour_ensure(job)) != nullptr)
        return err;
#endif

    return vdu5_plotchar(127, p, job);
}

MyError vdu5_flush(CoreJobWS& coreJob)
{
    uint8_t count = coreJob.wrch().textBufferPos();
    if (count == 0)
        return nullptr;

    return vdu5_emptybuffer(count, coreJob);
}

void vdu5_changed(CoreJobWS& coreJob, uint32_t code)
{
    JobWS& job = (JobWS&)coreJob;

    if ((code & ~0xff) != 0) {
        memset(job.vducharsdefined, 0, sizeof(job.vducharsdefined));
        return;
    }

    if (code < 32)
        return;

    uint32_t index = (code - 32) >> 3;
    uint8_t mask = (uint8_t)(1u << (code & 7u));

    job.vducharsdefined[index] &= (uint8_t)~mask;
}
