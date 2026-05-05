#include "kernel.h"

#include "Sprite.h"

#include "RLib/Constants/Sprite.h"
#include "ROSLib/OSSpriteOp.h"

#include "swis.h"

namespace riscos {
namespace Sprite {

Type ModeWord::type() const
{
    return isRO5() ? to5().type() : to3().type();
}

uint16_t ModeWord::dpiH() const
{
    return isRO5() ? to5().dpiH() : to3().dpiH();
}

uint16_t ModeWord::dpiV() const
{
    return isRO5() ? to5().dpiV() : to3().dpiV();
}

uint8_t ModeWord::eigX() const
{
    return isRO5() ? to5().eigX() : dpiToEig(to3().dpiH());
}

uint8_t ModeWord::eigY() const
{
    return isRO5() ? to5().eigY() : dpiToEig(to3().dpiV());
}

uint16_t ModeWord::modeFlags() const
{
    return isRO5() ? to5().modeFlags() : to3().modeFlags();
}

ModeWord ModeWord::fromModeWord3(Type type, uint16_t dpiH, uint16_t dpiV)
{
    return ModeWord3(type, dpiH, dpiV).modeWord();
}

ModeWord ModeWord::fromModeWord5(Type type, uint8_t eigX, uint8_t eigY,
                                 uint16_t modeFlags)
{
    return ModeWord5(type, eigX, eigY, modeFlags).modeWord();
}

ModeWord3 ModeWord::to3() const
{
    return ModeWord3(*this);
}

ModeWord5 ModeWord::to5() const
{
    return ModeWord5(*this);
}

// AreaView -----------------------------------------------------------
MyError AreaView::initialise(uint32_t size, size_t extensions)
{
    m_area->size = size;
    MyError err = initArea(AreaValue_UserName, m_area);
    if (err != nullptr)
        return err;

    m_area->first = sizeof(Area) + extensions;
    m_area->used = m_area->first;

    return nullptr;
}

void AreaView::initialiseCheekily(uint32_t size, size_t extensions)
{
    m_area->size = size;
    m_area->spriteCount = 0;
    m_area->first = sizeof(Area) + extensions;
    m_area->used = m_area->first;
}

// HeaderView ---------------------------------------------------------
bool HeaderView::hasPalette() const
{
    return Palette::isValidPaletteSize(paletteSize());
}

Palette HeaderView::palette() const
{
    return Palette(ptr());
}

// Selector -----------------------------------------------------------
Selector::Selector()
    : m_areaValue(AreaValue_System)
    , m_area(nullptr)
    , m_id((Header*)nullptr)
{
}

bool Selector::isSystemSprite() const
{
    return m_areaValue == AreaValue_System;
}

MyError Selector::resolve(uint32_t& reason)
{
    // Start of `sprite_put_altentry`

    // Is this a system sprite operation? (system sprite + name)
    if (reason >= 0x100)
        return nullptr;

    // Convert to an op with name.
    reason |= 0x100;

    return getSystemBase(m_area);
}

MyError Selector::resolve(ResolvedSelector& out) const
{
    Area* area = m_area;
    AreaValue areaValue = m_areaValue;

    // Already a pointer, so just return a container.
    if (areaValue == AreaValue_UserPtr) {
        out = ResolvedSelector(area, m_id.header());
        return nullptr;
    }

    // System sprite area? Get address of it.
    if (isSystemSprite()) {
        MyError err = getSystemBase(area);
        if (err != nullptr)
            return err;

        areaValue = AreaValue_UserName;
    }

    // Got a pointer to an area, convert id to pointer.
    Header* header;
    MyError err = OSSpriteOp::selectSprite(areaValue, area, m_id, header);
    if (err != nullptr)
        return err;

    out = ResolvedSelector(area, header);
    return nullptr;
}

// Info ---------------------------------------------------------------
MyError Info::get(const Selector& sprite)
{
    int width;
    int height;
    bool hasMask;
    OS::Mode mode;
    MyError err = readInfo(sprite, width, height, hasMask, mode);
    if (err != nullptr)
        return err;

    m_size.width = uint32_t(width);
    m_size.height = uint32_t(height);
    m_mode = mode;
    m_hasMask = hasMask;
    return nullptr;
}

// VDUContext ---------------------------------------------------------
void VDUContext::setToScreen()
{
    // Values from PDriver/Header.s: setscreenparams
    u.v[0] = AreaValue_UserPtr + SpriteReason_SwitchOutputToSprite;
    u.v[1] = 0;
    u.v[2] = 0;
    u.v[3] = 1;
}

MyError VDUContext::restore() const
{
    return _swix(OS_SpriteOp, _INR(0,3), u.v[0], u.v[1], u.v[2], u.v[3]);
}

MyError ScopedVDUContext::switchOutputToSprite(const Sprite::Selector& sprite)
{
    m_set = true;
    return Sprite::switchOutputToSprite(sprite, nullptr, *this);
}

MyError ScopedVDUContext::switchOutputToMask(const Sprite::Selector& sprite)
{
    m_set = true;
    return Sprite::switchOutputToMask(sprite, nullptr, *this);
}

// ----------------------------------------------------------------------------
MyError getSystemInfo(Area*& base, size_t& size, size_t& sizeLimit)
{
      return _swix(OS_ReadDynamicArea, _IN(0) | _OUTR(0,2), 3,
                   &base, &size, &sizeLimit);
}

MyError getSystemBase(Area*& base)
{
    return _swix(OS_ReadDynamicArea, _IN(0) | _OUT(0), 3, &base);
}

MyError getSystemSize(size_t& size)
{
    return _swix(OS_ReadDynamicArea, _IN(0) | _OUT(1), 3, &size);
}

// ----------------------------------------------------------------------------
MyError initArea(AreaValue areaValue, Area* area)
{
    return _swix(OS_SpriteOp,
                 _INR(0,1),
                 uint32_t(areaValue) | SpriteReason_ClearSprites,
                 area);
}

MyError create(const Selector& selector, bool createPalette,
               int width, int height, OS::Mode mode)
{
    return OSSpriteOp::createSprite(selector.areaValue(),
                                    (Area*)selector.area(),
                                    selector.id().name(),
                                    createPalette,
                                    width,
                                    height,
                                    mode);
}

MyError selectSprite(const Selector& selector, Header*& header)
{
    return OSSpriteOp::selectSprite(selector.areaValue(), selector.area(),
                                    selector.id(), header);
}

MyError deleteSprite(const Selector& selector)
{
    return _swix(OS_SpriteOp,
                 _INR(0,2),
                 uint32_t(selector.areaValue()) | SpriteReason_DeleteSprite,
                 selector.area().ptr(),
                 selector.id().name());
}

MyError createMask(const Selector& selector)
{
    return OSSpriteOp::createMask(selector.areaValue(),
                                  (Area*)selector.area(),
                                  selector.id());
}

MyError putScaled(const Sprite::Selector& selector, int x, int y,
                  Sprite::PlotAction action,
                  Sprite::ScaleFactor const* scaleFactors,
                  const ColourTrans::Table* colTrans)
{
    // norcroft namespace bug
    return _swix(OS_SpriteOp, _INR(0,7),
                 uint32_t(selector.areaValue()) | SpriteReason_PutSpriteScaled,
                 selector.area().ptr(),
                 selector.id().name(),
                 x, y, action, scaleFactors, colTrans);
}

MyError putAtUserCoords(const Selector& selector,
                        int x, int y, OS::GCOLAction action)
{
    return OSSpriteOp::putSpriteUserCoords(selector.areaValue(), selector.area(),
                                           selector.id(), x, y, action);
}

MyError readPalette(const Selector& selector, Palette& palette, OS::Mode& mode)
{
    int size;
    OS::PaletteBase* base;
    MyError err = OSSpriteOp::readPaletteSize(selector.areaValue(), selector.area(),
                                              selector.id(), size, base, mode);
    if (err != nullptr)
        return err;

    if (!Palette::isValidPaletteSize(size)) {
        palette.invalidate();
        return nullptr;
    }

    palette = Palette(base, uint32_t(size) / sizeof(*base));
    return nullptr;
}

MyError readInfo(const Selector& selector,
                               int& width, int& height, bool& mask,
                               OS::Mode& mode)
{
    return OSSpriteOp::readSpriteInfo(selector.areaValue(), selector.area(), selector.id(),
                           width, height, mask, mode);
}

MyError readSaveAreaSize(const Selector& selector, int& saveAreaBytes)
{
    return OSSpriteOp::readSaveAreaSize(selector.areaValue(), selector.area(),
                                         selector.id(), saveAreaBytes);
}

MyError plotMaskScaled(const Selector& selector, int x, int y,
                       Sprite::ScaleFactor const& factors)
{
    return OSSpriteOp::plotMaskScaled(selector.areaValue(), selector.area(), selector.id(),
                           x, y, &factors);
}

MyError plotMaskScaled(const Selector& selector, int x, int y)
{
    return OSSpriteOp::plotMaskScaled(selector.areaValue(), selector.area(), selector.id(),
                           x, y, nullptr);
}

MyError switchOutputToSprite(const Selector& selector,
                             VDUSaveArea* saveArea,
                             VDUContext& context)
{
    int word0;
    int word1;
    int word2;
    int word3;
    MyError err = OSSpriteOp::switchOutputToSprite(selector.areaValue(), selector.area(),
                                       selector.id(), saveArea,
                                       word0, word1, word2, word3);
    if (err != nullptr)
        return err;

    context.setRaw(uint32_t(word0), uint32_t(word1),
                   uint32_t(word2), uint32_t(word3));
    return nullptr;
}

MyError switchOutputToMask(const Selector& selector,
                           VDUSaveArea* saveArea,
                           VDUContext& context)
{
    int word0;
    int word1;
    int word2;
    int word3;
    MyError err = OSSpriteOp::switchOutputToMask(selector.areaValue(), selector.area(),
                                     selector.id(), saveArea,
                                     word0, word1, word2, word3);
    if (err != nullptr)
        return err;

    context.setRaw(uint32_t(word0), uint32_t(word1),
                   uint32_t(word2), uint32_t(word3));
    return nullptr;
}

MyError removeLeftHandWastage(const Selector& selector)
{
    return OSSpriteOp::removeLeftHandWastage(selector.areaValue(),
                                              selector.area(),
                                              selector.id());
}

} } // namespace riscos::Sprite
