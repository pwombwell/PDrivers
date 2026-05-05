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

static uint8_t *vducharsdefined_bytes(JobWS& job)
{
    return (uint8_t *)job.vducharsdefined;
}

static uint8_t *vducharsdefined_base(JobWS& job)
{
    return vducharsdefined_bytes(job) - 4;
}

static MyError vdu5_define(uint8_t ch,
                           JobWS& jobWS,
                           PDriverWS& psWS)
{
    if (ch == 127) {
        return nullptr;
    }

    Output& output(jobWS.output());
    MyError err = output.str(ch);
    if (err) {
        return err;
    }
    if ((err = output.str("DC ")) != nullptr)
        return err;

    uint8_t *block = (uint8_t *)psWS.globaltempws.vdu5Plotting.chardefnblock;
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

    uint8_t *base = vducharsdefined_base(jobWS);
    uint32_t index = ch >> 3;
    uint8_t mask = (uint8_t)(1u << (ch & 7u));
    base[index] |= mask;
    return nullptr;
}

static MyError vdu5_emptybuffer(uint32_t count, CoreWS& ws)
{
    if (count == 0)
        return nullptr;

    MyError err;

    JobWS& job(*(JobWS*)ws.currentJob());
    PDriverWS& psWS = (PDriverWS&)ws;
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
    uint8_t *base = vducharsdefined_base(job);

    for (uint32_t i = 0; i < count; ++i) {
        uint8_t ch = buf[i];
        uint32_t index = ch >> 3;
        uint8_t mask = (uint8_t)(1u << (ch & 7u));
        if ((base[index] & mask) != 0)
            continue;

        if ((err = vdu5_define(ch, job, psWS)) != nullptr)
            return err;
    }

    if ((err = output_PSstring(buf, count, output)) != nullptr)
        return err;

    if ((err = output_coordpair(job.startofvdu5str, output)) != nullptr)
        return err;
    if ((err = output_coordpair(size, output)) != nullptr)
        return err;
    if ((err = output_coordpair(job.vdu5().autoAdvance(), output)) != nullptr)
        return err;
    if ((err = output.str("V\n")) != nullptr)
        return err;

    job.wrch().setTextBufferPos(0);
    return nullptr;
}

MyError vdu5_char(uint8_t ch, Point<OS::Unit> p, CoreWS& ws)
{
// in vdu5_char or vdu5_emptybuffer
    debugLog("VDU5 captured '%c' %u\n", ch >= 32 ? ch : '.', ch);   
    
    JobWS& job(*(JobWS*)ws.currentJob());
    uint32_t pos = job.wrch().textBufferPos();
    if (pos == 0)
        job.startofvdu5str = p;

    uint8_t *buf = (uint8_t *)job.textbuffer;
    buf[pos] = ch;
    pos++;
    job.wrch().setTextBufferPos((uint8_t)pos);

    if (pos >= textbufferlen) {
        return vdu5_emptybuffer(pos, ws);
    }
    return nullptr;
}

MyError vdu5_delete(Point<OS::Unit> p, CoreWS& ws)
{
    JobWS& job(*(JobWS*)ws.currentJob());
    Output& output(job.output());
    MyError err = nullptr;
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
    CoreWS& ws = CoreWS::instance();
    uint8_t count = coreJob.wrch().textBufferPos();
    if (count == 0) {
        return nullptr;
    }
    return vdu5_emptybuffer(count, ws);
}

void vdu5_changed(CoreJobWS& coreJob, uint32_t code)
{
    JobWS& job((JobWS&)coreJob);

    if ((code & ~0xFFu) != 0) {
        memset(job.vducharsdefined, 0, sizeof(job.vducharsdefined));
        return;
    }

    uint8_t *base = vducharsdefined_base(job);
    uint32_t index = code >> 3;
    uint8_t mask = (uint8_t)(1u << (code & 7u));
    base[index] &= (uint8_t)~mask;
}
