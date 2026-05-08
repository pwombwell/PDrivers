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
#include "Core/Workspace.h"

#include "RLibX/Font.h"

#include <stddef.h>
#include <stdint.h>

/* Transitional overloads while migrating backend entry points to workspace-last. */
MyError font_restorecolours(int32_t handle, CoreWS& ws);


#if BeingDeveloped
static const char forcekerns_name[] = "PDriver$PSForceKerning";
#endif

static void invalidate_fontstack(FontHandle font,
                                 JobWS& jobWS,
                                 PDriverWS& psWS)
{
#if PSTextSpeedUps
    (void)psWS;
    jobWS.m_context.invalidate(font);
#else
    FontHandle* current = &psWS.globaltempws.fontPlotting.currentPSfont;
    if (*current == font) {
        *current = 0xffffffffu;
    }
#endif
}

/* Printer specific code for SWI PDriver_DeclareFont. */
MyError font_declare(const char* nameIn, DeclareFont_Flag flags, CoreJobWS& coreJob)
{
    JobWS& jobWS = (JobWS&)coreJob;

    if (nameIn == nullptr)
        return nullptr;

    CtrlString name(nameIn);

    size_t size = sizeof(DeclaredFont) + name.length() + 1;
    void* block;
    MyError err = rma_claim(size, block);
    if (err)
        return err;

#if PSDebugFont
    if ((err = output.str("% Declare font '")) != nullptr)
        return err;
#endif

    DeclaredFont* node = (DeclaredFont*)block;
    node->m_next = nullptr;
    node->flags = flags;
    char* bytes = node->chars();

    name.copyAsNul(bytes);

#if PSDebugFont
    if ((err = output.str("'\n")) != nullptr)
        return err;
#endif

    jobWS.declaredfonts.addHead(node);
    return nullptr;
}

/* Invalidate any references to the font handle in the font stack. */
MyError font_losefont(FontHandle font, CoreJobWS& coreJob)
{
    PDriverWS& psWS = (PDriverWS&)CoreWS::instance();
    JobWS& job = (JobWS&)coreJob;

    if (font == 0) {
        font_fontslost(coreJob);
        return nullptr;
    }

    invalidate_fontstack(font, job, psWS);
    job.clearMappedFont(font);
    return nullptr;
}

void font_fontslost(CoreJobWS& coreJob)
{
    JobWS& jobWS = (JobWS&)coreJob;
    PDriverWS& psWS = (PDriverWS&)CoreWS::instance();

    for (int32_t idx = 255; idx >= 0; --idx) {
        invalidate_fontstack((FontHandle)idx, jobWS, psWS);
    }
    jobWS.clearMappedFonts();
}

/* The string initialisation routine. */
MyError font_stringstart(uint32_t *max_chars_minus1,
                                 uint32_t *passes,
                                CoreJobWS& coreJob)
{
    CoreWS& ws = CoreWS::instance();
    const FontPaintState& fontPaint = ws.fontPaint();
    JobWS& jobWS = (JobWS&)coreJob;

    PDriverWS& psWS = (PDriverWS&)ws;
    Output& output(jobWS.output());
    MyError err;

    if (max_chars_minus1) {
        *max_chars_minus1 = 0xFF00u;
    }
    if (passes) {
        uint32_t passcount = fontPaint.m_initialFlags & Font::PaintFlag_Rubout;
        if (passcount == 0) {
            passcount = 1;
        }
        *passes = passcount;
    }

#if PSTextSpeedUps
    jobWS.m_context.current().m_font = 0xffffffffu;
#endif
    (void)psWS;
    if ((err = pagebox_setmaxbox(coreJob)) != nullptr)
        return err;

    return ensure_OScoords(output, jobWS);
}

/* The string finalisation routine. */
MyError font_stringend(CoreJobWS& coreJob)
{
    (void)coreJob;
    return nullptr;
}

/* The pass initialisation routine. */
MyError font_passstart(uint32_t pass,
                               CoreJobWS& coreJob)
{
    JobWS& job = (JobWS&)coreJob;

    return job.chunkPainter().startPass(pass);
}

/* The pass finalisation routine. */
MyError font_passend(uint32_t pass, CoreJobWS& coreJob)
{
    JobWS& job = (JobWS&)coreJob;

    return job.chunkPainter().endPass(pass);
}

/* Printer specific code to handle the painting of a string chunk. */
MyError font_paintchunk(const uint8_t *chunk,
                                const Point<OS::Millipoint>& position,
                                uint32_t length,
                                uint32_t pass,
                                CoreJobWS& coreJob)
{
    JobWS& job = (JobWS&)coreJob;
    return job.chunkPainter().paint(chunk, length, position, pass);
}

/* The string colour setting routines. */
MyError font_bg(uint32_t gcol,
                CoreJobWS& coreJob)
{
    JobWS& jobWS = (JobWS&)coreJob;
    jobWS.chunkPainter().currentColourState().setBackgroundGCOL(gcol);
    return nullptr;
}

MyError font_fg(uint32_t gcol,
                CoreJobWS& coreJob)
{
    JobWS& jobWS = (JobWS&)coreJob;
    jobWS.chunkPainter().currentColourState().setForegroundGCOL(gcol);
    return nullptr;
}

MyError font_absbg(uint32_t bbGGRR00,
                   CoreJobWS& coreJob)
{
    JobWS& jobWS = (JobWS&)coreJob;
    jobWS.chunkPainter().currentColourState().setBackgroundRGB(bbGGRR00);
    return nullptr;
}

MyError font_absfg(uint32_t bbGGRR00,
                   CoreJobWS& coreJob)
{
    JobWS& jobWS = (JobWS&)coreJob;
    jobWS.chunkPainter().currentColourState().setForegroundRGB(bbGGRR00);
    return nullptr;
}

MyError font_coloffset(int32_t offset,
                               CoreJobWS& coreJob)
{
    JobWS& jobWS = (JobWS&)coreJob;
    jobWS.chunkPainter().currentColourState().setForegroundOffset(offset);
    return nullptr;
}

MyError font_savecolours(CoreJobWS& coreJob)
{
    JobWS& jobWS = (JobWS&)coreJob;
    jobWS.chunkPainter().colours().save();
    return nullptr;
}

MyError font_restorecolours(int32_t handle,
                                    CoreWS& ws)
{
    JobWS& jobWS = (JobWS&)*ws.currentJob();
    (void)handle;
    jobWS.chunkPainter().colours().restore();
    return nullptr;
}
