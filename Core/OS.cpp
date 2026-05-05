#include <stddef.h>
#include <stdint.h>

#include "swis.h"

#include "OS.h"
#include "Constants.h"

#include "RLib/MessageTrans.h"

#ifndef JPEG_Info
#define JPEG_Info 0x00049980
#endif
#ifndef JPEG_PlotScaled
#define JPEG_PlotScaled 0x00049982
#endif
#ifndef JPEG_PDriverIntercept
#define JPEG_PDriverIntercept 0x00049986
#endif

extern const uint32_t SpriteType_New16bpp = 5u;
extern const uint32_t SpriteType_New32bpp = 6u;
extern const uint32_t UpCall_PDumperAction = 23u;

static const uint32_t OSChangeEnvironment_EscapeHandler = 9u;

MyError rma_claim(size_t size, void*& block)
{
    // No idea if this is necessary, but the string allocs in PDriver do this.
    size = (size + 3u) & ~3u; // word align size

    return _swix(OS_Module, _IN(0)|_IN(3)|_OUT(2), 6, size, &block);
}

MyError rma_free(const void* block)
{
    return _swix(OS_Module, _IN(0)|_IN(2), 2, block);
}

MyError OS_CallPaletteV(uint32_t logical, uint32_t type, 
                                 uint32_t &firstFlash, uint32_t &secondFlash,
                                 bool &complete)
{
    MyError err;
    uint32_t c;

    if ((err = _swix(OS_CallAVector, _INR(0,1)|_IN(4)|_IN(9)|_OUTR(2,4),
                     logical, type, 1, 35,
                     &firstFlash, &secondFlash, &c)) != nullptr)

    complete = c == 0;

    return nullptr;
}

MyError XOS_Module(uint32_t reason, void *r2, uint32_t r3, void **r2_out) {
    uintptr_t r2v = uintptr_t(r2);
    MyError err = _swix(OS_Module,
                                 _INR(0,3) | _OUT(2),
                                 reason,
                                 0u,
                                 r2v,
                                 r3,
                                 &r2v);
    if (!err && r2_out != nullptr) {
        *r2_out = (void *)(uintptr_t)r2v;
    }
    return err;
}

MyError XOS_ModuleRMADesc(uint32_t& largestFree, uint32_t& totalFree)
{
    enum { ModHandReason_RMADesc = 5 };

    return _swix(OS_Module, _IN(0) | _OUTR(2,3),
                 ModHandReason_RMADesc, &largestFree, &totalFree);
}


MyError XOS_BGet(int32_t handle, uint8_t *byte_out, int *eof_out) {
    uint32_t r0 = 0;
    int psr = 0;
    MyError err = _swix(OS_BGet,
                                 _IN(0) | _OUT(0) | _OUT(_FLAGS),
                                 (uint32_t)handle,
                                 &r0,
                                 &psr);
    if (!err) {
        if (byte_out != nullptr) {
            *byte_out = (uint8_t)r0;
        }
        if (eof_out != nullptr) {
            *eof_out = ((psr & _C) != 0) ? 1 : 0;
        }
    }
    return err;
}

MyError XOS_ReadEscapeState(int *escaped) {
    int psr = 0;
    MyError err = _swix(OS_ReadEscapeState, _OUT(_FLAGS), &psr);
    if (!err && escaped != nullptr) {
        *escaped = ((psr & _C) != 0) ? 1 : 0;
    }
    return err;
}


MyError XOS_CallAVector(uint32_t vector,
                                uint32_t *r0, uint32_t *r1, uint32_t *r2, uint32_t *r3,
                                uint32_t *r4, uint32_t *r5, uint32_t *r6, uint32_t *r7)
{
    uint32_t v0 = (r0 != nullptr) ? *r0 : 0u;
    uint32_t v1 = (r1 != nullptr) ? *r1 : 0u;
    uint32_t v2 = (r2 != nullptr) ? *r2 : 0u;
    uint32_t v3 = (r3 != nullptr) ? *r3 : 0u;
    uint32_t v4 = (r4 != nullptr) ? *r4 : 0u;
    uint32_t v5 = (r5 != nullptr) ? *r5 : 0u;
    uint32_t v6 = (r6 != nullptr) ? *r6 : 0u;
    uint32_t v7 = (r7 != nullptr) ? *r7 : 0u;

    MyError err =
        _swix(OS_CallAVector,
              _INR(0,7) | _IN(9) | _OUTR(0,7),
              v0, v1, v2, v3, v4, v5, v6, v7, vector,
              &v0, &v1, &v2, &v3, &v4, &v5, &v6, &v7);
    if (err != nullptr)
        return err;

    if (r0 != nullptr)
        *r0 = v0;
    if (r1 != nullptr)
        *r1 = v1;
    if (r2 != nullptr)
        *r2 = v2;
    if (r3 != nullptr)
        *r3 = v3;
    if (r4 != nullptr)
        *r4 = v4;
    if (r5 != nullptr)
        *r5 = v5;
    if (r6 != nullptr)
        *r6 = v6;
    if (r7 != nullptr)
        *r7 = v7;

    return nullptr;
}

MyError XOS_ReadPalette(uint32_t logical, uint32_t type, 
                                 uint32_t &firstFlash, uint32_t &secondFlash)
{
    return _swix(OS_ReadPalette, _INR(0,1)|_OUTR(2,3), logical, type,
                 &firstFlash, &secondFlash);
}

MyError XOS_ReadModeVariable(uint32_t mode, uint32_t variable, uint32_t& value)
{
    return _swix(OS_ReadModeVariable, _INR(0,1)|_OUT(2),
                 mode, variable, &value);
}

MyError XOS_ReadMemMapInfo(uint32_t& pageSize)
{
    return _swix(OS_ReadMemMapInfo, _OUT(0), &pageSize);
}

MyError XOS_ReadUnsigned(uint32_t base, const char *text, uint32_t& value)
{
    return _swix(OS_ReadUnsigned, _INR(0,1) | _OUT(2),
                 base, text, &value);
}

MyError XWimp_ClaimFreeMemory(uint32_t reason,
                                      uint32_t& bytes_in_out,
                                      uint32_t& slot_out)
{
    return _swix(Wimp_ClaimFreeMemory, _INR(0,1) | _OUTR(1,2),
                 reason, bytes_in_out, &bytes_in_out, &slot_out);
}

MyError XOS_Byte(uint32_t *r0, uint32_t *r1, uint32_t *r2)
{
    uint32_t value0 = (r0 != nullptr) ? *r0 : 0u;
    uint32_t value1 = (r1 != nullptr) ? *r1 : 0u;
    uint32_t value2 = (r2 != nullptr) ? *r2 : 0u;
    MyError err = _swix(OS_Byte,
                                 _INR(0,2) | _OUTR(0,2),
                                 value0, value1, value2,
                                 &value0, &value1, &value2);
    if (err != nullptr)
        return err;

    if (r0 != nullptr)
        *r0 = value0;
    if (r1 != nullptr)
        *r1 = value1;
    if (r2 != nullptr)
        *r2 = value2;

    return nullptr;
}

MyError XOS_ServiceCall(uint32_t code)
{
    return _swix(OS_ServiceCall, _IN(1), code);
}

MyError XOS_WriteI(uint32_t value) {
    return _swix((int)(OS_WriteI + (value & 0xFFu)), 0);
}

MyError XOS_ReadDynamicArea(int32_t area, void **address_out) {
    uint32_t r0 = (uint32_t)area;
    MyError err = _swix(OS_ReadDynamicArea, _IN(0) | _OUT(0), r0, &r0);
    if (!err && address_out != nullptr) {
        *address_out = (void *)(uintptr_t)r0;
    }
    return err;
}

MyError XOS_ChangeDynamicArea(int32_t area, int32_t change) {
    return _swix(OS_ChangeDynamicArea, _INR(0,1), (uint32_t)area, (uint32_t)change);
}

MyError XOS_CLI(const char *command) {
    return _swix(OS_CLI, _IN(0), command);
}

MyError XOS_AddCallBack(void *callback, uint32_t service) {
    return _swix(OS_AddCallBack, _INR(0,1), callback, service);
}

MyError XOS_UpCall(uint32_t r0, uint32_t r1, void *r2) {
    return _swix(OS_UpCall, _INR(0,2), r0, r1, r2);
}

MyError XOS_WriteC(uint32_t value) {
    return _swix(OS_WriteC, _IN(0), value);
}

MyError XOS_WriteN(const void *buffer, uint32_t len) {
    return _swix(OS_WriteN, _INR(0,1), buffer, len);
}

MyError XOS_SetColour(uint32_t flags, uint32_t colour) {
    return _swix(OS_SetColour, _INR(0,1), flags, colour);
}

MyError XFont_ReadScaleFactor(int32_t &x, int32_t &y)
{
    uint32_t r1 = 0;
    uint32_t r2 = 0;
    MyError err = _swix(Font_ReadScaleFactor, _OUTR(1,2), &r1, &r2);
    if (!err) {
        x = (int32_t)r1;
        y = (int32_t)r2;
    }
    return err;
}

MyError XFont_CurrentFont(FontHandle &font, uint32_t &bg, uint32_t &fg, uint32_t &offset) {
    uint32_t v0, v1, v2, v3;
    MyError err = _swix(Font_CurrentFont,
                                 _OUTR(0,3),
                                 &v0, &v1, &v2, &v3);
    if (err)
        return err;

    font = (FontHandle)v0;
    bg = v1;
    fg = v2;
    offset = v3;

    return nullptr;
}

MyError XFont_ReadDefn(FontHandle handle, char *buffer, uint32_t flags,
                               int32_t *x_out, int32_t *y_out) {
    uint32_t r2 = 0;
    uint32_t r3 = 0;
    MyError err = _swix(Font_ReadDefn,
                                 _IN(0) | _IN(1) | _IN(3) | _OUT(2) | _OUT(3),
                                 (uint32_t)handle,
                                 buffer,
                                 flags,
                                 &r2,
                                 &r3);
    if (!err) {
        if (x_out != nullptr) {
            *x_out = (int32_t)r2;
        }
        if (y_out != nullptr) {
            *y_out = (int32_t)r3;
        }
    }
    return err;
}

MyError XFont_ReadDefnSize(FontHandle handle, uint32_t flags,
                                   uint32_t *size_out)
{
    uint32_t r2 = 0;
    MyError err = _swix(Font_ReadDefn,
                                 _IN(0) | _IN(1) | _IN(3) | _OUT(2),
                                 (uint32_t)handle,
                                 0u,
                                 flags,
                                 &r2);
    if (!err && size_out != nullptr) {
        *size_out = r2;
    }
    return err;
}

MyError XFont_Converttopoints(int32_t *x_in_out, int32_t *y_in_out) {
    uint32_t r1 = (x_in_out != nullptr) ? (uint32_t)(*x_in_out) : 0u;
    uint32_t r2 = (y_in_out != nullptr) ? (uint32_t)(*y_in_out) : 0u;
    MyError err = _swix(Font_Converttopoints,
                                 _INR(1,2) | _OUTR(1,2),
                                 r1,
                                 r2,
                                 &r1,
                                 &r2);
    if (!err) {
        if (x_in_out != nullptr) {
            *x_in_out = (int32_t)r1;
        }
        if (y_in_out != nullptr) {
            *y_in_out = (int32_t)r2;
        }
    }
    return err;
}

MyError XFont_SetFont(FontHandle font) {
    return _swix(Font_SetFont, _IN(0), font);
}

MyError XFont_ListFonts(char *buffer, int32_t *context, int32_t path) {
    const char* r1 = buffer;
    uint32_t r2 = (context != nullptr) ? (uint32_t)(*context) : 0u;
    MyError err = _swix(Font_ListFonts,
                                 _INR(1,3) | _OUT(1) | _OUT(2),
                                 buffer,
                                 r2,
                                 (uint32_t)path,
                                 &r1,
                                 &r2);
    if (!err)
        *context = (int32_t)r2;

    return err;
}

MyError XMakePSFont_MakeFont(FileHandle output_handle,
                                     const char *font_name,
                                     char *return_name_buffer,
                                     uint32_t *buffer_size_in_out,
                                     uint32_t flags) {
    return MakePSFont::xmakeFont(output_handle,
                                 font_name,
                                 return_name_buffer,
                                 buffer_size_in_out,
                                 flags);
}

MyError XPDriver_MiscOp(uint32_t reason, void *buffer, uint32_t *size_in_out,
                                uint32_t *handle_in_out, uint32_t *flags_in_out) {
    uint32_t r2 = (size_in_out != nullptr) ? *size_in_out : 0u;
    uint32_t r3 = (handle_in_out != nullptr) ? *handle_in_out : 0u;
    uint32_t r4 = (flags_in_out != nullptr) ? *flags_in_out : 0u;
    MyError err = _swix(PDriver_MiscOp,
                                 _INR(0,4) | _OUT(2) | _OUT(3) | _OUT(4),
                                 reason,
                                 buffer,
                                 r2,
                                 r3,
                                 r4,
                                 &r2,
                                 &r3,
                                 &r4);
    if (!err) {
        if (size_in_out != nullptr) {
            *size_in_out = r2;
        }
        if (handle_in_out != nullptr) {
            *handle_in_out = r3;
        }
        if (flags_in_out != nullptr) {
            *flags_in_out = r4;
        }
    }
    return err;
}

MyError XColourTrans_GenerateTableGetSize(const _kernel_swi_regs& regs, uint32_t& size)
{
    return _swix(ColourTrans_GenerateTable, _INR(0,7)|_OUT(4),
                 regs.r[0], regs.r[1], regs.r[2], regs.r[3], 0,
                 regs.r[5], regs.r[6], regs.r[7], &size);
}

MyError XColourTrans_GenerateTable(const _kernel_swi_regs& regs)
{
    return _swix(ColourTrans_GenerateTable, _INR(0,7),
                 regs.r[0], regs.r[1], regs.r[2], regs.r[3], regs.r[4],
                 regs.r[5], regs.r[6], regs.r[7]);
}

MyError XColourTrans_ReturnGCOLForMode(uint32_t *r0,
                                                uint32_t *r1,
                                                uint32_t *r2)
{
    uint32_t value0 = (r0 != nullptr) ? *r0 : 0u;
    uint32_t value1 = (r1 != nullptr) ? *r1 : 0u;
    uint32_t value2 = (r2 != nullptr) ? *r2 : 0u;
    MyError err = _swix(ColourTrans_ReturnGCOLForMode,
                                 _INR(0,2) | _OUTR(0,2),
                                 value0, value1, value2,
                                 &value0, &value1, &value2);
    if (err != nullptr)
        return err;

    if (r0 != nullptr)
        *r0 = value0;
    if (r1 != nullptr)
        *r1 = value1;
    if (r2 != nullptr)
        *r2 = value2;

    return nullptr;
}

MyError XColourTrans_InvalidateCache(void) {
    return _swix(ColourTrans_InvalidateCache, 0);
}

MyError XOS_SpriteInitArea(Sprite::Area* area)
{
    return _swix(OS_SpriteOp, _INR(0,1), 9+256, area);
}


MyError XDraw_Fill(Draw::Path path, uint32_t flags,
                   const Draw::Matrix* matrix, Draw::Unit flatness)
{
    return _swix(Draw_Fill,
                 _INR(0,3),
                 path.components(),
                 flags,
                 matrix,
                 (uint32_t)flatness.raw());
}

MyError XDraw_Stroke(Draw::Path path, uint32_t flags,
                     const Draw::Matrix* matrix, Draw::Unit flatness,
                     Draw::Unit thickness, const Draw::CapJoin* capJoin,
                     const Draw::DashPattern* dashPattern)
{
    return _swix(Draw_Stroke,
                 _INR(0,6),
                 path.components(),
                 flags,
                 matrix,
                 (uint32_t)flatness.raw(),
                 (uint32_t)thickness.raw(),
                 capJoin,
                 dashPattern);
}

MyError XMessageTrans_OpenFile(MessageTrans::Block *block,
                                        const char* filename)
{
    return _swix(MessageTrans_OpenFile, _INR(0,3), 
                 block, filename, nullptr);
}

MyError XMessageTrans_CloseFile(MessageTrans::Block *block) {
    return _swix(MessageTrans_CloseFile, _IN(0), block);
}

#if 0
MyError XMessageTrans_ErrorLookup(uint32_t *r0, uint32_t *r1, uint32_t *r2,
                                          uint32_t *r3, uint32_t *r4, uint32_t *r5,
                                          uint32_t *r6, uint32_t *r7) {
    return swix_inout_u8(MessageTrans_ErrorLookup, r0, r1, r2, r3, r4, r5, r6, r7);
}

MyError XMessageTrans_GSLookup(uint32_t *r0, uint32_t *r1, uint32_t *r2,
                                       uint32_t *r3, uint32_t *r4, uint32_t *r5,
                                       uint32_t *r6, uint32_t *r7) {
    return swix_inout_u8(MessageTrans_GSLookup, r0, r1, r2, r3, r4, r5, r6, r7);
}

MyError XMessageTrans_Lookup(uint32_t *r0, uint32_t *r1, uint32_t *r2,
                                     uint32_t *r3, uint32_t *r4, uint32_t *r5,
                                     uint32_t *r6, uint32_t *r7) {
    return swix_inout_u8(MessageTrans_Lookup, r0, r1, r2, r3, r4, r5, r6, r7);
}
#endif

MyError XJPEG_Info(uint32_t reason, const void *jpeg, uint32_t length,
                           uint32_t *width_out, uint32_t *height_out,
                           uint32_t *flags_out, uint32_t *xdpi_out, uint32_t *ydpi_out) {
    uint32_t r0 = 0;
    uint32_t r2 = 0;
    uint32_t r3 = 0;
    uint32_t r4 = 0;
    uint32_t r5 = 0;
    MyError err = _swix(JPEG_Info,
                                 _INR(0,2) | _OUT(0) | _OUT(2) | _OUT(3) | _OUT(4) | _OUT(5),
                                 reason,
                                 jpeg,
                                 length,
                                 &r0,
                                 &r2,
                                 &r3,
                                 &r4,
                                 &r5);
    if (!err) {
        if (width_out != nullptr) {
            *width_out = r2;
        }
        if (height_out != nullptr) {
            *height_out = r3;
        }
        if (flags_out != nullptr) {
            *flags_out = r0;
        }
        if (xdpi_out != nullptr) {
            *xdpi_out = r4;
        }
        if (ydpi_out != nullptr) {
            *ydpi_out = r5;
        }
    }
    return err;
}

MyError XJPEG_PlotScaled(const void *jpeg, int32_t x, int32_t y,
                                 const int32_t *scale, uint32_t length,
                                 uint32_t flags) {
    return _swix(JPEG_PlotScaled,
                 _INR(0,5),
                 jpeg,
                 (uint32_t)x,
                 (uint32_t)y,
                 scale,
                 length,
                 flags);
}

MyError XJPEG_PDriverIntercept(uint32_t on) {
    return _swix(JPEG_PDriverIntercept, _IN(0), on);
}

MyError XOS_Word(uint32_t reason, void *block) {
    return _swix(OS_Word, _INR(0,1), reason, block);
}

MyError XDraw_ProcessPath(const void *path, uint32_t flags,
                          const Draw::Matrix *matrix, Draw::Unit flatness,
                          uint32_t reason_or_buffer, uint32_t *size_out)
{
    uint32_t r5 = (size_out != nullptr) ? *size_out : 0u;
    MyError err = _swix(Draw_ProcessPath,
                        _INR(0,5) | _OUT(5),
                        path,
                        flags,
                        matrix,
                        flatness.raw(),
                        reason_or_buffer,
                        r5,
                        &r5);

    if (!err && size_out != nullptr)
        *size_out = r5;

    return err;
}
