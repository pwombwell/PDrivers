#ifndef PDRIVERPS_SPRITE_H
#define PDRIVERPS_SPRITE_H

#include "Core/Device.h"

typedef struct PDriverWS PDriverWS;
typedef struct JobWS JobWS;

class Output;
class ColourMapping;

/* Sprite handling routines for the PostScript printer driver. */
MyError sprite_commonoutput(const Sprite::Selector& sprite,
                            const Sprite::Info& spriteInfo,
                            const Sprite::PixelRect& sourceRect,
                            const ColourMapping& mapping,
                            Sprite::PlotAction action,
                            uint32_t log2bpc,
                            JobWS& job);
MyError sprite_mask_commonoutput(const Sprite::Selector& sprite,
                                 const Sprite::Info& spriteInfo,
                                 const Sprite::PixelRect& sourceRect,
                                 uint32_t log2bpc,
                                 JobWS& job);

#if Medusa
MyError sprite_output32bpp(const Sprite::Selector& sprite,
                           const Sprite::Info& spriteInfo,
                           const Sprite::PixelRect& sourceRect,
                           bool useMask,
                           JobWS& job);
MyError sprite_output16bpp(const Sprite::Selector& sprite,
                           const Sprite::Info& spriteInfo,
                           const Sprite::PixelRect& sourceRect,
                           bool useMask,
                           JobWS& job);
#endif

#endif
