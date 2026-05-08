#include "Core/PDriver.h"

#include "Colour.h"
#include "GlobalWS.h"
#include "JobWS.h"
#include "Output.h"
#include "PDriverPS.h"
#include "Private.h"

#include "Core/Constants.h"
#include "Core/Job.h"
#include "Core/OS.h"
#include "Core/Workspace.h"

#include <string.h>

static MyError vdu5_define(uint8_t ch,
                           JobWS& jobWS)
{
    if (ch < 32 || ch == 127)
        return nullptr;

    Output& output(jobWS.output());
    MyError err = output.str(ch);
    if (err)
        return err;

    if ((err = output.str("DC ")) != nullptr)
        return err;

    uint8_t *block = (uint8_t *)PDriverWS::instance().globaltempws.vdu5Plotting.chardefnblock;
    block[0] = ch;
    if ((err = XOS_Word(0x0A, block)) != nullptr)
        return err;

    // Output the character definition as a straight hexadecimal string, in
    // bottom to top, left to right order.
    for (int i = 8; i >= 1; --i) {
        if ((err = output.hex(block[i])) != nullptr)
            return err;
    }

    if ((err = output.str("\n")) != nullptr)
        return err;

    uint32_t index = (ch - 32) >> 3;
    uint8_t mask = (uint8_t)(1u << (ch & 7u));
    jobWS.vducharsdefined[index] |= mask;

    return nullptr;
}

static MyError vdu5_emptybuffer(uint32_t count, JobWS& job)
{
    if (count == 0)
        return nullptr;

    MyError err;
    Output& output(job.output());

    const Size<OS::Unit>& size(job.vdu5().charSize());

    if (size.width == 0 || size.height == 0) {
        job.wrch().setTextBufferPos(0);
        return nullptr;
    }

#if PSCoordSpeedUps
    if ((err = ensure_OScoords(output, job)) != nullptr)
        return err;
    if ((err = colour_ensure(job)) != nullptr)
        return err;
#endif

    uint8_t *buf = (uint8_t *)job.textbuffer;

    for (uint32_t i = 0; i < count; ++i) {
        uint8_t ch = buf[i];
        if (ch < 32 || ch == 127)
            continue;

        uint32_t index = (ch - 32) >> 3;
        uint8_t mask = (uint8_t)(1u << (ch & 7u));

        if ((job.vducharsdefined[index] & mask) != 0)
            continue;

        if ((err = vdu5_define(ch, job)) != nullptr)
            return err;
    }

    if ((err = output.psString(buf, count)) != nullptr)
        return err;

    if ((err = output.writeCoordPair(job.startofvdu5str)) != nullptr)
        return err;
    if ((err = output.writeCoordPair(size)) != nullptr)
        return err;
    if ((err = output.writeCoordPair(job.vdu5().autoAdvance())) != nullptr)
        return err;
    if ((err = output.str("V\n")) != nullptr)
        return err;

    job.wrch().setTextBufferPos(0);
    return nullptr;
}

MyError vdu5_char(uint8_t c, const Point<OS::Unit>& p, CoreJobWS& coreJob)
{
    JobWS& job((JobWS&)coreJob);

    // in vdu5_char or vdu5_emptybuffer
    debugLog("VDU5 captured '%c' %u\n", c >= 32 ? c : '.', c);

    uint32_t pos = job.wrch().textBufferPos();
    if (pos == 0)
        job.startofvdu5str = p;

    uint8_t* buf = (uint8_t*)job.textbuffer;
    buf[pos] = c;
    pos++;
    job.wrch().setTextBufferPos((uint8_t)pos);

    if (pos >= textbufferlen) {
        return vdu5_emptybuffer(pos, job);
    }
    return nullptr;
}

MyError vdu5_delete(const Point<OS::Unit>& p, CoreJobWS& coreJob)
{
    MyError err;
    JobWS& job((JobWS&)coreJob);
    Output& output(job.output());

#if PSCoordSpeedUps
    if ((err = ensure_OScoords(output, job)) != nullptr)
        return err;
    if ((err = colour_ensure(job)) != nullptr)
        return err;
#endif

    if ((err = output.str("(\\177) ")) != nullptr)
        return err;
    if ((err = output_coordpair(p, output)) != nullptr)
        return err;
    if ((err = output_coordpair(job.vdu5().charSize(), output)) != nullptr)
        return err;
    return output.str("0 0 V\n");
}

MyError vdu5_flush(CoreJobWS& coreJob)
{
    JobWS& job((JobWS&)coreJob);

    uint8_t count = job.wrch().textBufferPos();
    if (count == 0)
        return nullptr;

    return vdu5_emptybuffer(count, job);
}

void vdu5_changed(CoreJobWS& coreJob, uint32_t code)
{
    JobWS& job((JobWS&)coreJob);

    if ((code & ~0xFFu) != 0) {
        memset(job.vducharsdefined, 0, sizeof(job.vducharsdefined));
        return;
    }

    if (code < 32 || code == 127)
        return;

    uint32_t index = (code - 32) >> 3;
    uint8_t mask = (uint8_t)(1u << (code & 7u));

    job.vducharsdefined[index] &= (uint8_t)~mask;
}
