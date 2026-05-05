#ifndef COMMON_SPRITE_H
#define COMMON_SPRITE_H

#include "RLibX/Sprite.h"

#if Medusa
/* Massage a sprite mode word into an 8bpp greyscale mode of same DPI. */
inline Sprite::ModeWord medusa_same_dpi_g256_mode(Sprite::ModeWord modeWord)
{
    return Sprite::ModeWord3(Sprite::Type_Bpp8, modeWord.dpiH(), modeWord.dpiV());
}
#endif

#endif
