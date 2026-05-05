#include "InterceptFont.h"

#include "Constants.h"
#include "Device.h"
#include "FontEncodingCache.h"
#include "FontPaint.h"
#include "FontPaintReader.h"
#include "FontPaintStream.h"
#include "InterceptManager.h"
#include "Job.h"
#include "MsgCode.h"
#include "OS.h"
#include "PDriver.h"
#include "Workspace.h"

#include "RLib/OS/Error.h"
#include "RLibX/Font/Identifier.h"
#include "RLibX/Font/ReadDefn.h"

#include <stdint.h>
#include <string.h>

enum {
    FontReason_LoseFont = 0x02u,
    FontReason_Paint = 0x06u,
    FontReason_SetFontColours = 0x12u,
    FontReason_SetPalette = 0x13u
};

static MyError fontswi_paint_scanstring(FontPaintFlag flags,
                                        const uint8_t* string,
                                        uint32_t length,
                                        FontOffset& scanOffset,
                                        uint32_t& split_count)
{
    uint32_t scan_flags = 0x140120;
#if UCSText
    scan_flags |= flags & (FontPaintFlag_16bit | FontPaintFlag_32bit);
#endif
    if ((flags & FontPaintFlag_Kern) != 0u)
        scan_flags |= (1u << 9);

    Font::ScanStringBlock scanBlock;
    scanBlock.split = ' ';
    MyError err = Font::scanString(0, string, scan_flags, scanOffset,
                                   scanBlock, length, &split_count);

    // Use the 'string width' (offset after printing) rather than the bounding box
    // when calculating the justification.  This is because the bounding box returned
    // from Font_ScanString does NOT include any embedded movement sequences at the
    // start or end of the string (it *does* count ones in the middle).  The string
    // width includes ALL the movement caused by escape sequences.

    return err;
}

MyError InterceptFont::setColours(uint32_t bg,
                                  uint32_t fg,
                                  uint32_t offset)
{
    MyError err = nullptr;

#if !FontColourFixes
    if (bg != -1) {
        bg &= 0xF;
    }
    if (fg != -1) {
        fg &= 0xF;
    }
#endif

    int32_t lgbpp = m_job.screenVars.lgbpp;
    int32_t *palette = m_job.fontpalette;

    int is_256 = (lgbpp == 3);
#if Medusa
    if (!is_256 && (lgbpp == 4 || lgbpp == 5)) {
        is_256 = 1;
    }
#endif

    if (is_256) {
#if FontColourFixes
        if (fg == (int32_t)TopBit) {
            goto setcolours_offset;
        }
#else
        if (fg == -1) {
            goto setcolours_offset;
        }
#endif

        int32_t entry = palette[fg];
        if (entry == -1) {
#if FontColourFixes
            if (bg != TopBit) {
                err = font_bg(bg, m_job);
            }
#else
            if (bg != -1) {
                err = font_bg(bg, m_job);
            }
#endif
            if (!err) {
                err = font_fg(fg, m_job);
            }
            goto setcolours_offset;
        }

        err = font_absfg((uint32_t)entry, m_job);
        if (!err) {
            int32_t bg_entry = palette[16];
            err = font_absbg((uint32_t)bg_entry, m_job);
        }
#if !FontColourFixes
        if (!err) {
            offset = palette[32];
        }
#endif
        goto setcolours_offset;
    }

    /* Non-256 colour mode handling. */
#if FontColourFixes
    if (bg != TopBit) {
#else
    if (bg != -1) {
#endif
        int32_t entry = palette[bg];
        if (entry != -1) {
            err = font_absbg(entry, m_job);
        } else {
            err = font_bg(bg, m_job);
        }
    }

    if (!err) {
#if FontColourFixes
        if (fg != TopBit) {
#else
        if (fg != -1) {
#endif
            int32_t use_offset = offset;
#if FontColourFixes
            if (use_offset == TopBit) {
                use_offset = m_job.fontcoloffset;
            }
#else
            if (use_offset == -1) {
                use_offset = m_job.fontcoloffset;
            }
#endif
            int32_t index = (fg + use_offset) & 0xf;
            int32_t entry = palette[index];
            if (entry != -1) {
                err = font_absfg(entry, m_job);
            } else {
                err = font_fg(fg, m_job);
            }
        }
    }

setcolours_offset:
    if (err)
        return err;

#if FontColourFixes
    if (offset != TopBit) {
        m_job.fontcoloffset = offset;
        err = font_coloffset(offset, m_job);
    }
#else
    if (offset != -1) {
        m_job.fontcoloffset = (uint32_t)offset;
        err = font_coloffset(offset, m_job);
    }
#endif

    return err;
}

static uint32_t fontswi_setpalette_interpolate(uint32_t divisor,
                                               uint32_t bg_mult,
                                               uint32_t fg_mult,
                                               uint32_t bg_component,
                                               uint32_t fg_component) {
    uint32_t accum = bg_mult * bg_component;
    accum += fg_mult * fg_component;
    accum += divisor / 2u;
    return accum / divisor;
}

MyError InterceptFont::setPalette(uint32_t bg_index,
                                  uint32_t fg_index,
                                  int32_t offset,
                                  uint32_t bg_rgb,
                                  uint32_t fg_rgb)
{
    MyError err = nullptr;

    bg_index &= 0xF;
    fg_index &= 0xF;
    bg_rgb &= ~0xFFu;
    fg_rgb &= ~0xFFu;

    int32_t lgbpp = m_job.screenVars.lgbpp;
    int32_t *palette = m_job.fontpalette;

    if (lgbpp >= 3) {
        palette[fg_index] = (int32_t)fg_rgb;
        palette[16] = (int32_t)bg_rgb;
#if FontColourFixes
        m_job.fontcoloffset = (uint32_t)offset;
        err = font_coloffset(offset, m_job);
#else
        palette[32] = offset;
#endif
        return err;
    }

    palette[bg_index] = (int32_t)bg_rgb;

    int32_t steps = offset;
    if (steps < 0)
        steps = -steps;

    uint32_t divisor = (uint32_t)steps + 1u;
    uint32_t bg_mult = (uint32_t)steps;
    uint32_t fg_mult = 1u;

    int32_t index = fg_index;
    while ((int32_t)bg_mult >= 0) {
        uint32_t blue = fontswi_setpalette_interpolate(divisor, bg_mult, fg_mult,
                                                       (bg_rgb >> 24) & 0xFFu,
                                                       (fg_rgb >> 24) & 0xFFu);
        uint32_t green = fontswi_setpalette_interpolate(divisor, bg_mult, fg_mult,
                                                        (bg_rgb >> 16) & 0xFFu,
                                                        (fg_rgb >> 16) & 0xFFu);
        uint32_t red = fontswi_setpalette_interpolate(divisor, bg_mult, fg_mult,
                                                      (bg_rgb >> 8) & 0xFFu,
                                                      (fg_rgb >> 8) & 0xFFu);

        palette[index & 0xF] = (int32_t)((blue << 24) | (green << 16) | (red << 8));

        if (offset >= 0)
            index += 1;
        else
            index -= 1;

        fg_mult += 1u;
        if (bg_mult == 0)
            break;

        bg_mult -= 1u;
    }

    return nullptr;
}

MyError InterceptFont::finish(MyError err)
{
    MyError escape_err = m_ws.m_escapeState.disableAndCheck();
    if (!err)
        err = escape_err;

    if (err)
        err = m_job.makePersistentError(err);

    return err;
}

MyError InterceptFont::setFontColours(int32_t bg,
                                         int32_t fg,
                                         int32_t offset)
{
    MyError err;
    if ((err = m_job.checkPersistentError()) != nullptr)
        return err;

    if ((err = m_ws.m_escapeState.enable()) == nullptr)
        err = vdu5_flush(m_job);

#if FontColourFixes
    if (!err) {
        bg &= 0xF;
        fg &= 0xF;
    }
#endif
    if (!err) {
        err = setColours(bg, fg, offset);
    }

    return finish(err);
}

MyError InterceptFont::setPaletteSWI(int32_t bg_index,
                                     int32_t fg_index,
                                     int32_t offset,
                                     uint32_t bg_rgb,
                                     uint32_t fg_rgb)
{
    MyError err;
    if ((err = m_job.checkPersistentError()) != nullptr)
        return err;

    if ((err = m_ws.m_escapeState.enable()) == nullptr)
        err = vdu5_flush(m_job);

    if (!err)
        err = setPalette(bg_index, fg_index, offset, bg_rgb, fg_rgb);

    return finish(err);
}

MyError InterceptFont::loseFont(FontHandle font, CoreWS& ws)
{
    MyError err = nullptr;
    JobManager& jobMgr(ws.jobMgr());

    CoreJobWS* job = jobMgr.firstJob();

    while (job) {
        if ((err = font_losefont(font, *job)) != nullptr)
            break;

        job = jobMgr.nextJob(job);
    }

    debugLog("loseFont err:%s", err ? err.message() : "<null>");
    return err;
}

MyError InterceptFont::paintNextPosition(const uint8_t *chunk, uint32_t length)
{
    FontPaintState& fontPaint = m_ws.fontPaint();
    uint32_t handle = fontPaint.m_font;

    FontPaintFlag flags = fontPaint.m_flags;
#if UCSText
    flags &= FontPaintFlag_Reversed | FontPaintFlag_Kern |
             FontPaintFlag_Matrix | FontPaintFlag_16bit | FontPaintFlag_32bit;
#else
    flags &= FontPaintFlag_Reversed | FontPaintFlag_Kern | FontPaintFlag_Matrix;
#endif
    flags |= FontPaintFlag_CoordsBlk | FontPaintFlag_UseHandle | FontPaintFlag_Length;

    Font::ScanStringBlock coordBlock;
    coordBlock.space = fontPaint.m_adjustments.m_spaceAdd;
    coordBlock.letter = fontPaint.m_adjustments.m_charAdd;
    coordBlock.split = -1;

    Offset<OS::Millipoint> offset(BigNum, BigNum);

    debugLog("paint nextposition scanstring font:%u flags:%08x length:%u",
             handle, flags, length);

    MyError err = Font::scanString((int32_t)handle, chunk, flags,
                                   offset, coordBlock,
                                   length, nullptr);

    debugLog("paint nextposition scanstring result font:%u err:%p x:%d y:%d",
             handle, err.raw(), offset.dx, offset.dy);

    if (err)
        return err;

    fontPaint.setPaintEndOffset(offset);
    return nullptr;
}

MyError InterceptFont::paintPrintable(FontPaintStream& stream,
                                      bool isUtf8,
                                      uint32_t pass)
{
    FontPaintState& fontPaint = m_ws.fontPaint();
    const uint8_t* start = stream.current();

    fontPaint.setCurrentFlagsFromMatrix();

    uint32_t length = stream.printableLength(isUtf8);
    MyError err = nullptr;

    if ((err = paintNextPosition(start, length)) != nullptr)
        return err;

    FontPaintFlag initflags = fontPaint.m_initialFlags;
    fontPaint.clipRuboutEndToPaintEnd(initflags);

    if ((err = font_paintchunk(start, fontPaint.m_paintPosition,
                               length, pass, m_job)) != nullptr)
        return err;

    fontPaint.finishPrintableChunk(initflags);

    stream.advance(length);
    return nullptr;
}

// `fontswi_paint_nocoordsblock`
MyError InterceptFont::paintNoCoordsBlock(FontPaintFlag flags, OS::Millipoint x)
{
    // Now we need to handle the case of there being no coordinate block.  This is
    // simply a case of converting the old style call to be a new style one, ie.
    // getting and converting the justification region into coordinate offsets
    // and then reading the four points for the rubout box and then stashing them
    // in the block in 1/72000" format.
    MyError err;
    FontPaintState& fontPaint = m_ws.fontPaint();

    if (!(flags & FontPaintFlag_Justify)) {
        fontPaint.setSpaceAdjustment(0);
    } else {
        // Get the justification final point.
        FontPoint justify;

        if ((err = Font::convertToPoints(m_job.oldpoint, justify)) != nullptr)
            return err;

        // We now calculate the justification offsets within the line, this is quite
        // simple as we already know the number of space characters and the maximum
        // offsets within the line.

        OS::Millipoint remaining = justify.x - x - m_ws.fontPaint().m_maxHorizontalOffset;
        OS::Millipoint spaceAdd = 0;

        if (m_ws.fontPaint().m_spaceCount != 0) {
            OS::Millipoint absRemaining = remaining < 0 ? -remaining : remaining;
            spaceAdd = absRemaining / m_ws.fontPaint().m_spaceCount;
            if (remaining < 0)
                spaceAdd = -spaceAdd;
        }

        fontPaint.setSpaceAdjustment(spaceAdd);
    }

    if (!!(flags & FontPaintFlag_Rubout)) {
        FontPoint topRight;
        if ((err = Font::convertToPoints(m_job.olderpoint, topRight)) != nullptr)
            return err;

        FontPoint bottomLeft;
        if ((err = Font::convertToPoints(m_job.olderpoint, bottomLeft)) != nullptr)
            return err;

        m_ws.fontPaint().setRuboutBox(Rect<OS::Millipoint>(bottomLeft, topRight));
    }

    return nullptr;
}

MyError InterceptFont::finishPaint(MyError err)
{
    debugLog("paint finishPaint entry err:%p", err.raw());
    MyError adj_err = m_ws.m_interceptMgr.change(Intercept_All);
    debugLog("paint finishPaint change err:%p", adj_err.raw());
    if (!err)
        err = adj_err;

    return finish(err);
}

MyError InterceptFont::paint()
{
    // R0   Font handle (if applicable)
    // R1   Pointer to string (may include control characters)
    // R2   Flags
    // R3   X
    // R4   Y
    // R5   Pointer to block (if applicable)
    // R6   Pointer to transformation matrix (if applicable)
    // R7   Length (if applicable)
    MyError err;

    FontHandle font = m_regs.as<FontHandle>(0);
    const uint8_t* string = m_regs.as<const uint8_t*>(1);
    FontPaintFlag flags = m_regs.as<FontPaintFlag>(2);
    Point<int32_t> pos(m_regs.as<int32_t>(3), m_regs.as<int32_t>(4)); // unknown unit
    const FontPaintCoords* coordBlock = m_regs.as<const FontPaintCoords*>(5);
    const int32_t* matrix = m_regs.as<const int32_t*>(6);
    uint32_t length = m_regs[7];

#if CaptureTrace
    (void)capturetrace_log(m_job,
                           "trace_fontpaint_entry %u %u %ld %ld %u %u %u",
                           (unsigned)font,
                           (unsigned)flags,
                           (long)pos.x,
                           (long)pos.y,
                           (unsigned)(uintptr_t)coordBlock,
                           (unsigned)(uintptr_t)matrix,
                           (unsigned)length);
    if (coordBlock != nullptr) {
        (void)capturetrace_log(m_job,
                               "trace_fontpaint_coords %ld %ld %ld %ld "
                               "%ld %ld %ld %ld",
                               (long)coordBlock->m_adjustments.m_spaceAdd.dx.raw(),
                               (long)coordBlock->m_adjustments.m_spaceAdd.dy.raw(),
                               (long)coordBlock->m_adjustments.m_charAdd.dx.raw(),
                               (long)coordBlock->m_adjustments.m_charAdd.dy.raw(),
                               (long)coordBlock->m_ruboutBox.x0.raw(),
                               (long)coordBlock->m_ruboutBox.y0.raw(),
                               (long)coordBlock->m_ruboutBox.x1.raw(),
                               (long)coordBlock->m_ruboutBox.y1.raw());
    }
    if (matrix != nullptr) {
        (void)capturetrace_log(m_job,
                               "trace_fontpaint_matrix %ld %ld %ld %ld %ld %ld",
                               (long)matrix[0],
                               (long)matrix[1],
                               (long)matrix[2],
                               (long)matrix[3],
                               (long)matrix[4],
                               (long)matrix[5]);
    }
#endif

    if ((err = m_job.checkPersistentError()) != nullptr)
        return err;

    // Match FontSWI.s: coordinates without FontPaintFlag_Mpoint are already
    // millipoints; coordinates with it set are OS units to convert.
    FontPoint position;
    if ((flags & FontPaintFlag_Mpoint) != 0) {
        Point<OS::Unit> posOS;
        posOS.x = OS::Unit(pos.x);
        posOS.y = OS::Unit(pos.y);

        if ((err = Font::convertToPoints(posOS, position)) != nullptr)
            return err;
    } else {
        position = Point<OS::Millipoint>(pos.x, pos.y);
    }

#if CaptureTrace
    (void)capturetrace_log(m_job,
                           "trace_fontpaint_position_mp %ld %ld",
                           (long)position.x.raw(),
                           (long)position.y.raw());
#endif

    return paint(font, string, flags, position, coordBlock, matrix, length);
}

MyError InterceptFont::paint(FontHandle font,
                             const uint8_t* string,
                             FontPaintFlag flags,
                             FontPoint position,
                             const FontPaintCoords* coordBlock,
                             const int32_t* matrix,
                             uint32_t length)
{
    MyError err;
    FontPaintState& fontPaint = m_ws.fontPaint();

    FontOffset scanOffset(BigNum, BigNum);
    uint32_t split_count = 0;
    const uint8_t* string_end = nullptr;
    FontOffset user_offset(0, 0);
    FontHandle font_handle = 0;
    FontPaintFlag initflags = FontPaintFlag_None;

    err = m_ws.m_escapeState.enable();
    if (!err)
        err = vdu5_flush(m_job);
    if (!err)
        err = m_ws.m_interceptMgr.change(Intercept_All & ~Intercept_Font);
    if (err)
        return finishPaint(err);

    debugLog("paint before initial scanstring font:%u flags:%08x length:%u",
             font, flags, length);
    err = fontswi_paint_scanstring(flags, string, length,
                                   scanOffset, split_count);
    debugLog("paint after initial scanstring err:%p scan:%d,%d splits:%u",
             err.raw(), scanOffset.dx.raw(), scanOffset.dy.raw(), split_count);

    if (err)
        return finishPaint(err);

    fontPaint.setScanResults(scanOffset.dx, scanOffset.dy, split_count);

    if ((flags & FontPaintFlag_CoordsBlk) != 0u) {
        // A coordinate block has been specified, we must copy the relevant areas of
        // this away into our own workspace assuming that all values are in millipoints
        // as 1/256 of an OS unit sequences are not currently supported by the Font Manager.
        fontPaint.setAdjustments(coordBlock->m_adjustments);

        if ((flags & FontPaintFlag_Rubout) != 0u)
            fontPaint.setRuboutBox(coordBlock->m_ruboutBox);
    } else {
        if ((err = paintNoCoordsBlock(flags, position.x)) != nullptr)
            return finishPaint(err);
    }

    if ((flags & FontPaintFlag_Matrix) == 0u || matrix == nullptr) {
        fontPaint.setInitialMatrix(nullptr);
    } else {
        fontPaint.setInitialMatrix(matrix);
    }

    if ((flags & FontPaintFlag_Length) != 0u) {
        string_end = string + length;
    }

    if ((err = Font::convertToPoints(m_job.usersoffset, user_offset)) != nullptr)
        return finishPaint(err);

    font_handle = !!(flags & FontPaintFlag_UseHandle) ? font : 0;
    if (font_handle == 0) {
        uint32_t r0 = 0;
        uint32_t r1 = 0;
        uint32_t r2 = 0;
        uint32_t r3 = 0;
        err = XFont_CurrentFont(r0, r1, r2, r3);
        if (err) {
            return finishPaint(err);
        }
        font_handle = r0;
    }

    initflags = flags & ~(FontPaintFlag_CoordsBlk |
                          FontPaintFlag_Matrix |
                          FontPaintFlag_Length |
                          FontPaintFlag_UseHandle |
                          FontPaintFlag_Mpoint);
    position -= user_offset;

    fontPaint.setInitialState(font_handle, string, initflags, position);
    fontPaint.m_ruboutBox.offsetBy(-user_offset);

    err = font_savecolours(m_job);
    if (!err) {
        uint32_t max_chars_minus1 = 0;
        uint32_t passes = 0;

        debugLog("paint entry r0/font:%u flags:%08x usehandle:%u",
                 font, flags, !!(flags & FontPaintFlag_UseHandle));

        debugLog("resolved font_handle:%u current before paint", font_handle);

        err = font_stringstart(&max_chars_minus1, &passes, m_job);
        (void)max_chars_minus1;
        if (!err) {
            for (uint32_t pass = passes; pass > 0; --pass) {
                fontPaint.beginPass();

                err = font_passstart(pass, m_job);
                if (err)
                    break;

                FontPaintReader reader(string, fontPaint.m_initialFlags, string_end);
                while (!reader.atEnd()) {
                    FontPaintReader next(reader);
                    uint32_t c = 0;
                    err = next.readNextChar(fontPaint.m_font, c);
                    if (err != nullptr)
                        break;

                    if (c >= 32) {
                        err = paintPrintable(reader.stream(), next.isUtf8(), pass);
                        if (err != nullptr)
                            break;
                        continue;
                    }

                    reader = next;
                    FontPaintStream& stream = reader.stream();

                    switch (c) {
                    case FontPaintControl_End:
                    case FontPaintControl_LineFeed:
                    case FontPaintControl_CarriageReturn:
                        goto fontswi_paint_passdone;

                    case FontPaintControl_HorizontalMove:
                    case FontPaintControl_VerticalMove: {
                        OS::Millipoint delta = stream.readDelta();

                        if (c == FontPaintControl_VerticalMove)
                            fontPaint.m_paintPosition += FontOffset(0, delta);
                        else
                            fontPaint.m_paintPosition += FontOffset(delta, 0);
                        break;
                    }

                    case FontPaintControl_OneColour: {
                        uint32_t colour = stream.readUnsigned();
#if FontColourFixes
                        uint32_t bg = TopBit;
                        uint32_t fg = TopBit;
                        uint32_t offset = TopBit;
#else
                        uint32_t bg = 0xffffffffu;
                        uint32_t fg = 0xffffffffu;
                        uint32_t offset = 0xffffffffu;
#endif
                        if (colour < 128)
                            fg = colour & 0xf;
                        else
                            bg = colour & 0xf;

                        if ((err = setColours(bg, fg, offset)) != nullptr)
                            break;

                        break;
                    }

                    case FontPaintControl_AllColours: {
                        int32_t bg = (int32_t)stream.readUnsigned();
                        int32_t fg = (int32_t)stream.readUnsigned();
                        int32_t offset = (int32_t)stream.readUnsigned();
                        bg &= 0xF;
                        fg &= 0xF;
#if FontColourFixes
                        offset = (int32_t)((int8_t)offset);
#else
                        offset &= 0xF;
#endif
                        err = setColours(bg, fg, offset);
                        break;
                    }

                    case FontPaintControl_AbsoluteColours: {
                        uint32_t bgRgb = stream.readColour();
                        if ((err = font_absbg(bgRgb, m_job)) != nullptr)
                            break;

                        uint32_t fgRgb = stream.readColour();
                        if ((err = font_absfg(fgRgb, m_job)) != nullptr)
                            break;

                        int32_t offset = (int32_t)stream.readUnsigned();
#if FontColourFixes
                        offset = (int32_t)((int8_t)offset);
#endif
                        m_job.fontcoloffset = (uint32_t)offset;
                        if ((err = font_coloffset(offset, m_job)) != nullptr)

                            break;

                        break;
                    }

                    case FontPaintControl_Comment:
                        for (;;) {
                            uint32_t val = stream.readUnsigned();
                            if (val < 32u) {
                                break;
                            }
                        }
                        break;

                    case FontPaintControl_Underline: {
                        int32_t pos = stream.readSigned();
                        uint32_t thick = stream.readUnsigned();
                        fontPaint.setUnderline(Base::Fixed<8>::fromRaw(pos),
                                               Base::Fixed<8>::fromRaw(thick));
                        break;
                    }

                    case FontPaintControl_SetFont: {
                        uint32_t newFont = stream.readUnsigned();
                        fontPaint.setCurrentFont(newFont);
                        break;
                    }

                    case FontPaintControl_Matrix4:
                    case FontPaintControl_Matrix6:
                        fontPaint.setCurrentMatrix(stream.readMatrix(c == FontPaintControl_Matrix6));
                        break;

                    default:
                        err = m_ws.messages.lookupError(ErrorBlock_PrintCantThisFontPaint);
                        break;
                    }

                    if (err != nullptr)
                        break;
                }

fontswi_paint_passdone:
                {
                    MyError pass_err = font_passend(pass, m_job);
                    if (!err && pass_err) {
                        err = pass_err;
                    }
                }
                if (err) {
                    break;
                }
            }

            if (!err) {
                err = font_stringend(m_job);
            }
        }
    }

    {
        debugLog("paint restore font %u initial %u", fontPaint.m_font, font_handle);

        MyError set_err = XFont_SetFont(fontPaint.m_font);
        debugLog("paint restore setfont font:%u err:%p",

                 fontPaint.m_font, set_err.raw());


        if (!err)
            err = set_err;
    }

    return finishPaint(err);
}

MyError InterceptFont::intercept(OS::Regs& regs, CoreWS& ws)
{
    if (ws.m_countingPass)
        return nullptr;

    MyError err = nullptr;
    CoreJobWS* job = ws.currentJob();
    uint32_t reason = regs[8];

    switch (reason) {
        case FontReason_Paint:
            if (job != nullptr) {
                InterceptFont self(regs, ws, *job);
                err = self.paint();
            }
            break;

        case FontReason_LoseFont:
            err = loseFont(regs[0], ws);
            break;

        case FontReason_SetPalette:
            if (job != nullptr) {
                InterceptFont self(regs, ws, *job);
                err = self.setPaletteSWI((int32_t)regs[1],
                                         (int32_t)regs[2],
                                         (int32_t)regs[3],
                                         regs[4],
                                         regs[5]);
            }
            break;

        case FontReason_SetFontColours:
            if (job != nullptr) {
                InterceptFont self(regs, ws, *job);
                err = self.setFontColours((int32_t)regs[1],
                                          (int32_t)regs[2],
                                          (int32_t)regs[3]);
            }
            break;

        default:
            err = nullptr;
            break;
    }

    return err;
}
