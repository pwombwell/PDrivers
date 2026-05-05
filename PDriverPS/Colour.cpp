#include "Core/PDriver.h"
#include "Colour.h"

#include "Output.h"
#include "PDriverPS.h"
#define PDRIVERPS_PRIVATE_NO_COMPAT_MACROS
#include "Private.h"

#include "Core/ColourUtils.h"
#include "Core/Job.h"
#include "Core/Workspace.h"

#include <stddef.h>

MyError colour_setrealrgb(uint32_t bbGGRR00, CoreJobWS& coreJob)
{
    CoreWS& ws = CoreWS::instance();
    JobWS& jobWS = (JobWS&)coreJob;
    PDriverWS& psWS = (PDriverWS&)ws;

#if PSCoordSpeedUps
    (void)psWS;
    jobWS.realcolour = bbGGRR00;
    return nullptr;
#else
    MyError err;
    GraphicsContext& ctx = jobWS.m_context.current();
    Output& output(jobWS.output());

    if ((coreJob.info.features & PDriverInfo::Colour_Colour) == 0u) {
        uint8_t grey = ColourUtils_rgbtogrey(bbGGRR00);
        if (ctx.m_colour == (uint32_t)grey) {
            return nullptr;
        }
        ctx.m_colour = (uint32_t)grey;

        if ((err = output.num(grey)) != nullptr)
            return err;

        return output.str(" G ");
    }

    bbGGRR00 &= ~0xFFu;
    if (ctx.m_colour == bbGGRR00) {
        return nullptr;
    }
    ctx.m_colour = bbGGRR00;

    if ((err = output_rgbvalue(bbGGRR00, output)) != nullptr)
        return err;

    return output.str(" C ");
#endif
}

// `colour_ensure`
MyError colour_ensure(JobWS& job)
{
#if PSCoordSpeedUps
    MyError err;
    uint32_t rgb = job.realcolour;
    GraphicsContext& ctx = job.m_context.current();
    Output& output(job.output());

    if (!(job.info.features() & PDriverInfo::Colour_Colour)) {
        uint8_t grey = ColourUtils_rgbtogrey(rgb);
        if (ctx.m_colour == (uint32_t)grey)
            return nullptr;

        ctx.m_colour = (uint32_t)grey;

        if ((err = output.num(grey)) != nullptr)
            return err;

        return output.str(" G ");
    }

    rgb &= ~0xFFu;
    if (ctx.m_colour == rgb) {
        return nullptr;
    }
    ctx.m_colour = rgb;

    if ((err = output_rgbvalue(rgb, output)) != nullptr)
        return err;

    return output.str(" C ");
#else
    return nullptr;
#endif
}

// Device.h entry point
uint8_t colour_rgbtopixval(uint32_t bbGGRR00, CoreJobWS& coreJob)
{
    if (!coreJob.info.isColour())
        return ColourUtils_rgbtogrey(bbGGRR00);

    ColourJob& colourJob = toColourJob(toJobWS(coreJob));
    return colourJob.rgbToPixelValue(bbGGRR00);
}

// Device.h entry point
void colour_rgbtopixvalwide(uint32_t bbGGRR00,
                            uint32_t& value,
                            uint8_t& bytes,
                            CoreJobWS& coreJob)
{
    // PS pixel values are always 1 byte
    value = colour_rgbtopixval(bbGGRR00, coreJob);
    bytes = 1;
}
