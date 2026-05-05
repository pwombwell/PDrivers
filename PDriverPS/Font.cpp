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

static MyError font_restorecolours(CoreWS& ws);

static JobWS& currentJobWS(CoreWS& ws)
{
    return *(JobWS*)ws.currentJob();
}

static MyError font_scanstring_width(FontHandle fontHandle,
                                     const uint8_t* chunk,
                                     uint32_t flags,
                                     uint32_t length,
                                     FontOffset& offset)
{
    return Font::scanString(fontHandle, chunk, flags, offset, length, nullptr);
}

#if BeingDeveloped
static const char forcekerns_name[] = "PDriver$PSForceKerning";
#endif

/* Routines to get the correct RGB value for the foreground/background font colours. */
static uint32_t font_getfgrgb(const JobWS& jobWS)
{
    const FontColourState& colours(jobWS.fontColours().current());
    if (!colours.foregroundUsesGcol()) {
        return colours.foregroundRgb();
    }

    uint32_t gcol = colours.foregroundGcolWithOffset();
    return gcol_lookup(gcol << 8, jobWS);
}

static uint32_t font_getbgrgb(const JobWS& jobWS)
{
    const FontColourState& colours(jobWS.fontColours().current());
    if (!colours.backgroundUsesGcol()) {
        return colours.backgroundRgb();
    }

    uint32_t gcol = colours.backgroundGcol();
    return gcol_lookup(gcol << 8, jobWS);
}

/* Subroutine to print a chunk of rubout box. */
static MyError font_dorubout(Output& output,
                             uint32_t bbGGRR00,
                             OS::Millipoint left,
                             OS::Millipoint right,
                             JobWS& job)
{
    MyError err;
    const FontPaintState& fontPaint = CoreWS::instance().fontPaint();

    FontRect box(fontPaint.m_ruboutBox);

    if (left < box.x0)
        left = box.x0;

    if (right < box.x0)
        right = box.x0;

    if (left > box.x1)
        left = box.x1;

    if (right > box.x1)
        right = box.x1;

    if (right <= left || box.y1 <= box.y0)
        return nullptr;

    if ((err = colour_setrealrgb(bbGGRR00, job)) != nullptr)
        return err;
#if PSCoordSpeedUps
    if ((err = colour_ensure(job)) != nullptr)
        return err;
#endif

    if ((err = output_coordpair(left, box.y0, output)) != nullptr)
        return err;
    if ((err = output_coordpair(right, box.y1, output)) != nullptr)
        return err;
    return output.str("Bx fill\n");
}

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
MyError font_declare(const char* nameIn, uint32_t flags, CoreJobWS& coreJob)
{
    JobWS& jobWS = (JobWS&)coreJob;
    PS::FontCatalogue& fonts(jobWS.document().fonts());

#if BeingDeveloped
    int32_t len = -1;
    MyError dev_err = OS::xreadVarValSize(forcekerns_name, nullptr, &len, 0);
    if (!dev_err && len != 0) {
        flags |= 2u;
    }
#endif

    if (nameIn == nullptr) {
        if (!fonts.hasDeclaredFonts()) {
            fonts.resetDeclaredFonts();
        }
#if PSDebugFont
        MyError err = output.str("% End of declared fonts\n");
        if (err) {
            return err;
        }
#endif
        return nullptr;
    }

    CtrlString name(nameIn);

    size_t size = sizeof(DeclaredFont) + name.length();
    void* block;
    MyError err = rma_claim(size, block);
    if (err) {
        return err;
    }

#if PSDebugFont
    if ((err = output.str("% Declare font '")) != nullptr)
        return err;
#endif

    DeclaredFont* node = (DeclaredFont*)block;
    node->flags = flags;
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

    if (max_chars_minus1) {
        *max_chars_minus1 = 0xFF00u;
    }

#if PSCoordSpeedUps
    uint32_t passcount = fontPaint.m_initialFlags & FontPaintFlag_Rubout;
    if (passcount == 0) {
        passcount = 1;
    }
    if (passes) {
        *passes = passcount;
    }

    return ensure_textcoords(output, jobWS);
#else
    uint32_t passcount = fontPaint.m_flags & FontPaintFlag_Rubout;
    if (passcount == 0) {
        passcount = 1;
    }
    if (passes) {
        *passes = passcount;
    }

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
    CoreWS& ws = CoreWS::instance();
    JobWS& jobWS = (JobWS&)coreJob;
    const FontPaintState& fontPaint = ws.fontPaint();
    PDriverWS& psWS = (PDriverWS&)ws;
    MyError err = nullptr;
#if PSDebugFont
    Output& output(jobWS.output());
    if ((err = output.str("% font_passstart\n")) != nullptr)
        return err;
#endif

#if !PSTextSpeedUps
    if (pass == 1) {
        FontHandle* current = &psWS.globaltempws.fontPlotting.currentPSfont;
        *current = 0xFFFFFFFFu;
    }
#endif

    if ((err = font_restorecolours(ws)) != nullptr)
        return err;
    if (pass == 1) {
        return nullptr;
    }

    uint32_t rgb = font_getbgrgb(jobWS);
    uint32_t *pending = &psWS.globaltempws.fontPlotting.pendingrubout_rgb;
    pending[0] = rgb;
    pending[1] = (uint32_t)fontPaint.m_ruboutBox.x0.raw();
    return nullptr;
}

/* The pass finalisation routine. */
MyError font_passend(uint32_t pass, CoreJobWS& coreJob)
{
    JobWS& job = (JobWS&)coreJob;
    PDriverWS& psWS = PDriverWS::instance();
    const FontPaintState& fontPaint = psWS.fontPaint();
    Output& output(job.output());
    if (pass != 2) {
        return nullptr;
    }

#if PSDebugFont
    MyError err = output.str("% font_passend\n");
    if (err) {
        return err;
    }
#endif

    uint32_t rgb = psWS.globaltempws.fontPlotting.pendingrubout_rgb;
    OS::Millipoint left = psWS.globaltempws.fontPlotting.pendingrubout_left;
    OS::Millipoint right = fontPaint.m_ruboutBox.x1;

    return font_dorubout(output, rgb, left, right, job);
}

// `font_paintchunk_text` (after not jumping to `font_paintchunk_fontunchanged`)
MyError fontChanged(FontHandle fontHandle, Output& output, JobWS& job)
{
    MyError err;
    Font::ReadDefn defn;

    // Read font definition; needed to get size even if the font is in fontmapping
    if ((err = defn.init(fontHandle)) != nullptr)
        return err;

    // See if we know about this font handle already
    PS::Document& doc(job.document());
    PrinterFontMapping& mapping(doc.fonts().fontMapping());

    PrinterFontResolution resolution = mapping.resolveForHandle(fontHandle,
                                                                defn.getIdentifier(),
                                                                doc.verbose(),
                                                                job.jobfontlist,
                                                                FontBlockWord(PDriverMiscOp_PS_Font),
                                                                FontBlockWord(PDriverMiscOp_PS_DF));

    const char *ps_font_name = resolution.name();
#if PSDebugFont
    if (resolution.fromCache()) {
        (void)output.str("% Font handle ");
        (void)output.str((int32_t)font_handle);
        (void)output.str(" maps to ");
        (void)output.str(ps_font_name);
        (void)output.str('\n');
    }
    if (resolution.wasDeclared()) {
        (void)output.str("% Mapping font handle ");
        (void)output.str((int32_t)font_handle);
        (void)output.str(" to ");
        (void)output.str(ps_font_name);
        (void)output.str('\n');
    }
#endif

    if ((err = output.str('/')) != nullptr)
        return err;
    const char *p = ps_font_name;
    while ((unsigned char)*p >= 32) {
        char c = *p++;
        if (c == '*')
            continue;

        if ((err = output.str(c)) != nullptr)
            return err;
    }
    if ((err = output.str(' ')) != nullptr)
        return err;
    if ((err = output_coordpair(Point<Draw::Unit>(defn.pointsX(), defn.pointsY()), output)) != nullptr)
        return err;

    return output.str("Fn\n");
}

// Printer specific code to handle the painting of a string chunk.
MyError font_paintchunk(const uint8_t *chunk,
                        const Point<OS::Millipoint>& position,
                        uint32_t length,
                        uint32_t pass,
                        CoreJobWS& coreJob)
{
    MyError err;
    JobWS& job = (JobWS&)coreJob;

    CoreWS& ws = CoreWS::instance();

    const FontPaintState& fontPaint = ws.fontPaint();
    PDriverWS& psWS = (PDriverWS&)ws;

    Output& output(job.output());
    
#if PSDebugFont
    if ((err = output.str("% font_paintchunk\n")) != nullptr)
        return err;
#endif

    if (pass == 2) {
        uint32_t new_rgb = font_getbgrgb(job);
        uint32_t *pending = &psWS.globaltempws.fontPlotting.pendingrubout_rgb;
        uint32_t pending_rgb = pending[0];
        int32_t pending_left = (int32_t)pending[1];

        if (new_rgb != pending_rgb) {
            if ((err = font_dorubout(output, pending_rgb, pending_left, position.x, job)) != nullptr)
                return err;
            pending[0] = new_rgb;
            pending[1] = (uint32_t)position.x.raw();
        }
        return nullptr;
    }

    FontHandle fontHandle = fontPaint.m_font; // `fontpaint_font`
    bool fontHasChanged = false;

#if PSTextSpeedUps
    // Are we out of step with the PostScript font? If so, we want to
    // update PostScript.
    GraphicsContext& ctx(job.m_context.current());
    if (ctx.m_font != fontHandle) {
        ctx.m_font = fontHandle;
        fontHasChanged = true;
    }
#else
    FontHandle* current = &psWS.globaltempws.fontPlotting.currentPSfont;
    if (*current != font_handle) {
        *current = font_handle;
        fontHasChanged = true;
    }
#endif

    if (fontHasChanged) {
        if ((err = fontChanged(fontHandle, output, job)) != nullptr)
            return err;
    }

    // `font_paintchunk_fontunchanged`
    uint32_t flags = fontPaint.m_flags;

    if ((flags & FontPaintFlag_Reversed) != 0) {
        err = output_PSstringBackwards(chunk, length, output);
    } else {
        err = output_PSstring(chunk, length, output);
    }
    if (err)
        return err;

    uint32_t rgb = font_getfgrgb(job);
    if ((err = colour_setrealrgb(rgb, job)) != nullptr)
        return err;
#if PSCoordSpeedUps
    if ((err = colour_ensure(job)) != nullptr)
        return err;
#endif

#if PSDebugFont
    if ((err = output.str("\n% fontpaint_flags = ")) != nullptr)
        return err;
    if ((err = output.num((int32_t)flags)) != nullptr)
        return err;
    if ((err = output.str('\n')) != nullptr)
        return err;
#endif

    if ((flags & FontPaintFlag_Matrix) != 0) {
        if ((err = output.str("GS ")) != nullptr)
            return err;

        const Draw::Matrix& m = fontPaint.m_matrix;
        if ((err = output_coordpair(m.a.raw(), m.b.raw(), output)) != nullptr)
            return err;
        if ((err = output_coordpair(m.c.raw(), m.d.raw(), output)) != nullptr)
            return err;

        // since the SDM operator is organised for Draw-style transformations, it divides
        // the translational part by 256 - this is *not* required for the font transformation,
        // so is countered here (easier than defining font-specific version of SDM in prologue)
        int32_t tx = m.e.raw() << 8;
        int32_t ty = m.f.raw() << 8;
        if ((err = output_coordpair(tx, ty, output)) != nullptr)
            return err;

        int32_t start_x = position.x.raw();
        int32_t start_y = position.y.raw();
        if ((flags & FontPaintFlag_Reversed) != 0) {
            start_x = fontPaint.m_paintEndPosition.x.raw();
            start_y = fontPaint.m_paintEndPosition.y.raw();
        }
        if ((err = output_coordpair(start_x, start_y, output)) != nullptr)
            return err;

        if ((err = output.str("T SDM\n")) != nullptr)
            return err;
    }

    FontOffset spaceadd = fontPaint.m_adjustments.m_spaceAdd;
    FontOffset charadd = fontPaint.m_adjustments.m_charAdd;

    if (spaceadd.dx != 0 || spaceadd.dy != 0 || charadd.dx != 0 || charadd.dy != 0) {
        if ((err = output.writeCoordPair(spaceadd)) != nullptr)
            return err;
#if PSAxisAlignedJustification
        if ((flags & FontPaintFlag_Matrix) != 0) {
            if ((err = output.str("IT ")) != nullptr)
                return err;
        }
#endif

        if ((err = output.str("32 ")) != nullptr)
            return err;

        if ((err = output.writeCoordPair(charadd)) != nullptr)
            return err;
#if PSAxisAlignedJustification
        if ((flags & FontPaintFlag_Matrix) != 0) {
            if ((err = output.str("IT ")) != nullptr)
                return err;
        }
#endif

        FontOffset scanOffset(BigNum, BigNum);
        if ((err = font_scanstring_width(fontHandle, chunk, 0x180u,
                                         length, scanOffset)) != nullptr)
            return err;
        if ((err = output.writeCoordPair(scanOffset)) != nullptr)
            return err;

        int32_t ulpos = fontPaint.m_underlinePosition.raw();
        int32_t ulthick = fontPaint.m_underlineThickness.raw();
        if (ulthick != 0) {
            FontPoint start = position;
            FontPoint end = fontPaint.m_paintEndPosition;
            FontOffset underlineOffset;

            if ((flags & FontPaintFlag_Reversed) == 0) {
                underlineOffset = end - start;
            } else {
                underlineOffset = start - end;
            }

            int32_t ulpos_signed = (int32_t)((int8_t)ulpos);
            if ((err = output.writeCoordPair(underlineOffset)) != nullptr)
                return err;
#if PSAxisAlignedJustification
            if ((flags & FontPaintFlag_Matrix) != 0) {
                if ((err = output.str("IT ")) != nullptr)
                    return err;
            }
#endif

            if ((err = output_coordpair(ulpos_signed, ulthick, output)) != nullptr)
                return err;
        }

        int32_t start_x = 0;
        int32_t start_y = 0;
        if ((flags & FontPaintFlag_Matrix) == 0) {
            if ((flags & FontPaintFlag_Reversed) == 0) {
                start_x = position.x.raw();
                start_y = position.y.raw();
            } else {
                start_x = fontPaint.m_paintEndPosition.x.raw();
                start_y = fontPaint.m_paintEndPosition.y.raw();
            }
        }
        if ((err = output_coordpair(start_x, start_y, output)) != nullptr)
            return err;

        if (ulthick != 0) {
            if ((err = output.str("U")) != nullptr)
                return err;
        }

        if ((err = output.str("J")) != nullptr)
            return err;
    } else {
        FontOffset scanOffset(BigNum, BigNum);
        if ((err = font_scanstring_width(fontHandle, chunk, 0x180u,
                                         length, scanOffset)) != nullptr)
            return err;
        if ((err = output.writeCoordPair(scanOffset)) != nullptr)
            return err;

        int32_t start_x = 0;
        int32_t start_y = 0;
        if ((flags & FontPaintFlag_Matrix) == 0) {
            if ((flags & FontPaintFlag_Reversed) == 0) {
                start_x = position.x.raw();
                start_y = position.y.raw();
            } else {
                start_x = fontPaint.m_paintEndPosition.x.raw();
                start_y = fontPaint.m_paintEndPosition.y.raw();
            }
        }
        if ((err = output_coordpair(start_x, start_y, output)) != nullptr)
            return err;

        int32_t ulpos = fontPaint.m_underlinePosition.raw();
        int32_t ulthick = fontPaint.m_underlineThickness.raw();
        if (ulthick != 0) {
            int32_t ulpos_signed = (int32_t)((int8_t)ulpos);
            if ((err = output_coordpair(ulpos_signed, ulthick, output)) != nullptr)
                return err;

            if ((flags & FontPaintFlag_Kern) != 0) {
                FontOffset kernOffset(BigNum, BigNum);
                if ((err = font_scanstring_width(fontHandle, chunk, 0x380u,
                                                 length, kernOffset)) != nullptr)
                    return err;
                if ((err = output.writeCoordPair(kernOffset)) != nullptr)
                    return err;
            }

            if ((err = output.str("U")) != nullptr)
                return err;
        }
    }

    if (!!(flags & FontPaintFlag_Kern)) {
        if ((err = output.str("K")) != nullptr)
            return err;
    }

    if ((err = output.str("Tx\n")) != nullptr)
        return err;

    if ((flags & FontPaintFlag_Matrix) != 0) {
        if ((err = output.str("GR\n")) != nullptr)
            return err;
    }

    return nullptr;
}

/* The string colour setting routines. */
MyError font_bg(uint32_t gcol,
                CoreJobWS& coreJob)
{
    JobWS& jobWS = (JobWS&)coreJob;
    Output& output(jobWS.output());
#if PSDebugFont
    MyError err = output.str("% font_bg\n");
    if (err) {
        return err;
    }
#else
    (void)output;
#endif

    jobWS.fontColours().current().setBackgroundGcol(gcol);
    return nullptr;
}

MyError font_fg(uint32_t gcol,
                CoreJobWS& coreJob)
{
    JobWS& jobWS = (JobWS&)coreJob;
    Output& output(jobWS.output());
#if PSDebugFont
    MyError err = output.str("%! font_fg\n");
    if (err) {
        return err;
    }
#else
    (void)output;
#endif

    jobWS.fontColours().current().setForegroundGcol(gcol);
    return nullptr;
}

MyError font_absbg(uint32_t bbGGRR00,
                   CoreJobWS& coreJob)
{
    JobWS& jobWS = (JobWS&)coreJob;
    Output& output(jobWS.output());
#if PSDebugFont
    MyError err = output.str("% font_absbg\n");
    if (err) {
        return err;
    }
#else
    (void)output;
#endif

    jobWS.fontColours().current().setBackgroundRgb(bbGGRR00);
    return nullptr;
}

MyError font_absfg(uint32_t bbGGRR00,
                   CoreJobWS& coreJob)
{
    JobWS& jobWS = (JobWS&)coreJob;
    Output& output(jobWS.output());
#if PSDebugFont
    MyError err = output.str("% font_absfg\n");
    if (err) {
        return err;
    }
#else
    (void)output;
#endif

    jobWS.fontColours().current().setForegroundRgb(bbGGRR00);
    return nullptr;
}

MyError font_coloffset(int32_t offset, CoreJobWS& coreJob)
{
    JobWS& jobWS = (JobWS&)coreJob;
    Output& output(jobWS.output());
#if PSDebugFont
    MyError err = output.str("% font_coloffset\n");
    if (err) {
        return err;
    }
#else
    (void)output;
#endif

    jobWS.fontColours().current().setForegroundOffset(offset);
    return nullptr;
}

MyError font_savecolours(CoreJobWS& coreJob)
{
    JobWS& jobWS = (JobWS&)coreJob;
    Output& output(jobWS.output());
#if PSDebugFont
    MyError err = output.str("% font_savecolours\n");
    if (err) {
        return err;
    }
#else
    (void)output;
#endif

    jobWS.fontColours().save();
    return nullptr;
}

static MyError font_restorecolours(CoreWS& ws)
{
    JobWS& jobWS = currentJobWS(ws);

    jobWS.fontColours().restore();
    return nullptr;
}
