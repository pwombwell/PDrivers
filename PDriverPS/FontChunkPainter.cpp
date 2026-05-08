#include "FontChunkPainter.h"

#include "Colour.h"
#include "JobWS.h"

#include "Core/Device.h"
#include "Core/FontPaintState.h"
#include "Core/Workspace.h"

PSFontChunkPainter::PSFontChunkPainter(JobWS& job)
    : m_job(job)
    , m_painter(*this, job)
{
}

void PSFontChunkPainter::resetCurrentFont()
{
#if !PSTextSpeedUps
    m_currentPSFont = 0xffffffffu;
#endif
}

bool PSFontChunkPainter::changeFont(FontHandle font)
{
#if PSTextSpeedUps
    // Are we out of step with the PostScript font? If so, we want to
    // update PostScript.
    GraphicsContext& ctx(m_job.m_context.current());
    if (ctx.m_font == font)
        return false;

    ctx.m_font = font;
    return true;
#else
    if (m_currentPSFont == font)
        return false;

    m_currentPSFont = font;
    return true;
#endif
}

// `font_paintchunk_text` (after not jumping to `font_paintchunk_fontunchanged`)
MyError PSFontChunkPainter::selectFont(FontHandle font, const Font::ReadDefn& defn)
{
    MyError err;
    Output& output(m_job.output());

    // See if we know about this font handle already
    PS::Document& doc(m_job.document());
    PrinterFontNameEntry& mapping(doc.fonts().FontNameEntry());

    PrinterFontResolution resolution = mapping.resolveForHandle(font,
                                                                defn.getIdentifier(),
                                                                doc.verbose(),
                                                                m_job.jobfontlist,
                                                                FontNameEntry::Word(PDriverMiscOp_PS_Font),
                                                                FontNameEntry::Word(PDriverMiscOp_PS_DF));

#if PSDebugFont
    if (resolution.fromCache()) {
        (void)output.str("% Font handle ");
        (void)output.str((int32_t)font);
        (void)output.str(" maps to ");
        (void)output.str(resolution.name());
        (void)output.str('\n');
    }
    if (resolution.wasDeclared()) {
        (void)output.str("% Mapping font handle ");
        (void)output.str((int32_t)font);
        (void)output.str(" to ");
        (void)output.str(resolution.name());
        (void)output.str('\n');
    }
#endif

    if ((err = output.str('/')) != nullptr)
        return err;

    const char* p = resolution.name();
    while ((uint8_t)*p >= 32) {
        char c = *p++;
        if (c == '*')
            continue;

        if ((err = output.str(c)) != nullptr)
            return err;
    }
    if ((err = output.str(' ')) != nullptr)
        return err;
    if ((err = output.writeCoordPair(defn.pointsX(), defn.pointsY())) != nullptr)
        return err;

    return output.str("Fn\n");
}

// `font_dorubout` - Print a chunk of rubout box.
MyError PSFontChunkPainter::fillRect(const Font::Rect& rect, uint32_t bbGGRR00)
{
    MyError err;
    Output& output(m_job.output());

    if ((err = colour_setrealrgb(bbGGRR00, m_job)) != nullptr)
        return err;
#if PSCoordSpeedUps
    if ((err = colour_ensure(m_job)) != nullptr)
        return err;
#endif

    if ((err = output.writeCoordPair(rect.x0, rect.y0)) != nullptr)
        return err;
    if ((err = output.writeCoordPair(rect.x1, rect.y1)) != nullptr)
        return err;

    return output.str("Bx fill\n");
}

// `font_paintchunk_NoMatrixToSend` - justifying case
MyError PSFontChunkPainter::justifying(const uint8_t* chunk, size_t length,
                                       const Font::Point& position)
{
    MyError err;
    Output& output(m_job.output());
    const FontPaintState& fontState(CoreWS::instance().fontPaint());
    const Font::PaintFlag flags = fontState.m_flags;

    const Font::Offset& spaceAdd = fontState.m_adjustments.m_spaceAdd;
    const Font::Offset& charAdd = fontState.m_adjustments.m_charAdd;

    if ((err = output.writeCoordPair(spaceAdd)) != nullptr)
        return err;
#if PSAxisAlignedJustification
    if ((flags & Font::PaintFlag_Matrix) != 0) {
        if ((err = output.str("IT ")) != nullptr)
            return err;
    }
#endif

    if ((err = output.str("32 ")) != nullptr)
        return err;

    if ((err = output.writeCoordPair(charAdd)) != nullptr)
        return err;
#if PSAxisAlignedJustification
    if ((flags & Font::PaintFlag_Matrix) != 0) {
        if ((err = output.str("IT ")) != nullptr)
            return err;
    }
#endif

    Font::Offset scanOffset(BigNum, BigNum);
    if ((err = Font::scanString(fontState.m_font, chunk, length, scanOffset)) != nullptr)
        return err;
    if ((err = output.writeCoordPair(scanOffset)) != nullptr)
        return err;

    int32_t ulpos = fontState.m_underlinePosition.raw();
    int32_t ulthick = fontState.m_underlineThickness.raw();
    if (ulthick != 0) {
        Font::Point start = position;
        Font::Point end = fontState.m_paintEndPosition;
        Font::Offset underlineOffset;

        if ((flags & Font::PaintFlag_Reversed) == 0) {
            underlineOffset = end - start;
        } else {
            underlineOffset = start - end;
        }

        int32_t ulposSigned = (int32_t)((int8_t)ulpos);
        if ((err = output.writeCoordPair(underlineOffset)) != nullptr)
            return err;
#if PSAxisAlignedJustification
        if ((flags & Font::PaintFlag_Matrix) != 0) {
            if ((err = output.str("IT ")) != nullptr)
                return err;
        }
#endif

        if ((err = output.writeCoordPair(ulposSigned, ulthick)) != nullptr)
            return err;
    }

    Point<OS::Millipoint> start;
    if ((flags & Font::PaintFlag_Matrix) == 0) {
        if ((flags & Font::PaintFlag_Reversed) == 0)
            start = position;
        else
            start = fontState.m_paintEndPosition;
    }
    if ((err = output.writeCoordPair(start)) != nullptr)
        return err;

    if (ulthick != 0) {
        if ((err = output.str("U")) != nullptr)
            return err;
    }

    if ((err = output.str("J")) != nullptr)
        return err;

    return nullptr;
}

// `font_paintchunk_notjustifying`
MyError PSFontChunkPainter::notJustifying(const uint8_t* chunk, size_t length,
                                          const Font::Point& position)
{
    MyError err;
    Output& output(m_job.output());
    const FontPaintState& fontState(CoreWS::instance().fontPaint());
    const Font::PaintFlag flags = fontState.m_flags;

    Font::Offset scanOffset(BigNum, BigNum);
    if ((err = Font::scanString(fontState.m_font, chunk, length, scanOffset)) != nullptr)
        return err;
    if ((err = output.writeCoordPair(scanOffset)) != nullptr)
        return err;

    Font::Point start;
    if ((flags & Font::PaintFlag_Matrix) == 0) {
        if ((flags & Font::PaintFlag_Reversed) == 0)
            start = position;
        else
            start = fontState.m_paintEndPosition;
    }

    if ((err = output.writeCoordPair(start)) != nullptr)
        return err;

    int32_t ulpos = fontState.m_underlinePosition.raw();
    int32_t ulthick = fontState.m_underlineThickness.raw();
    if (ulthick != 0) {
        int32_t ulposSigned = (int32_t)((int8_t)ulpos);
        if ((err = output.writeCoordPair(ulposSigned, ulthick)) != nullptr)
            return err;

        if ((flags & Font::PaintFlag_Kern) != 0) {
            Font::Offset kernOffset(BigNum, BigNum);
            if ((err = Font::scanStringWithKerning(fontState.m_font, chunk, length, kernOffset)) != nullptr)
                return err;
            if ((err = output.writeCoordPair(kernOffset)) != nullptr)
                return err;
        }

        if ((err = output.str("U")) != nullptr)
            return err;
    }

    return nullptr;
}

MyError PSFontChunkPainter::paintText(const FontChunkPaint& paint, uint32_t bbGGRR00)
{
    MyError err;
    Output& output(m_job.output());
    const uint8_t* chunk = paint.chunk();
    size_t length = paint.length();
    const Font::Point& position = paint.position();
    const FontPaintState& fontState = paint.state();

#if PSDebugFont
    if ((err = output.str("% font_paintchunk\n")) != nullptr)
        return err;
#endif

    // `font_paintchunk_fontunchanged`
    const Font::PaintFlag flags = fontState.m_flags;

    if ((flags & Font::PaintFlag_Reversed) != 0) {
        err = output.psStringBackwards(chunk, length);
    } else {
        err = output.psString(chunk, length);
    }
    if (err)
        return err;

    if ((err = colour_setrealrgb(bbGGRR00, m_job)) != nullptr)
        return err;
#if PSCoordSpeedUps
    if ((err = colour_ensure(m_job)) != nullptr)
        return err;
#endif

#if PSDebugFont
    if ((err = output.str("\n% fontpaint_flags = ")) != nullptr)
        return err;
    if ((err = output.num(flags)) != nullptr)
        return err;
    if ((err = output.str('\n')) != nullptr)
        return err;
#endif

    if ((flags & Font::PaintFlag_Matrix) != 0) {
        if ((err = outputMatrix()) != nullptr)
            return err;
    }

    // `font_paintchunk_NoMatrixToSend`

    if (fontState.m_adjustments.hasAdjustments()) {
        if ((err = justifying(chunk, length, position)) != nullptr)
            return err;
    } else {
        if ((err = notJustifying(chunk, length, position)) != nullptr)
            return err;
    }

    if (!!(flags & Font::PaintFlag_Kern)) {
        if ((err = output.str("K")) != nullptr)
            return err;
    }

    if ((err = output.str("Tx\n")) != nullptr)
        return err;

    if ((flags & Font::PaintFlag_Matrix) != 0) {
        if ((err = output.str("GR\n")) != nullptr)
            return err;
    }

    return nullptr;
}

MyError PSFontChunkPainter::outputMatrix()
{
    MyError err;
    Output& output(m_job.output());
    const FontPaintState& fontState(CoreWS::instance().fontPaint());
    const Font::PaintFlag flags = fontState.m_flags;

    if ((err = output.str("GS ")) != nullptr)
        return err;

    const Draw::Matrix& m = fontState.m_matrix;
    if ((err = output.writeCoordPair(m.a.raw(), m.b.raw())) != nullptr)
        return err;
    if ((err = output.writeCoordPair(m.c.raw(), m.d.raw())) != nullptr)
        return err;

    // since the SDM operator is organised for Draw-style transformations, it divides
    // the translational part by 256 - this is *not* required for the font transformation,
    // so is countered here (easier than defining font-specific version of SDM in prologue)
    int32_t tx = m.e.raw() << 8;
    int32_t ty = m.f.raw() << 8;
    if ((err = output.writeCoordPair(tx, ty)) != nullptr)
        return err;

    Point<OS::Millipoint> start;
    if ((flags & Font::PaintFlag_Reversed) != 0)
        start = fontState.m_paintEndPosition;

    if ((err = output.writeCoordPair(start)) != nullptr)
        return err;

    return output.str("T SDM\n");
}
