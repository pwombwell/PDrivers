#ifndef PDRIVERPDF_SPRITE_H
#define PDRIVERPDF_SPRITE_H

#include "Core/Device.h"

#include "RLib/Geometry/Size.h"
#include "RLib/OS.h"

typedef struct PDriverWS PDriverWS;
typedef struct JobWS JobWS;

struct ColourMapping;

/* Sprite handling routines for the PDF printer driver. */
MyError sprite_commonoutput(const Sprite::Selector& spriteIn,
                            const Sprite::Info& spriteInfo,
                            const Geometry::Size<uint32_t>& clip,
                            const ColourMapping& mapping,
                            bool useMask,
                            uint32_t log2bpc,
                            JobWS& job);

MyError sprite_mask_commonoutput(const Sprite::Selector& spriteIn,
                                 const Sprite::Info& spriteInfo,
                                 const Geometry::Size<uint32_t>& clip,
                                 JobWS& job);

#endif
