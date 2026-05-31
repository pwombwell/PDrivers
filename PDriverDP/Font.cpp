#include "kernel.h"
#include "Core/PDriver.h"

#include "Colour.h"
#include "Constants.h"
#include "JobWS.h"
#include "PDriverDP.h"
#include "Private.h"

#include "Core/Colour.h"
#include "Core/Constants.h"
#include "Core/InterceptManager.h"
#include "Core/Workspace.h"

#include "swis.h"

struct DpFontPaintBlock {
    Font::Offset spaceAdd;
    Font::Offset charAdd;
    Font::Rect   rubout;
};

static uint32_t stripType(const JobWS& job)
{
    return job.config().stripType();
}

static bool useTrueColourFontPalette(const JobWS& job)
{
    uint32_t type = stripType(job);
    return type == 4u || type == 5u;
}

static uint32_t pixval256toRgb(uint32_t pixval, const JobWS& job)
{
#if MonoBufferOK
#if NbppBufferOK
    if (job.config().outputBpp() != 8u)
        return pixval;
#else
    if (job.config().use1bpp() != 0u)
        return pixval;
#endif
#endif

    uint32_t low2 = pixval & 0x3u;
    uint32_t red = (pixval & 0x07u) | ((pixval & 0x10u) >> 1);
    uint32_t green = low2 | ((pixval & 0x60u) >> 3);
    uint32_t blue = low2 | ((pixval & 0x88u) >> 3);

    blue = (blue << 4) & 0xF0u;
    return ((blue << 16) | (green << 20) | (red << 12));
}

static MyError fontSetColours(CoreWS& ws)
{
    JobWS& job((JobWS&)ws);

#if MonoBufferOK
#if NbppBufferOK
    if (job.config().outputBpp() != 8u)
#else
    if (job.config().use1bpp() != 0u)
#endif
    {
        ScopedPassthrough scopedPT(ws.m_interceptMgr, Passthrough_ColTrans);
        return _swix(Font_SetFontColours, _INR(0,3),
                     0u, job.m_fontBG, job.m_fontFG, 0u);
    }
#endif

    ScopedPassthrough scopedPT(ws.m_interceptMgr, Passthrough_ColTrans);
    return _swix(Font_SetPalette, _INR(1,6),
                 0u, 0u, 0u, job.m_fontBG, job.m_fontFG, 0u);
}

static MyError fontSetScaleFactor(JobWS& job)
{
    uint32_t scaleX(job.m_prevFontScaleX);
    uint32_t scaleY(job.m_prevFontScaleY);

    MyError err = Font::readScaleFactor(job.m_prevFontScaleX, job.m_prevFontScaleY);
    if (err != nullptr)
        return err;

    scaleY = YScale(180, job);
    scaleY = Divide(72000, scaleY);
    if (scaleY == 0)
        scaleY = 1;

    scaleX = XScale(180, job);
    scaleX = Divide(72000, scaleX);
    if (scaleX == 0)
        scaleX = 1;

    return _swix(Font_SetScaleFactor, _INR(1,2), scaleX, scaleY);
}

static MyError fontRestoreScale(JobWS& job)
{
    return _swix(Font_SetScaleFactor, _INR(1,2),
                 job.m_prevFontScaleX, job.m_prevFontScaleY);
}

static void copyWidthValue(uint8_t*& out,
                           uint32_t widthFlags,
                           int32_t value,
                           bool signedValue)
{
    if (widthFlags == Font::PaintFlag_16bit) {
        uint16_t word = (uint16_t)value;
        out[0] = (uint8_t)(word & 0xFFu);
        out[1] = (uint8_t)(word >> 8);
        out += 2;
        return;
    }

    if (widthFlags == Font::PaintFlag_32bit) {
        uint32_t word = (uint32_t)value;
        out[0] = (uint8_t)(word & 0xFFu);
        out[1] = (uint8_t)((word >> 8) & 0xFFu);
        out[2] = (uint8_t)((word >> 16) & 0xFFu);
        out[3] = (uint8_t)(word >> 24);
        out += 4;
        return;
    }

    if (signedValue)
        *out++ = (uint8_t)(int8_t)value;
    else
        *out++ = (uint8_t)value;
}

MyError font_declare(const char* name, DeclareFont_Flag flags, CoreJobWS& job)
{
    (void)name;
    (void)flags;
    return nullptr;
}

MyError font_fg(uint32_t gcol, CoreJobWS& coreJob)
{
    JobWS& job = toJobWS(coreJob);
    job.m_fontFgGcol = (uint8_t)gcol;
    return font_coloffset((int32_t)job.m_fontColourOffset, coreJob);
}

MyError font_coloffset(int32_t offset, CoreJobWS& coreJob)
{
    JobWS& job = toJobWS(coreJob);
    job.m_fontColourOffset = (uint8_t)offset;

    if (job.m_fontFgGcol == 0xFFu)
        return nullptr;

    uint32_t gcol = (job.m_fontFgGcol + offset) & 15u;
    uint32_t rgb = pixval_lookup(gcol);
    return font_absfg(rgb, job);
}

MyError font_absfg(uint32_t bbGGRR00, CoreJobWS& coreJob)
{
    JobWS& job = toJobWS(coreJob);
    job.m_fontFgGcol = 0xFFu;

    if (useTrueColourFontPalette(job)) {
        job.m_fontFG = bbGGRR00;
        return nullptr;
    }

    uint32_t pixval = colour_rgbtopixval(bbGGRR00, job);
    job.m_fontFG = pixval256toRgb(pixval, job);
    return nullptr;
}

MyError font_bg(uint32_t gcol, CoreJobWS& coreJob)
{
    uint32_t rgb = pixval_lookup(gcol);
    return font_absbg(rgb, coreJob);
}

MyError font_absbg(uint32_t bbGGRR00, CoreJobWS& coreJob)
{
    JobWS& job = (JobWS&)coreJob;

    if (useTrueColourFontPalette(job)) {
        job.m_fontBG = bbGGRR00;
        return nullptr;
    }

    uint32_t pixval = colour_rgbtopixval(bbGGRR00, job);
    job.m_fontBG = pixval256toRgb(pixval, job);
    return nullptr;
}

MyError font_savecolours(CoreJobWS& coreJob)
{
    (void)coreJob;
    return nullptr;
}

MyError font_stringstart(uint32_t& maxCharsMinus1,
                         uint32_t& passes,
                         CoreJobWS& coreJob)
{
    CoreWS& ws = CoreWS::instance();
    JobWS& jobWS = toJobWS(coreJob);

    Passthrough savedPassthrough = ws.m_interceptMgr.getPassthrough();
    jobWS.m_stashedPassthrough = (uint8_t)savedPassthrough;
    ws.m_interceptMgr.setPassthrough(Passthrough(savedPassthrough | Passthrough_Wrch | Passthrough_Draw));

    uint32_t widthFlags = ws.fontPaint().m_initialFlags &
                          (Font::PaintFlag_16bit | Font::PaintFlag_32bit);
    uint32_t available = jobWS.fontWS().chunkBufferBytes();
    if (widthFlags == Font::PaintFlag_16bit)
        available -= 8u;
    else if (widthFlags == Font::PaintFlag_32bit)
        available -= 16u;
    else
        available -= 4u;

    maxCharsMinus1 = available;
    passes = 1u;

    return nullptr;
}

MyError font_passstart(uint32_t pass, CoreJobWS& coreJob)
{
    (void)pass;
    (void)coreJob;
    return nullptr;
}

MyError font_paintchunk(const uint8_t* chunk,
                        const Point<OS::Millipoint>& position,
                        uint32_t length,
                        uint32_t pass,
                        CoreJobWS& coreJob)
{
    CoreWS& ws = CoreWS::instance();
    JobWS& job = toJobWS(coreJob);
    (void)pass;

    if ((coreJob.disabled & Disabled_NullClip) != 0u)
        return nullptr;

    FontPaintState& fontPaint = ws.fontPaint();
    MyError err = fontSetScaleFactor(job);
    if (err != nullptr)
        return err;

    err = fontSetColours(ws);
    if (err != nullptr) {
        (void)fontRestoreScale(job);
        return err;
    }

    uint8_t* slavedFonts = job.slavedFonts();
    uint32_t masterFont = fontPaint.m_font & 0xFFu;
    FontHandle slaveFont = slavedFonts[masterFont];
    if (slaveFont == 0u) {
        uint8_t* buffer = job.fontWS().chunkBuffer();
        int32_t xSize = 0;
        int32_t ySize = 0;
        err = _swix(Font_ReadDefn,
                    _IN(0) | _IN(1) | _IN(3) | _OUT(2) | _OUT(3),
                    fontPaint.m_font,
                    buffer,
                    0x4C4C5546u,
                    &xSize,
                    &ySize);
        if (err != nullptr) {
            (void)fontRestoreScale(job);
            return err;
        }

        uint32_t foundFont = 0;
        err = _swix(Font_FindFont,
                    _INR(1,5) | _OUT(0),
                    buffer, xSize, ySize, 0u, 0u,
                    &foundFont);
        if (err != nullptr) {
            (void)fontRestoreScale(job);
            return err;
        }

        slaveFont = foundFont;
        slavedFonts[masterFont] = (uint8_t)foundFont;
    }

    err = _swix(Font_SetFont, _IN(0), slaveFont);
    if (err != nullptr) {
        (void)fontRestoreScale(job);
        return err;
    }

    uint32_t widthFlags = fontPaint.m_flags &
                          (Font::PaintFlag_16bit | Font::PaintFlag_32bit);
    uint8_t* buffer = job.fontWS().chunkBuffer();

    copyWidthValue(buffer, widthFlags, 25, false);
    copyWidthValue(buffer, widthFlags, fontPaint.m_underlinePosition.raw(), true);
    copyWidthValue(buffer, widthFlags, fontPaint.m_underlineThickness.raw(), false);

    memcpy(buffer, chunk, length);
    buffer += length;

    if (widthFlags == Font::PaintFlag_16bit) {
        buffer[0] = 0;
        buffer[1] = 0;
    } else if (widthFlags == Font::PaintFlag_32bit) {
        buffer[0] = 0;
        buffer[1] = 0;
        buffer[2] = 0;
        buffer[3] = 0;
    } else {
        buffer[0] = 0;
    }

    Offset<Draw::Unit> origin(job.output().currentOffset());
    Offset<OS::Unit> originPix(toOSUnit(origin));

    alignToPixel(originPix);

    Offset<OS::Millipoint> originMS;

    if ((err = _swix(Font_Converttopoints, _INR(1,2) | _OUTR(1,2),
                     originPix.dx, originPix.dy, &originMS.dx, &originMS.dy)) != nullptr) {
        (void)fontRestoreScale(job);
        return err;
    }

    DpFontPaintBlock& paintBlock = *(DpFontPaintBlock*)(void *)job.fontCoordBlockWords();

    paintBlock.spaceAdd = fontPaint.m_adjustments.m_spaceAdd;
    paintBlock.charAdd = fontPaint.m_adjustments.m_charAdd;

    paintBlock.rubout.x0 = fontPaint.m_ruboutStart - originMS.dx;
    paintBlock.rubout.y0 = fontPaint.m_ruboutBox.y0 - originMS.dy;
    paintBlock.rubout.x1 = fontPaint.m_ruboutEnd - originMS.dx;
    paintBlock.rubout.y1 = fontPaint.m_ruboutBox.y1 - originMS.dy;

    uint32_t flags = fontPaint.m_flags | Font::PaintFlag_CoordsBlk;
    flags &= ~(Font::PaintFlag_Justify | Font::PaintFlag_Mpoint);

    err = _swix(Font_Paint,
                _INR(0,7),
                slaveFont,
                job.fontWS().chunkBuffer(),
                flags,
                position.x - originMS.dx,
                position.y - originMS.dy,
                paintBlock,
                &fontPaint.m_matrix,
                length);

    MyError restoreErr = fontRestoreScale(job);
    return err != nullptr ? err : restoreErr;
}

MyError font_passend(uint32_t pass, CoreJobWS& coreJob)
{
    (void)pass;
    (void)coreJob;
    return nullptr;
}

MyError font_stringend(CoreJobWS& coreJob)
{
    CoreWS& ws = CoreWS::instance();
    JobWS& jobWS = toJobWS(coreJob);
    ws.m_interceptMgr.setPassthrough((Passthrough)jobWS.m_stashedPassthrough);
    return nullptr;
}

MyError font_losefont(FontHandle handle, CoreJobWS& coreJob)
{
    MyError err;
    JobWS& job = toJobWS(coreJob);

    uint8_t* slavedFonts = job.slavedFonts();

    if (handle != 0) {
        FontHandle slave = slavedFonts[handle];

        if (slave != 0)
            slavedFonts[handle] = 0;
            return _swix(Font_LoseFont, _IN(0), slave);

        return nullptr;
    }

    // No handle given - free all fonts.
    for (uint32_t index = 255; index != 0u; --index) {
        FontHandle slave = slavedFonts[index];

        if (slave != 0) {
            slavedFonts[index] = 0;

            if ((err = _swix(Font_LoseFont, _IN(0), slave)) != nullptr)
                return err;
        }
    }

    return nullptr;
}
