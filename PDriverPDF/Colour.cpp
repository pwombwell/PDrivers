#include "Core/PDriver.h"
#include "Colour.h"

#include "GlobalWS.h"
#include "JobWS.h"
#include "Output.h"
#include "Private.h"

#include "Core/ColourUtils.h"

#include <stddef.h>

MyError colour_setrealrgb(uint32_t bbGGRR00, CoreJobWS& coreJob)
{
    JobWS& jobWS = (JobWS&)coreJob;

#if PSCoordSpeedUps
    jobWS.realcolour = bbGGRR00;
    return nullptr;
#else
    Output& output(jobWS.output());
    GraphicsContext& ctx = jobWS.m_context.current();

    if (!coreJob.info.isColour()) {
        uint8_t grey = ColourUtils_rgbtogrey(bbGGRR00);
        if (ctx.m_colour == (uint32_t)grey) {
            return nullptr;
        }
        ctx.m_colour = (uint32_t)grey;
        return output.writeGrey(grey);
    }

    bbGGRR00 &= ~0xFFu;
    if (ctx.m_colour == bbGGRR00) {
        return nullptr;
    }
    ctx.m_colour = bbGGRR00;
    return output.writeRGB(bbGGRR00);
#endif
}

MyError colour_ensure(JobWS& job)
{
    Output& output(job.output());

#if PSCoordSpeedUps
    uint32_t rgb = job.realcolour;
    GraphicsContext& ctx = job.m_context.current();

    if (!job.info.isColour()) {
        uint8_t grey = ColourUtils_rgbtogrey(rgb);
        if (ctx.m_colour == (uint32_t)grey)
            return nullptr;

        ctx.m_colour = grey;
        return output.writeGrey(grey);
    }

    rgb &= ~0xFFu;
    if (ctx.m_colour == rgb)
        return nullptr;

    ctx.m_colour = rgb;
    return output.writeRGB(rgb);
#else
    return nullptr;
#endif
}

uint8_t colour_rgbtopixval(uint32_t bbGGRR00, CoreJobWS& coreJob)
{
    if (!coreJob.info.isColour())
        return ColourUtils_rgbtogrey(bbGGRR00);

    ColourJob& colourJob = toColourJob(toJobWS(coreJob));
    return colourJob.rgbToPixelValue(bbGGRR00);
}

void colour_rgbtopixvalwide(uint32_t bbGGRR00,
                            uint32_t& value,
                            uint8_t& bytes,
                            CoreJobWS& coreJob)
{
    // PS pixel values are always 1 byte, and PDF copies PS.
    value = colour_rgbtopixval(bbGGRR00, coreJob);
    bytes = 1;
}
