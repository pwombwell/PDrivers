#include "PDriver.h"
#include "InterceptColTrans.h"

#include "Constants.h"
#include "Device.h"
#include "InterceptManager.h"
#include "Job.h"
#include "MsgCode.h"
#include "OS.h"
#include "Workspace.h"
#include "cmhg.h"

#include "RLib/OS/Vector.h"
#include "RLibX/Utils/ScopedPtr.h"

#include "swis.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

VectorReturn InterceptColTrans::pass()
{
    MyError err = m_ws.m_escapeState.disableAndCheck();
    if (err)
        return m_job.makePersistentError(err);

    return VectorReturn::pass();
}

#if Medusa
static MyError getSpriteMode(Sprite::Area* spriteArea,
                             uintptr_t nameOrPtr,
                             uint32_t flags,
                             OS::Mode& mode)
{
    Sprite::Selector sprite;
    Sprite::Info info;
    
    // flags: Bit 0: nameOrPtr is sprite, else name
    if (!!(flags & 1))
        sprite = Sprite::Selector(spriteArea, (Sprite::Header*)nameOrPtr);
    else
        sprite = Sprite::Selector(spriteArea, (const char*)nameOrPtr);

    MyError err = info.get(sprite);
    if (err)
        return err;

    mode = info.mode();

    return nullptr;
}

static bool isSpriteAreaPtr(uintptr_t spriteOrMode)
{
    // https://www.riscosopen.org/wiki/documentation/show/ColourTrans%20mode%20identification
    // In:  R0 = mode number, OR
    //      R0 = sprite type, OR
    //      R0 -> mode selector, OR
    //      R0 -> sprite area
    //      R1 -> sprite name, or sprite
    //      R5: Bit 0: R1 -> sprite, else name (only significant when R0 - >sprite area)
    //
    // Out: true if most likely a sprite pointer block. Caller should ensure
    //      any subsequent call to OS_SpriteOp returns any error flag.
        
    // Is this -1 (current mode)?
    if (spriteOrMode == uintptr_t(-1))
        return false;
    
    // Is it an old-fashioned mode number?
    if ((spriteOrMode & ~0xffu) == 0u) 
        return false;

#if Medusa
    // Is it a Sprite::Type? (bit 0 is set for a Type).
    if (!!(spriteOrMode & 1)) 
        return false;

    // Bodge for !Draw, which hopes to fool the system into thinking
    // it has passed a sprite area in by setting R0=256
    if (spriteOrMode == 256) 
        return true;
    
    // Bodge for the Wimp
    if (spriteOrMode == 0x8000) 
        return true;
#endif

    // It's not a mode number, so most likely a pointer. It could be either a
    // mode selector ptr or a sprite area ptr. Determine which:
    //  - Get first word of sprite area/mode selector.
    //  - is bit 0 set?
    //  - if so, it's a mode selector which OS_ReadModeVariable can handle

    uint32_t firstWord = *(const uint32_t*)spriteOrMode;
    if ((firstWord & 1u) == 0u)
        return true; // Most likely sprite area pointer.

    // otherwise, mode selector (treat like mode).
    return false;
}

// `intcolour_16or32`
static MyError intcolour_16or32(uintptr_t spriteOrMode,
                                uintptr_t r1,
                                uint32_t r5,
                                bool& is16or32)
{
    MyError err;
    OS::Mode mode(spriteOrMode);

    if (isSpriteAreaPtr(spriteOrMode)) {
        // Get OS::Mode from sprite, returning error if pointing to nonsense.
        if ((err = getSpriteMode((Sprite::Area*)spriteOrMode,
                                 r1, r5, mode)) != nullptr)
            return err;

        // mode has been replaced by the sprite's mode.
    }

    uint32_t log2bpp;
    if ((err = OS::xreadModeVariable(mode, OS::ModeVar_Log2BPP, log2bpp)) != nullptr)
        return err;

    is16or32 = (log2bpp == 4 || log2bpp == 5);

    return nullptr;
}
#endif

VectorReturn InterceptColTrans::selectTable(OS::Regs& regs)
{
    // Pass through to ColourTrans if destination mode <> -1.
    if (int32_t(regs[2]) != -1)
        return pass();

    // Zero the flags (in r5) unless r0 is a sprite area pointer.
    uint32_t effectiveR5 = isSpriteAreaPtr(regs[0]) ? regs[5] : 0;

    // store the R5 flags value (zapped to 0 as appropriate)    
    m_ws.ctrans_selgentab_R5 = effectiveR5;

#if Medusa
    bool is_16_or_32 = false;
    MyError err = intcolour_16or32(regs[0], regs[1], regs[5], is_16_or_32);
    if (err)
        return err;

    if (is_16_or_32) {
        if (!m_ws.jpeg_ctransflag) {
            return pass();
        }
    }
#endif

    return tableGenerate(regs);
}

VectorReturn InterceptColTrans::generateTable(OS::Regs& regs)
{
    uint32_t top_byte = regs[5] >> 24;
    if (top_byte != 0u && int32_t(regs[2]) != -1)
        return pass();

    m_ws.ctrans_selgentab_R5 = regs[5];

#if Medusa
    // Is source mode 16 or 32bpp?
    bool is16or32 = 0;
    MyError err = intcolour_16or32(regs[0], regs[1], regs[5], is16or32);
    if (err)
        return err;

    if (is16or32) {
        if (!m_ws.jpeg_ctransflag)
            return pass();
    }
#endif

    return tableGenerate(regs);
}

// `intcolour_table_generate`
VectorReturn InterceptColTrans::tableGenerate(OS::Regs& regs)
{
    uint32_t callerR5 = regs[5]; // can probably bin m_ws.ctrans_selgentab_R5
    MyError err;

    // check for jpeg request
    if (m_ws.jpeg_ctransflag) {
        if (regs[4] == 0) { // is it asking for buffer size?
            regs[4] = 12; // size of 32k table descriptor block
            return VectorReturn::claim();
        }

        // should only get here for jpeg request, deep pixels to
        // non-deep strip type
        return jpeg_ctrans_handle(*regs.as<ColourTrans::Table32K*>(4), m_job);
    }

// .intcolour_not_jpeg
    // Allow ColourTrans calls.. preserving the old state of the flags word
    ScopedPassthrough scopedPT(m_ws.m_interceptMgr, Passthrough_ColTrans);

    uint8_t* outTable = (uint8_t*)(uintptr_t)regs[4];  // Return buffer to be filled
    ScopedPtr<uint32_t> tempTable;        // No temporary store allocated yet

    // Return the size of the table of palette entries generated
    const ColourTrans::TableFlags flags =
        (ColourTrans::TableFlags)(regs[5] |
                                  (uint32_t)ColourTrans::TableFlags_ReturnPaletteTable);
    uint32_t size = 0;
    err = ColourTrans::generateTableGetSize(regs.as<OS::Mode>(0),
                                            regs.as<const OS::PaletteBase*>(1),
                                            regs.as<OS::Mode>(2),
                                            regs.as<const OS::PaletteBase*>(3),
                                            flags,
                                            size);

    // Attempt to claim a buffer big enough to put all the physical palette entries into
    if (!err && !tempTable.allocate(size / sizeof(uint32_t)))
        return MyError::OOM();

    if (!err) {
        // Fill the buffer with the physical colours for converting to pixel values
        err = ColourTrans::generateTable(regs.as<OS::Mode>(0),
                                         regs.as<const OS::PaletteBase*>(1),
                                         regs.as<OS::Mode>(2),
                                         regs.as<const OS::PaletteBase*>(3),
                                         (ColourTrans::Table*)tempTable.get(),
                                         flags);
    }

    if (err)
        return err;

    uint32_t effective_r5 = m_ws.ctrans_selgentab_R5;
    const uint32_t wide_bit = 0x10u;
    uint32_t totalBytes = 0;
    size_t entries = size / sizeof(uint32_t);

    for (uint32_t index = 0; index < entries; ++index) {
        uint32_t rgb = tempTable[index] & ~0xFFu;

        if (effective_r5 & wide_bit) {
            uint32_t value;
            uint8_t bytes;

            colour_rgbtopixvalwide(rgb, value, bytes, m_job);

            if (outTable) {
                size_t offset = totalBytes;

                if (bytes == 1u) {
                    outTable[offset] = (uint8_t)value;
                } else if (bytes == 2u) {
                    outTable[offset + 0] = (uint8_t)(value & 0xFFu);
                    outTable[offset + 1] = (uint8_t)((value >> 8) & 0xFFu);
                } else {
                    ((uint32_t *)(outTable + (index << 2)))[0] = value;
                }
            }
            totalBytes += bytes;
        } else {
            uint8_t value = colour_rgbtopixval(rgb, m_job);
            if (outTable)
                outTable[index] = value;

            totalBytes++;
        }
    }

    if (!outTable)              // Were they just reading the size?
        regs[4] = totalBytes; //   yes, so modify the return frames entry giving the size

    return VectorReturn::claim();
}

// `intcolour_readpalette`
// Deal with calling the PaletteV and then read palette.
uint32_t InterceptColTrans::readPalette(uint32_t pixval, uint32_t type)
{
    MyError err;
    uint32_t first, second;
    bool complete;

    // r0=logical colour, r1=type r4=read(1) r9=PaletteV(35)
    if ((err = OS_CallPaletteV(pixval, type, first, second, complete)) != nullptr)
        return 0;

    // On Medusa PaletteV is claimed and the value returned is expected to be correct.
    if (complete)
        return first;

    // Attempt to read the palette if that fails.
    if ((err = XOS_ReadPalette(pixval, type, first, second)) != nullptr)
        return 0;

    // Return so that &B0G0R000 -> &BBGGRR00.
    uint32_t mask = 0xF0F0F000u;
    first &= mask;
    first |= (first >> 4);

    return first;
}

// Given a palette entry (BBGGRRxx), return (and set) the closest GCOL
// in the current mode and palette. 
VectorReturn InterceptColTrans::setGCOL(OS::Regs& regs)
{
    // Avoid interference with a buffered with sequence of VDU 5 characters.
    MyError err = vdu5_flush(m_job);
    if (err)
        return err;

    uint32_t rgb = regs[0] & ~0xFFu;

    m_job.screenVars.setGCOLandColour(OS::GCOLAction(regs[4]), regs[3], rgb);

    // Convert into ColourTrans_ReturnGCOL to get the returned GCOL.
    regs[8] = ColourTrans_ReturnGCOL - ColourTrans_SelectTable;

    return pass();
}

// Subroutine to produce a contrasting RGB value.
// Entry: rgb = 0xbbggrr00, returns modified 0xbbggrr00.
static uint32_t oppositeColour(uint32_t bgr0)
{
    if ((bgr0 & 0x80000000u) == 0)
        bgr0 |= 0xFF000000u;
    else
        bgr0 &= ~0xFF000000u;

    if ((bgr0 & 0x00800000u) == 0)
        bgr0 |= 0x00FF0000u;
    else
        bgr0 &= ~0x00FF0000u;

    if ((bgr0 & 0x00008000u) == 0)
        bgr0 |= 0x0000FF00u;
    else
        bgr0 &= ~0x0000FF00u;

    return bgr0;
}

VectorReturn InterceptColTrans::retCN(OS::Regs& regs)
{
    // Pass on to printer-dependent code.
    regs[0] = colour_rgbtopixval(regs[0], m_job);
    return VectorReturn::claim();
}

VectorReturn InterceptColTrans::retModeCN(OS::Regs& regs)
{
    // Convert to ReturnColourNumber if "current mode" requested; pass through, otherwise.
    if (int32_t(regs[1]) != -1)
        return pass();

    return retCN(regs);
}

VectorReturn InterceptColTrans::setOppGCOL(OS::Regs& regs)
{
    // Avoid interference with a buffered with sequence of VDU 5 characters.
    MyError err = vdu5_flush(m_job);
    if (err)
        return err;

    uint32_t rgb = regs[0] & ~0xFFu;
    rgb = oppositeColour(rgb);

    m_job.screenVars.setGCOLandColour(OS::GCOLAction(regs[4]), regs[3], rgb);

    regs[8] = ColourTrans_ReturnOppGCOL - ColourTrans_SelectTable;

    return pass();
}

VectorReturn InterceptColTrans::retOppCN(OS::Regs& regs)
{
    uint32_t rgb = oppositeColour(regs[0]);
    regs[0] = (uint32_t)colour_rgbtopixval(rgb, m_job);

    return VectorReturn::claim();
}

VectorReturn InterceptColTrans::retModeOppCN(OS::Regs& regs)
{
    if (int32_t(regs[1]) != -1)
        return pass();

    return retOppCN(regs);
}

VectorReturn InterceptColTrans::setFontCols(OS::Regs& regs)
{
    // The original assembly performed a 26-bit mode switch to SVC here as
    // the ColourV handler runs in SVC in 32-bit builds.
    MyError err = vdu5_flush(m_job);
    if (err)
        return err;

    uint32_t bg = regs[1];
    uint32_t fg = regs[2];
    uint32_t offset = regs[3];

#if FontHandleEnsure
    // This now checks for handles <=0 so that the RISCOS_Lib no longer fails,
    // it used to only check for handles of zero - just like the manuals document.
    FontHandle font_handle = (FontHandle)regs[0];
    if (int32_t(font_handle) <= 0) {
        if ((err = XFont_CurrentFont(font_handle, bg, fg, offset)) != nullptr)
            return err;
    }

    if ((err = XFont_SetFont(font_handle)) != nullptr)
        return err;

    regs[0] = 0; // Further calls should not change the handle
#endif

    if ((err = font_absbg(bg, m_job)) != nullptr)
        return err;

    if ((err = font_absfg(fg, m_job)) != nullptr)
        return err;

    m_job.fontcoloffset = offset;
    if ((err = font_coloffset(int32_t(offset), m_job)) != nullptr)
        return err;

#if FontHandleEnsure
    {
        uint32_t font_handle = regs[0];
        uint32_t r1 = bg;
        uint32_t r2 = fg;
        uint32_t r3 = offset;
        err = XFont_CurrentFont(font_handle, r1, r2, r3);
        if (err)
            return err;

        regs[0] = font_handle;
    }
#endif

    return VectorReturn::claim();
}

const InterceptColTrans::Handler InterceptColTrans::s_table[] = {
    &InterceptColTrans::selectTable,    /* SelectTable */
    &InterceptColTrans::pass,           /* SelectGCOLTable */
    &InterceptColTrans::pass,           /* ReturnGCOL */
    &InterceptColTrans::setGCOL,        /* SetGCOL */
    &InterceptColTrans::retCN,          /* ReturnColourNumber */
    &InterceptColTrans::pass,           /* ReturnGCOLForMode */
    &InterceptColTrans::retModeCN,      /* ReturnColourNumberForMode */
    &InterceptColTrans::pass,           /* ReturnOppGCOL */
    &InterceptColTrans::setOppGCOL,     /* SetOppGCOL */
    &InterceptColTrans::retOppCN,       /* ReturnOppColourNumber */
    &InterceptColTrans::pass,           /* ReturnOppGCOLForMode */
    &InterceptColTrans::retModeOppCN,   /* ReturnOppColourNumberForMode */
    nullptr,                            /* GCOLToColourNumber */
    nullptr,                            /* ColourNumberToGCOL */
    &InterceptColTrans::pass,           /* ReturnFontColours */
    &InterceptColTrans::setFontCols,    /* SetFontColours */
    nullptr,                            /* InvalidateCache */
    &InterceptColTrans::pass,           /* SetCalibration */
    &InterceptColTrans::pass,           /* ReadCalibration */
    &InterceptColTrans::pass,           /* ConvertDeviceColour */
    &InterceptColTrans::pass,           /* ConvertDevicePalette */
    &InterceptColTrans::pass,           /* ConvertRGBToCIE */
    &InterceptColTrans::pass,           /* ConvertCIEToRGB */
    &InterceptColTrans::pass,           /* WriteCalibrationToFile */
    &InterceptColTrans::pass,           /* ConvertRGBToHSV */
    &InterceptColTrans::pass,           /* ConvertHSVToRGB */
    &InterceptColTrans::pass,           /* ConvertRGBToCMYK */
    &InterceptColTrans::pass,           /* ConvertCMYKToHSV */
    &InterceptColTrans::pass,           /* ReadPalette */
    &InterceptColTrans::pass,           /* SetPalette */
    &InterceptColTrans::pass,           /* SetColour */
    nullptr,                            /* MiscOp */
    &InterceptColTrans::pass,           /* WriteLoadingsToFile */
    nullptr,                            /* SetTextColour */
    nullptr,                            /* SetOppTextColour */
    &InterceptColTrans::generateTable   /* GenerateTable */
};

// The ColourV interception routine. `interceptcolour`
VectorReturn InterceptColTrans::intercept(OS::Regs& regs, CoreWS& ws)
{
    // Is this a ColourTrans call we're not interested in?
    if (ws.m_interceptMgr.hasPassthrough(Passthrough_ColTrans))
        return VectorReturn::pass();

    // note that an application should not rely on colour trans stuff (eg. translation
    //   table) in counting pass being meaningful during real pass.
    // Pass on if it is a counting pass (printer code would return rubbish anyway)
    if (ws.m_countingPass)
        return VectorReturn::pass();

    // If array entry is zero, pass to previous owner without entering the
    // printer driver's error-handling environment, and in particular
    // without having the chance to generate a persistent error.
    if (regs[8] < ARRAY_SIZE(s_table))
        if (s_table[regs[8]] == nullptr)
            return VectorReturn::pass();

    // Pick up current job's workspace, check for persistent errors, enable
    // ESCAPEs and pass through recursive OS_WriteC's
    CoreJobWS* job = ws.currentJob();
    if (job == nullptr)
        return VectorReturn::pass();

    InterceptColTrans colTrans(ws, *job);

    MyError err = job->checkPersistentError();
    if (err)
        return VectorReturn(err);

    // Error if the reason code is out of range. Otherwise process the call.
    if (regs[8] >= ARRAY_SIZE(s_table)) {
#if VectorErrors
        err = ws.messages.lookupError(&ErrorBlock_PrintCantUnkColV);
        err = job->makePersistentError(err);
        return VectorReturn(err);
#else
        return VectorReturn::pass();
#endif
    }

    if ((err = ws.m_escapeState.enable()) != nullptr)
        return VectorReturn(job->makePersistentError(err));

    Handler handler = s_table[regs[8]];
    // Norcroft doesn't support (colTrans.*handler)(regs);
    VectorReturn ret = ((&colTrans)->*handler)(regs);

    if (ret.isClaim())
        return ret;

    err = ws.m_escapeState.disableAndCheck();
    if (err)
        err = job->makePersistentError(err);

    if (ret.isError())
        return ret;

    return VectorReturn(err);
}

