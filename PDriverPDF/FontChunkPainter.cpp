#include "Core/PDriver.h"

#include "FontChunkPainter.h"

#include "Colour.h"
#include "GlobalWS.h"
#include "JobWS.h"
#include "Output.h"
#include "Private.h"
#include "PDF/FontRecord.h"

#include "Common/PrinterFontName.h"
#include "Core/Device.h"
#include "Core/Workspace.h"

#include <string.h>

PDFFontChunkPainter::PDFFontChunkPainter(JobWS& job)
    : m_job(job)
    , m_painter(*this, job)
    , m_fontRecord(nullptr)
    , m_fontSize16(160)
{
}

static MyError fontOutputRealSpace(int32_t numerator,
                                   int32_t denominator,
                                   Output& output)
{
    MyError err = output.writeReal(numerator, denominator);
    if (err)
        return err;

    return output.str(' ');
}

static MyError fontOutputMillipointSpace(OS::Millipoint value,
                                         Output& output)
{
    return fontOutputRealSpace(value.raw(), 400, output);
}

static MyError fontOutputTextMatrix(const Draw::Matrix& matrix,
                                    const Font::Point& pos,
                                    Output& output)
{
    MyError err;

    if ((err = fontOutputRealSpace(matrix.a.raw(), 65536, output)) != nullptr)
        return err;
    if ((err = fontOutputRealSpace(matrix.b.raw(), 65536, output)) != nullptr)
        return err;
    if ((err = fontOutputRealSpace(matrix.c.raw(), 65536, output)) != nullptr)
        return err;
    if ((err = fontOutputRealSpace(matrix.d.raw(), 65536, output)) != nullptr)
        return err;
    if ((err = fontOutputMillipointSpace(pos.x, output)) != nullptr)
        return err;
    if ((err = fontOutputMillipointSpace(pos.y, output)) != nullptr)
        return err;

    return output.str("Tm\n");
}

static MyError fontOutputIdentityTextMatrix(const Font::Point& pos,
                                            Output& output)
{
    MyError err;
    if ((err = output.str("1 0 0 1 ")) != nullptr)
        return err;
    if ((err = fontOutputMillipointSpace(pos.x, output)) != nullptr)
        return err;
    if ((err = fontOutputMillipointSpace(pos.y, output)) != nullptr)
        return err;

    return output.str("Tm\n");
}

static MyError fontOutputPDFCharLiteral(uint8_t c, Output& output)
{
    MyError err;
    if ((err = output.str('(')) != nullptr)
        return err;

    if (c < ' ' || c >= 126) {
        if ((err = output.str('\\')) != nullptr)
            return err;
        if ((err = output.str('0' + (c >> 6))) != nullptr)
            return err;
        if ((err = output.str('0' + ((c >> 3) & 0x7))) != nullptr)
            return err;
        if ((err = output.str('0' + (c & 0x7))) != nullptr)
            return err;
    } else {
        if (c == '\\' || c == '(' || c == ')') {
            if ((err = output.str('\\')) != nullptr)
                return err;
        }
        if ((err = output.str(c)) != nullptr)
            return err;
    }

    return output.str(')');
}

static uint32_t fontCodeunitBytes(uint32_t flags)
{
#if UCSText
    uint32_t width = flags & (Font::PaintFlag_16bit | Font::PaintFlag_32bit);
    if (width == Font::PaintFlag_32bit)
        return 4;
    if (width == Font::PaintFlag_16bit)
        return 2;
#else
    (void)flags;
#endif
    return 1;
}

static MyError fontOutputGlyphTJ(const uint8_t* glyph,
                                 uint32_t glyphLength,
                                 Output& output)
{
    if (glyphLength == 1) {
        MyError err = fontOutputPDFCharLiteral(glyph[0], output);
        if (err)
            return err;

        return output.str(" Tj\n");
    }

    MyError err = output_PSstring(glyph, glyphLength, output);
    if (err)
        return err;

    return output.str("Tj\n");
}

static MyError fontMakeReversedUnits(const uint8_t* chunk,
                                     uint32_t length,
                                     uint32_t unitBytes,
                                     uint8_t** reversedOut)
{
    *reversedOut = nullptr;
    if (chunk == nullptr || length == 0)
        return nullptr;

    void* block;
    MyError err = rma_claim(length, block);
    if (err)
        return err;

    uint8_t* dst = (uint8_t*)block;
    uint32_t units = length / unitBytes;
    for (uint32_t i = 0; i < units; ++i) {
        const uint8_t* src = chunk + ((units - 1 - i) * unitBytes);
        memcpy(dst + (i * unitBytes), src, unitBytes);
    }

    *reversedOut = dst;
    return nullptr;
}

static MyError fontScanStringPrefix(FontHandle font,
                                    const uint8_t* text,
                                    uint32_t length,
                                    uint32_t paintFlags,
                                    Font::Offset spaceAdd,
                                    Font::Offset charAdd,
                                    Font::Offset& offset)
{
    Font::ScanStringFlag scanFlags = Font::ScanStringFlag_None;
    if ((paintFlags & Font::PaintFlag_Kern) != 0)
        scanFlags = scanFlags | Font::ScanStringFlag_Kern;
    if ((paintFlags & Font::PaintFlag_Matrix) != 0)
        scanFlags = scanFlags | Font::ScanStringFlag_Matrix;
#if UCSText
    if ((paintFlags & Font::PaintFlag_16bit) != 0)
        scanFlags = scanFlags | Font::ScanStringFlag_16bit;
    if ((paintFlags & Font::PaintFlag_32bit) != 0)
        scanFlags = scanFlags | Font::ScanStringFlag_32bit;
#endif

    Font::ScanStringBlock coordBlock;
    coordBlock.space = spaceAdd;
    coordBlock.letter = charAdd;
    coordBlock.split = -1;

    return Font::scanString(font, text, length, scanFlags, coordBlock, offset);
}

static MyError fontDrawUnderline(Output& output,
                                 uint32_t flags,
                                 int32_t startX,
                                 int32_t startY,
                                 int32_t endX,
                                 int32_t endY,
                                 int32_t ulposSigned,
                                 int32_t ulthick,
                                 JobWS& job)
{
    const FontPaintState& fontPaint = CoreWS::instance().fontPaint();
    if (ulthick == 0)
        return nullptr;

    int32_t sx = startX;
    int32_t sy = startY;
    int32_t ex = endX;
    int32_t ey = endY;

    if ((flags & Font::PaintFlag_Matrix) != 0) {
        const Draw::Matrix& m = fontPaint.m_matrix;
        int32_t offx = (int32_t)(((int64_t)m.c.raw() * ulposSigned) >> 16);
        int32_t offy = (int32_t)(((int64_t)m.d.raw() * ulposSigned) >> 16);
        sx += offx;
        sy += offy;
        ex += offx;
        ey += offy;
    } else {
        sy += ulposSigned;
        ey += ulposSigned;
    }

    if (sx == ex && sy == ey)
        return nullptr;

    int32_t lineWidth = (ulthick < 0) ? -ulthick : ulthick;
    if (lineWidth == 0)
        return nullptr;

    MyError err = output_save(job);
    if (err)
        return err;

    if ((err = fontOutputMillipointSpace(OS::Millipoint(lineWidth), output)) != nullptr)
        return err;
    if ((err = output.str("w\n")) != nullptr)
        return err;

    if ((err = fontOutputMillipointSpace(OS::Millipoint(sx), output)) != nullptr)
        return err;
    if ((err = fontOutputMillipointSpace(OS::Millipoint(sy), output)) != nullptr)
        return err;
    if ((err = output.str("m\n")) != nullptr)
        return err;

    if ((err = fontOutputMillipointSpace(OS::Millipoint(ex), output)) != nullptr)
        return err;
    if ((err = fontOutputMillipointSpace(OS::Millipoint(ey), output)) != nullptr)
        return err;
    if ((err = output.str("l\nS\n")) != nullptr)
        return err;

    return output_restore(job);
}

void PDFFontChunkPainter::resetCurrentFont()
{
#if PDFTextSpeedUps
    m_job.m_context.current().m_font = 0xffffffffu;
#else
    m_currentPDFFont = 0xffffffffu;
#endif
    m_fontRecord = nullptr;
}

bool PDFFontChunkPainter::changeFont(FontHandle font)
{
#if PDFTextSpeedUps
    GraphicsContext& ctx(m_job.m_context.current());
    if (ctx.m_font == font)
        return false;

    ctx.m_font = font;
    return true;
#else
    if (m_currentPDFFont == font)
        return false;

    m_currentPDFFont = font;
    return true;
#endif
}

MyError PDFFontChunkPainter::selectFont(FontHandle font, const Font::ReadDefn& defn)
{
    PrinterFontResolution resolution = m_job.FontNameEntry().resolveForHandlePDF(font,
                                                                              defn.getIdentifier(),
                                                                            m_job.jobverbose,
                                                                            m_job.jobfontlist,
                                                                            FontNameEntry::Word(PDriverMiscOp_PS_Font),
                                                                            FontNameEntry::Word(PDriverMiscOp_PS_DF));

    MyError err = m_job.m_pdfDocument.registerFont(resolution.name(),
                                                   PDF::FontEncoding_BuiltIn,
                                                   m_fontRecord);
    if (err)
        return err;

    m_fontSize16 = defn.pointsY().raw();
    if (m_fontSize16 <= 0)
        m_fontSize16 = defn.pointsX().raw();
    if (m_fontSize16 <= 0)
        m_fontSize16 = 160;

    return nullptr;
}

MyError PDFFontChunkPainter::fillRect(const Font::Rect& rect, uint32_t bbGGRR00)
{
    MyError err;
    Output& output(m_job.output());

    if ((err = colour_setrealrgb(bbGGRR00, m_job)) != nullptr)
        return err;
#if PSCoordSpeedUps
    if ((err = colour_ensure(m_job)) != nullptr)
        return err;
#endif

    if ((err = ensure_OScoords(output, m_job)) != nullptr)
        return err;

    OS::Millipoint width = rect.x1 - rect.x0;
    OS::Millipoint height = rect.y1 - rect.y0;
    if (width <= 0 || height <= 0)
        return nullptr;

    if ((err = fontOutputMillipointSpace(rect.x0, output)) != nullptr)
        return err;
    if ((err = fontOutputMillipointSpace(rect.y0, output)) != nullptr)
        return err;
    if ((err = fontOutputMillipointSpace(width, output)) != nullptr)
        return err;
    if ((err = fontOutputMillipointSpace(height, output)) != nullptr)
        return err;

    return output.str("re\nf\n");
}

MyError PDFFontChunkPainter::paintText(const FontChunkPaint& paint, uint32_t bbGGRR00)
{
    if (m_fontRecord == nullptr)
        return nullptr;

    MyError err = nullptr;
    Output& output(m_job.output());
    const FontPaintState& fontPaint = paint.state();
    const uint8_t* chunk = paint.chunk();
    uint32_t length = (uint32_t)paint.length();

    if (length == 0)
        return nullptr;

    uint32_t flags = fontPaint.m_flags;

    if ((err = colour_setrealrgb(bbGGRR00, m_job)) != nullptr)
        return err;
#if PSCoordSpeedUps
    if ((err = colour_ensure(m_job)) != nullptr)
        return err;
#endif

    if ((err = ensure_OScoords(output, m_job)) != nullptr)
        return err;

    Font::Point start = paint.position();
    Font::Point end = fontPaint.m_paintEndPosition;
    if ((flags & Font::PaintFlag_Reversed) != 0) {
        start = fontPaint.m_paintEndPosition;
        end = paint.position();
    }

    Font::Offset spaceAdd = fontPaint.m_adjustments.m_spaceAdd;
    Font::Offset charAdd = fontPaint.m_adjustments.m_charAdd;

    int32_t ulposSigned = (int32_t)((int8_t)fontPaint.m_underlinePosition.raw());
    int32_t ulthick = fontPaint.m_underlineThickness.raw();

    int needsPositioning = 0;

    if (spaceAdd.dx != 0 || spaceAdd.dy != 0 || charAdd.dx != 0 || charAdd.dy != 0)
        needsPositioning = 1;

    uint32_t codeunitBytes = fontCodeunitBytes(flags);
    if (codeunitBytes == 0 || (length % codeunitBytes) != 0)
        codeunitBytes = 1;
    uint32_t codeunitCount = length / codeunitBytes;
    if (codeunitCount == 0) {
        codeunitBytes = 1;
        codeunitCount = length;
    }

    const uint8_t* renderChunk = chunk;
    uint8_t* renderAlloc = nullptr;

    if ((flags & Font::PaintFlag_Reversed) != 0 &&
        codeunitCount > 1 &&
        (needsPositioning || codeunitBytes > 1)) {
        if ((err = fontMakeReversedUnits(chunk, length, codeunitBytes, &renderAlloc)) != nullptr)
            return err;
        renderChunk = renderAlloc;
    }

    if ((err = output.str("BT\n/F")) != nullptr)
        goto cleanup;
    if ((err = output.num(m_fontRecord->resourceId())) != nullptr)
        goto cleanup;
    if ((err = output.str(' ')) != nullptr)
        goto cleanup;
    /* The page CTM is still in OS units, so Tf must be output in OS units too. */
    if ((err = output.writeReal(m_fontSize16 * 5, 32)) != nullptr)
        goto cleanup;
    if ((err = output.str(" Tf\n")) != nullptr)
        goto cleanup;

    if (!needsPositioning) {
        if ((flags & Font::PaintFlag_Matrix) != 0) {
            const Draw::Matrix& m = fontPaint.m_matrix;
            Font::Offset offset(m.e.raw() >> 8, m.f.raw() >> 8);
            Font::Point t = start + offset;
            err = fontOutputTextMatrix(m, t, output);
        } else {
            err = fontOutputIdentityTextMatrix(start, output);
        }
        if (err)
            goto cleanup;

        if ((flags & Font::PaintFlag_Reversed) != 0) {
            if (renderAlloc != nullptr) {
                err = output_PSstring(renderChunk, length, output);
            } else {
                err = output_PSstringBackwards(chunk, length, output);
            }
        } else {
            err = output_PSstring(chunk, length, output);
        }
        if (err)
            goto cleanup;

        if ((err = output.str("Tj\n")) != nullptr)
            goto cleanup;
    } else {
        for (uint32_t i = 0; i < codeunitCount; ++i) {
            Font::Offset offset(0, 0);

            if (i != 0) {
                uint32_t prefixLength = i * codeunitBytes;
                err = fontScanStringPrefix(fontPaint.m_font,
                                           renderChunk,
                                           prefixLength,
                                           flags,
                                           spaceAdd,
                                           charAdd,
                                           offset);
                if (err)
                    goto cleanup;
            }

            Font::Point glyphPos = start + offset;

            if ((flags & Font::PaintFlag_Matrix) != 0) {
                const Draw::Matrix& m = fontPaint.m_matrix;
                Font::Offset matrixOffset(m.e.raw() >> 8, m.f.raw() >> 8);

                Font::Point t = glyphPos + matrixOffset;
                err = fontOutputTextMatrix(m, t, output);
            } else {
                err = fontOutputIdentityTextMatrix(glyphPos, output);
            }
            if (err)
                goto cleanup;

            const uint8_t* glyph = renderChunk + (i * codeunitBytes);
            if ((err = fontOutputGlyphTJ(glyph, codeunitBytes, output)) != nullptr)
                goto cleanup;
        }
    }

    if ((err = output.str("ET\n")) != nullptr)
        goto cleanup;

    if (ulthick != 0) {
        err = fontDrawUnderline(output,
                                flags,
                                start.x.raw(),
                                start.y.raw(),
                                end.x.raw(),
                                end.y.raw(),
                                ulposSigned,
                                ulthick,
                                m_job);
        if (err)
            goto cleanup;
    }

cleanup:
    if (renderAlloc != nullptr)
        (void)rma_free(renderAlloc);

    return err;
}
