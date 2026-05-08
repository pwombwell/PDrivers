#include "Core/PDriver.h"

#include "Colour.h"
#include "JobWS.h"
#include "Output.h"
#include "PDriverPS.h"
#define PDRIVERPS_PRIVATE_NO_COMPAT_MACROS
#include "Private.h"

#include "Common/PrinterFontName.h"
#include "Core/Colour.h"
#include "Core/Constants.h"
#include "Core/Job.h"
#include "Core/Workspace.h"

#include "RLibX/Font.h"
#include "RLibX/Font/ReadDefn.h"
#include "RLibX/OS.h"

#include <stddef.h>
#include <stdint.h>

#if BeingDeveloped
static const char forcekerns_name[] = "PDriver$PSForceKerning";
#endif


static void invalidate_fontstack(FontHandle font,
                                 JobWS& jobWS,
                                 PDriverWS& psWS)
{
#if PSTextSpeedUps
    jobWS.m_context.invalidate(font);
#else
    FontHandle* current = &psWS.globaltempws.fontPlotting.currentPSfont;
    if (*current == font) {
        *current = 0xFFFFFFFFu;
    }
#endif
}

/* Printer specific code for SWI PDriver_DeclareFont. */
MyError font_declare(const char* nameIn, DeclareFont_Flag flags, CoreJobWS& coreJob)
{
    JobWS& jobWS = (JobWS&)coreJob;
    PS::FontCatalogue& fonts(jobWS.document().fonts());

#if BeingDeveloped
    int32_t len = -1;
    MyError dev_err = OS::xreadVarValSize(forcekerns_name, nullptr, &len, 0);
    if (!dev_err && len != 0) {
        flags |= DeclareFont_Flag::Kern;
    }
#endif

    if (nameIn == nullptr) {
        if (!fonts.hasDeclaredFonts())
            fonts.resetDeclaredFonts();

#if PSDebugFont
        MyError err = output.str("% End of declared fonts\n");
        if (err)
            return err;
#endif
        return nullptr;
    }

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
    node->flags = flags;
    node->m_next = nullptr;
    char* bytes = node->chars();

    name.copyAsNul(bytes);

#if PSDebugFont
    if ((err = output.str("'\n")) != nullptr)
        return err;
#endif

    fonts.addDeclaredFont(node);
    return nullptr;
}

// Invalidate any references to the font handle in the font stack.
MyError font_losefont(FontHandle handle, CoreJobWS& coreJob)
{
    PDriverWS& ws = (PDriverWS&)CoreWS::instance();
    JobWS& job = (JobWS&)coreJob;

    if (handle == 0) {
        font_fontslost(coreJob);
        return nullptr;
    }

    invalidate_fontstack(handle, job, ws);
    job.document().fonts().clearMappedFont(handle);
    return nullptr;
}

void font_fontslost(CoreJobWS& coreJob)
{
    JobWS& jobWS = (JobWS&)coreJob;
    PDriverWS& psWS = (PDriverWS&)CoreWS::instance();
    for (int32_t idx = 255; idx >= 0; --idx) {
        invalidate_fontstack((uint32_t)idx, jobWS, psWS);
        jobWS.document().fonts().clearMappedFont(idx);
    }
}

/* The string initialisation routine. */
MyError font_stringstart(uint32_t *max_chars_minus1,
                         uint32_t *passes,
                         CoreJobWS& coreJob)
{
    CoreWS& ws = CoreWS::instance();
    JobWS& jobWS = (JobWS&)coreJob;
    const FontPaintState& fontPaint = ws.fontPaint();
    Output& output(jobWS.output());
    MyError err = nullptr;
#if PSDebugFont
    if ((err = output.str("% font_stringstart\n")) != nullptr)
        return err;
#endif

    if (max_chars_minus1)
        *max_chars_minus1 = 0xFF00u;

#if PSCoordSpeedUps
    uint32_t passcount = fontPaint.m_initialFlags & Font::PaintFlag_Rubout;
    if (passcount == 0)
        passcount = 1;

    if (passes)
        *passes = passcount;

    return ensure_textcoords(output, jobWS);
#else
    uint32_t passcount = fontPaint.m_flags & Font::PaintFlag_Rubout;
    if (passcount == 0)
        passcount = 1;

    if (passes)
        *passes = passcount;

    int32_t scale_x, scale_y;
    if ((err = Font::xreadScaleFactor(scale_x, scale_y)) != nullptr)
        return err;
    if ((err = output_gsave(output, jobWS)) != nullptr)
        return err;
    if ((err = output_coordpair(scale_x, scale_y, output)) != nullptr)
        return err;
    return output.str("TS\n");
#endif
}

/* The string finalisation routine. */
MyError font_stringend(CoreJobWS& coreJob)
{
    JobWS& jobWS = (JobWS&)coreJob;
#if PSCoordSpeedUps
#if PSDebugFont
    Output& output(jobWS.output());
    MyError err = output.str("% font_stringend\n");
    if (err) {
        return err;
    }
#endif
    (void)jobWS;
    return nullptr;
#else
#if PSDebugFont
    MyError err = output.str("% font_stringend\n");
    if (err) {
        return err;
    }
#endif
    return output_grestore(output, jobWS);
#endif
}

/* The pass initialisation routine. */
MyError font_passstart(uint32_t pass, CoreJobWS& coreJob)
{
    JobWS& job = (JobWS&)coreJob;

    return job.chunkPainter().startPass(pass);
}

// The pass finalisation routine.
MyError font_passend(uint32_t pass, CoreJobWS& coreJob)
{
    JobWS& job = (JobWS&)coreJob;

    return job.chunkPainter().endPass(pass);
}

// `font_paintchunk` - Handle the painting of a string chunk.
MyError font_paintchunk(const uint8_t* chunk,
                        const Point<OS::Millipoint>& position,
                        uint32_t length,
                        uint32_t pass,
                        CoreJobWS& coreJob)
{
    JobWS& job = (JobWS&)coreJob;

    return job.chunkPainter().paint(chunk, length, position, pass);
}

/* The string colour setting routines. */
MyError font_bg(uint32_t gcol, CoreJobWS& coreJob)
{
    JobWS& job = (JobWS&)coreJob;
#if PSDebugFont
    MyError err = job.output().str("% font_bg\n");
    if (err)
        return err;
#endif

    job.chunkPainter().currentColourState().setBackgroundGCOL(gcol);
    return nullptr;
}

MyError font_fg(uint32_t gcol, CoreJobWS& coreJob)
{
    JobWS& job = (JobWS&)coreJob;
#if PSDebugFont
    MyError err = job.output().str("%! font_fg\n");
    if (err)
        return err;
#endif

    job.chunkPainter().currentColourState().setForegroundGCOL(gcol);
    return nullptr;
}

MyError font_absbg(uint32_t bbGGRR00, CoreJobWS& coreJob)
{
    JobWS& job = (JobWS&)coreJob;
#if PSDebugFont
    MyError err = job.output().str("% font_absbg\n");
    if (err)
        return err;
#endif

    job.chunkPainter().currentColourState().setBackgroundRGB(bbGGRR00);
    return nullptr;
}

MyError font_absfg(uint32_t bbGGRR00, CoreJobWS& coreJob)
{
    JobWS& job = (JobWS&)coreJob;
#if PSDebugFont
    MyError err = job.output().str("% font_absfg\n");
    if (err)
        return err;
#endif

    job.chunkPainter().currentColourState().setForegroundRGB(bbGGRR00);
    return nullptr;
}

MyError font_coloffset(int32_t offset, CoreJobWS& coreJob)
{
    JobWS& job = (JobWS&)coreJob;
#if PSDebugFont
    MyError err = job.output().str("% font_coloffset\n");
    if (err)
        return err;
#endif

    job.chunkPainter().currentColourState().setForegroundOffset(offset);
    return nullptr;
}

MyError font_savecolours(CoreJobWS& coreJob)
{
    JobWS& job = (JobWS&)coreJob;
#if PSDebugFont
    MyError err = job.output().str("% font_savecolours\n");
    if (err)
        return err;
#endif

    job.chunkPainter().colours().save();
    return nullptr;
}

