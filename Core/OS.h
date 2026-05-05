#ifndef CORE_OS_H
#define CORE_OS_H

#include "RLib/MessageTrans.h"
#include "RLibX/Utils/Debug.h"

#include "RLibX/ColourTrans.h"
#include "RLib/Draw.h"
#include "RLibX/Font.h"
#include "RLibX/JPEG.h"
#include "RLibX/MakePSFont.h"
#include "RLib/OS.h"
#include "RLibX/Sprite.h"

typedef void* EnvironmentHandler[3];

// Note no constructors are called with rma_claim*().
MyError rma_claim(size_t size, void*& block);
MyError rma_free(const void* block);

// All deprecated. Use namespaced varants in RLib.
MyError OS_CallPaletteV(uint32_t logical, uint32_t type,
                                 uint32_t &firstFlash, uint32_t &secondFlash,
                                 bool &complete);

MyError XOS_Module(uint32_t reason, void *r2, uint32_t r3, void **r2_out);
MyError XOS_ModuleRMADesc(uint32_t& largestFree, uint32_t& totalFree);
MyError XOS_ReadEscapeState(int *escaped);
MyError XOS_CallAVector(uint32_t vector,
                                uint32_t *r0, uint32_t *r1, uint32_t *r2, uint32_t *r3,
                                uint32_t *r4, uint32_t *r5, uint32_t *r6, uint32_t *r7);
MyError XOS_ReadPalette(uint32_t logical, uint32_t type, 
                                 uint32_t &firstFlash, uint32_t &secondFlash);

MyError XOS_ReadModeVariable(uint32_t mode, uint32_t variable, uint32_t& value);
MyError XOS_ReadMemMapInfo(uint32_t& pageSize);
MyError XOS_ReadUnsigned(uint32_t base, const char *text, uint32_t& value);
MyError XWimp_ClaimFreeMemory(uint32_t reason,
                                      uint32_t& bytes_in_out,
                                      uint32_t& slot_out);

MyError XOS_Byte(uint32_t *r0, uint32_t *r1, uint32_t *r2);


MyError XOS_ServiceCall(uint32_t code);

MyError XOS_WriteI(uint32_t value);
MyError XOS_ReadDynamicArea(int32_t area, void **address_out);
MyError XOS_ChangeDynamicArea(int32_t area, int32_t change);
MyError XOS_CLI(const char *command);
MyError XOS_AddCallBack(void *callback, uint32_t service);
MyError XOS_UpCall(uint32_t r0, uint32_t r1, void *r2);
MyError XOS_WriteC(uint32_t value);
MyError XOS_WriteN(const void *buffer, uint32_t len);
MyError XOS_SetColour(uint32_t flags, uint32_t colour);

MyError XFont_ReadScaleFactor(int32_t &x, int32_t &y);
MyError XFont_CurrentFont(FontHandle &font, uint32_t &bg, uint32_t &fg, uint32_t &offset);
MyError XFont_ReadDefn(FontHandle handle, char *buffer, uint32_t flags,
                               int32_t *x_out, int32_t *y_out);
MyError XFont_ReadDefnSize(FontHandle handle, uint32_t flags,
                                   uint32_t *size_out);
MyError XFont_Converttopoints(int32_t *x_in_out, int32_t *y_in_out);
MyError XFont_SetFont(FontHandle font);
MyError XFont_ListFonts(char *buffer, int32_t *context, int32_t path);

MyError XMakePSFont_MakeFont(FileHandle output_handle,
                                     const char *font_name,
                                     char *return_name_buffer,
                                     uint32_t *buffer_size_in_out,
                                     uint32_t flags);
MyError XPDriver_MiscOp(uint32_t reason, void *buffer, uint32_t *size_in_out,
                                uint32_t *handle_in_out, uint32_t *flags_in_out);

MyError XColourTrans_ReturnGCOLForMode(uint32_t *r0, uint32_t *r1, uint32_t *r2);
MyError XColourTrans_InvalidateCache(void);


MyError XOS_SpriteInitArea(Sprite::Area* area); // SpriteOp 9 + 256
MyError XOS_SpriteReadPalette(uint32_t *r0, uint32_t *r1, uint32_t *r2, uint32_t *r3,
                             uint32_t *r4, uint32_t *r5, uint32_t *r6, uint32_t *r7);

MyError XDraw_Fill(Draw::Path path, uint32_t flags,
                   const Draw::Matrix* matrix, Draw::Unit flatness);
MyError XDraw_Stroke(Draw::Path path, uint32_t flags,
                     const Draw::Matrix* matrix, Draw::Unit flatness,
                     Draw::Unit thickness, const Draw::CapJoin* capJoin,
                     const Draw::DashPattern* dashPattern);
MyError XJPEG_Info(uint32_t reason, const void *jpeg, uint32_t length,
                           uint32_t *width_out, uint32_t *height_out,
                           uint32_t *flags_out, uint32_t *xdpi_out, uint32_t *ydpi_out);
MyError XJPEG_PlotScaled(const void *jpeg, int32_t x, int32_t y,
                                 const int32_t *scale, uint32_t length,
                                 uint32_t flags);
MyError XJPEG_PDriverIntercept(uint32_t on);
MyError XOS_Word(uint32_t reason, void *block);
MyError XDraw_ProcessPath(const void *path, uint32_t flags,
                                  const Draw::Matrix *matrix, Draw::Unit flatness,
                                  uint32_t reason_or_buffer, uint32_t *size_out);

#endif
