#include "swis.h"
#include "OSSpriteOp.h"

#include "RLib/Constants/Sprite.h"

namespace OSSpriteOp {

MyError createSprite(Sprite::AreaValue areaValue, Sprite::Area* area,
                     const char* spriteName, bool createPalette,
                     int width, int height, OS::Mode mode)
{
    return _swix(OS_SpriteOp, _INR(0,6),
                 SpriteReason_CreateSprite | areaValue, area,
                 spriteName, createPalette, width, height, mode.toNumber());
}

MyError selectSprite(Sprite::AreaValue areaValue,
                     const Sprite::Area* area, Sprite::Id id,
                     Sprite::Header*& header)
{
    return _swix(OS_SpriteOp, _INR(0,2) | _OUT(2),
                 SpriteReason_SelectSprite | areaValue, area, id.header(),
                 &header);
}

MyError createMask(Sprite::AreaValue areaValue, Sprite::Area* area,
                   Sprite::Id id)
{
    return _swix(OS_SpriteOp, _INR(0,2),
                 SpriteReason_CreateMask | areaValue, area, id.header());
}

MyError putSpriteUserCoords(Sprite::AreaValue areaValue,
                            const Sprite::Area* area, Sprite::Id id,
                            int x, int y, OS::GCOLAction action)
{
    return _swix(OS_SpriteOp, _INR(0,5),
                 SpriteReason_PutSpriteUserCoords | areaValue, area, id.header(),
                 x, y, action);
}

MyError readPaletteSize(Sprite::AreaValue areaValue,
                        const Sprite::Area* area, Sprite::Id id,
                        int& size, OS::PaletteBase*& palette,
                        OS::Mode& mode)
{
    // OS::Mode is a uint32_t wrapper so this should be fine.
    return _swix(OS_SpriteOp, _INR(0,3) | _OUTR(3,5),
                 SpriteReason_CreateRemovePalette | areaValue, area, id.header(), -1,
                 &size, &palette, &mode);
}

MyError readSpriteInfo(Sprite::AreaValue areaValue,
                       const Sprite::Area* area, Sprite::Id id,
                       int& width, int& height, bool& mask,
                       OS::Mode& mode)
{
    return _swix(OS_SpriteOp, _INR(0,2) | _OUTR(3,6),
                 SpriteReason_ReadSpriteSize | areaValue, area, id.header(),
                 &width, &height, &mask, &mode);
}

MyError readSaveAreaSize(Sprite::AreaValue areaValue,
                         const Sprite::Area* area, Sprite::Id id,
                         int& saveAreaBytes)
{
    return _swix(OS_SpriteOp, _INR(0,2) | _OUT(2),
                 SpriteReason_ReadSaveAreaSize | areaValue, area, id.header(),
                 &saveAreaBytes);
}

MyError plotMaskScaled(Sprite::AreaValue areaValue,
                       const Sprite::Area* area, Sprite::Id id,
                       int x, int y,
                       const Sprite::ScaleFactor* factors)
{
    return _swix(OS_SpriteOp, _INR(0,4) | _IN(6),
                 SpriteReason_PlotMaskScaled | areaValue, area, id.header(),
                 x, y, factors);
}

MyError switchOutputToSprite(Sprite::AreaValue areaValue,
                             const Sprite::Area* area, Sprite::Id id,
                             Sprite::VDUSaveArea* saveArea, int& context0,
                             int& context1, int& context2,
                             int& context3)
{
    return _swix(OS_SpriteOp, _INR(0,3) | _OUTR(0,3),
                 SpriteReason_SwitchOutputToSprite | areaValue, area, id.header(),
                 saveArea,
                 &context0, &context1, &context2, &context3);
}

MyError switchOutputToMask(Sprite::AreaValue areaValue,
                           const Sprite::Area* area, Sprite::Id id,
                           Sprite::VDUSaveArea* saveArea, int& context0,
                           int& context1, int& context2,
                           int& context3)
{
    return _swix(OS_SpriteOp, _INR(0,3) | _OUTR(0,3),
                 SpriteReason_SwitchOutputToMask | areaValue, area, id.header(),
                 saveArea,
                 &context0, &context1, &context2, &context3);
}

MyError removeLeftHandWastage(Sprite::AreaValue areaValue,
                              const Sprite::Area* area,
                              Sprite::Id id)
{
    return _swix(OS_SpriteOp, _INR(0,2),
                 SpriteReason_RemoveLeftHandWastage | areaValue, area,
                 id.header());
}

} // namespace OSSpriteOp
