#ifndef RLIBX_SPRITE_H
#define RLIBX_SPRITE_H

#pragma once

#include "RLib/RLib.h"

#include "RLib/OS/Sprite.h"
#include "RLib/Geometry.h"
#include "RLib/Base/Fixed.h"
#include "RLibX/Utils/EnumBitmaskOps.h"

#include "OS.h"

namespace riscos {
namespace ColourTrans { struct Table; }

namespace Sprite {
class Palette;

// SpriteOp Plot flags
// https://www.riscosopen.org/wiki/documentation/show/OS_SpriteOp%20Scaled%2FTransformed%20Plot%20Flags
enum PlotAction {
    PlotAction_GCOLMask             = 7,     // GCOL action (OS::GCOLAction)
    PlotAction_UseMask              = 1u<<3, // Use sprite mask/alpha channel
    
    PlotAction_IgnoreColTrans       = 1u<<4, // Translation table can be ignored
    PlotAction_UsePalette           = 1u<<4, // ie. alias for ignore colour trans table.

    PlotAction_WideColTrans         = 1u<<5, // Translation table is wide
    PlotAction_Dither               = 1u<<6, // Enable dithering
    PlotAction_ColourMap            = 1u<<7, // is a pointer to a Colour Mapping Descriptor

    PlotAction_TranslucencyShift    = 8u,      // Translucency (0 = opaque, 255 = 1/256 visibility)
    PlotAction_TranslucencyMask     = 0xffu<<8 // Translucency (0 = opaque, 255 = 1/256 visibility)
};
DEFINE_ENUM_BITWISE_OPERATORS(PlotAction);

// RISC OS Sprite header view.
class HeaderView {
public:
    HeaderView(const Header* header) : m_header(*header) { }
    HeaderView(const Header& header) : m_header(header) { }

    operator const Header&() const { return m_header; }
    const Header* ptr() const { return &m_header; }

    bool hasPalette() const;
    Palette palette() const;

    uint8_t* imagePtr() const { return (uint8_t*)&m_header + m_header.imageOffset; }
    bool hasMask() const { return m_header.maskOffset > 0 && m_header.maskOffset != m_header.imageOffset; }

    // Returns a pointer to a mask, or nullptr if none (mask offset == image offset).
    uint8_t* maskPtr() const { return hasMask() ? (uint8_t*)&m_header + m_header.maskOffset : nullptr; }
    uint32_t rowBytes() const { return (m_header.widthWordsM1 + 1) * 4; }

    const OS::Mode& mode() const { return m_header.mode; }

    // Note: Width (in pixels) requires the number of bpp to be known.
    // It may be easier to use Sprite::Info.
    uint32_t width(uint8_t bpp) const { return (m_header.widthWordsM1*32 + m_header.rightBit + 1 - m_header.leftBit) / bpp; }
    uint32_t height() const { return m_header.heightM1 + 1; }

private:
    friend class Palette;
    size_t paletteSize() const { return min(m_header.imageOffset, m_header.maskOffset)-sizeof(Header); }

    uint32_t min(uint32_t x, uint32_t y) const { return x < y ? x : y; }

private:
    const Header& m_header;
};
inline HeaderView Header::view() const { return HeaderView(this); }

// RISC OS Sprite Area view.
class AreaView {
public:
    AreaView(Area* area) : m_area(area) { (void)*this; }

    operator Area*() const { return m_area; }
    Area* ptr() const { return m_area; }

    Header* firstSprite() const {
        if (m_area == nullptr) return nullptr;
        return (Header*)((uint8_t*)(m_area) + m_area->first);
    }

    // Initialise, or clear, a sprite area of given size. Any extensions block is
    // immediately after the Sprite::Area so added to the offset to first byte.
    MyError initialise(uint32_t size, size_t extensions = 0);

    // Initialise, or clear, a sprite area without using the OS_SpriteOp call.
    void initialiseCheekily(uint32_t size, size_t extensions = 0);

private:
    Area* m_area;
};
inline AreaView Area::view() { return AreaView(this); }

// RISC OS Sprite Area view.
class ConstAreaView {
public:
    ConstAreaView(const Area* area) : m_area(area) { (void)*this; }
    ConstAreaView(const AreaView& view) : m_area(view.ptr()) { (void)*this; }

    operator const Area*() const { return m_area; }
    const Area* ptr() const { return m_area; }

    const Header* firstSprite() const {
        if (m_area == nullptr) return nullptr;
        return (Header*)((uint8_t*)(m_area) + m_area->first);
    }

private:
    const Area* m_area;
};
inline ConstAreaView Area::view() const { return ConstAreaView(this); }

struct ScaleFactor {
    uint32_t multiplierX;
    uint32_t multiplierY;
    uint32_t divisorX;
    uint32_t divisorY;
};

// Not sure about this - currently Id has the constructors.
// class IdView {
// public:
//     IdView(Id& id) : m_id(id) { }
// 
//     const char* name() const { return m_id.u.m_name; }
//     Header* header() const { return m_id.u.m_header; }
// 
// private:
//     Id& m_id;
// };

// Struct that contains r0, r1 and r2, for entry to many OS_SpriteOp calls.
class ResolvedSelector;
class Selector {
public:
    Selector();// : m_area(nullptr), m_id((Header*)nullptr) { }

    Selector(uint32_t reason, const void* area, const void* nameOrPtr)
        : m_areaValue(AreaValue(reason & ~0xff))
        , m_area((Area*)(m_areaValue ? area : nullptr))
        , m_id((const char*)nameOrPtr) // any ctor would be safe.
    { }

    // Select a named sprite in the System sprite area.
    Selector(const char* name)
        : m_areaValue(AreaValue_System), m_area(nullptr), m_id(name)
    { }

    // Select a named sprite in the given sprite area.
    Selector(Area* area, const char* name)
        : m_areaValue(AreaValue_UserName) , m_area(area), m_id(name)
    { }

    // Select a sprite by pointer in the given sprite area.
    Selector(Area* area, const Header* header)
        : m_areaValue(AreaValue_UserPtr) , m_area(area), m_id(header)
    { }

    // Select a sprite by id in the given sprite area.
    // 'reason' identifies whether id is a name or pointer.
    Selector(AreaValue areaValue, Area* area, Id id)
        : m_areaValue(AreaValue(areaValue)), m_area(area), m_id(id)
    { }

    AreaValue areaValue() const { return m_areaValue; }
    AreaView area() const { return AreaView(m_area); }
    Id id() const { return m_id; }

    bool isSystemSprite() const;

    // Resolve this selector's area only (if system sprites), and update the
    // supplied reason code to point to a user area.
    MyError resolve(uint32_t& reason);

    // Create a new selector that points directly at a sprite in a sprite area.
    //
    // If given a system sprite area, it sets the area to the start of the
    // system sprite dynamic area.
    MyError resolve(ResolvedSelector& out) const;

protected:
    AreaValue   m_areaValue;
    Area*       m_area;
    Id          m_id;
};

// A Selector that can only point to a sprite and its sprite area directly.
//
// The layout of the sprite area must not be modified when using pointers.
class ResolvedSelector : public Selector {
public:
    ResolvedSelector() : Selector((Area*)nullptr, (Header*)nullptr) { }
    ResolvedSelector(Area* area, const Header* header)
        : Selector(area, header)
    { }

    // Will always a pointer or nullptr if it's been invalidated.
    const Header* header() const { return m_id.u.m_header; }

    // Invalidate, because the sprite area may have been modified.
    void invalidate() { m_id = Id(); }
};

/// Gets and stores the result of OS_ReadSpriteInfo.
class Info {
public:
    Info() {}
    Info(uint32_t width, uint32_t height, OS::Mode mode, bool hasMask)
        : m_size(width, height), m_mode(mode), m_hasMask(hasMask)
    {}

    /// Get the sprite info using the given Selector.
    MyError get(const Selector& sprite);

    // Return width/height in pixels (unlike the Header).
    uint32_t width() const { return m_size.width; }
    uint32_t height() const { return m_size.height; }
    const Geometry::Size<uint32_t>& size() const { return m_size; }
    OS::Mode mode() const { return m_mode; }
    bool hasMask() const { return m_hasMask; }

    void setWidth(uint32_t width) { m_size.width = width; }
    void setHeight(uint32_t height) { m_size.height = height; }
    void setSize(const Geometry::Size<uint32_t>& size) { m_size = size; }
    void setMode(OS::Mode mode) { m_mode = mode; }
    void setHasMask(bool hasMask) { m_hasMask = hasMask; }

private:
    Geometry::Size<uint32_t>  m_size;
    OS::Mode        m_mode;
    bool            m_hasMask;
};

// https://www.riscosopen.org/wiki/documentation/show/Sprite%20Mode%20Word
// Sprites can only store a Sprite::ModeWord (which can contain a mode number),
// but many APIs also accept a ModeSpecifier.
// NYI: pre-RO3.5 getters for dpi, etc.
struct ModeWord3;
struct ModeWord5;

struct ModeWord {
    ModeWord(OS::Mode mode) : m_word(mode.toNumber()) { }
    ModeWord(uint32_t mode) : m_word(mode) { }

    typedef OS::Mode OSMode; // work around a norcroft bug
    operator OSMode() const { return OS::Mode(m_word); }

    bool isModeNumber() const { return m_word > 256; }

    bool hasWideMask() const { return (m_word >> 31) != 0; }
    Type type() const;

    uint16_t dpiH() const;
    uint16_t dpiV() const;

    // Note that an eigen value of 3 is considered equal to 22dpi and 23dpi.
    uint8_t eigX() const;
    uint8_t eigY() const;

    uint16_t modeFlags() const;

    bool isRO5() const { return ((m_word >> 27) & 0x0f) == 0xf; }

    // The RO3/RO5-specific APIs intentionally don't check validity.
  
    static uint16_t eigToDPI(uint8_t eig) { return 180 >> (eig & 3); }
    static uint8_t dpiToEig(uint16_t dpi) { // use midpoints between values
        if (dpi >= 135) return 0;  // eg. 180dpi = eig 0.
        if (dpi >=  68) return 1;  // eg. 90dpi  = eig 1.
        if (dpi >=  34) return 2;  // eg. 45dpi  = eig 2.
        return 3; // eg. 22dpi or 23dpi = eig 3.
    }

    static ModeWord fromModeWord3(Type type, uint16_t dpiH, uint16_t dpiV);
    static ModeWord fromModeWord5(Type type, uint8_t eigX, uint8_t eigY,
                                  uint16_t modeFlags = 0);

    uint32_t word() const { return m_word; }

private:
    ModeWord3 to3() const;
    ModeWord5 to5() const;

protected:
    uint32_t m_word;
};

struct ModeWord3 {
    ModeWord3(ModeWord word) : m_word(word.word()) { }
    ModeWord3(Type type, uint16_t dpiH, uint16_t dpiV)
        : m_word((uint32_t(type) << 27) | (uint32_t(dpiV) << 14) | dpiH << 1 | 1)
    { }

    operator ModeWord() const { return ModeWord(m_word); }

    Type type() const { return Type((m_word >> 27) & 0x0f); }

    uint8_t dpiH() const { return (m_word >>  1) & 0x13; }
    uint8_t dpiV() const { return (m_word >> 14) & 0x13; }

    uint16_t modeFlags() const {
        switch (type()) {
            case 7:  return 0x1000; // KYMC (CYMK little-endian)
            case 10: return 1u<<7;  // 65536 cols RGB 5:6:5 mode (not FullPalette)
            default: return 0;
        }
    }

    ModeWord modeWord() const { return ModeWord(m_word); }

private:
    uint32_t m_word;
};

struct ModeWord5 {
    ModeWord5(ModeWord word) : m_word(word.word()) { }
    ModeWord5(Type type, uint8_t eigX, uint8_t eigY, uint16_t modeFlags = 0)
        : m_word((uint32_t(type) << 20) | (uint32_t(eigY & 3) << 6)
                 | (uint32_t(eigX & 3) << 4) | (modeFlags & 0xff00u)
                 | (0xfu << 27))
    { }

    operator ModeWord() const { return ModeWord(m_word); }

    Type type() const { return Type((m_word >> 20) & 0x3f); }

    uint8_t eigX() const { return (m_word >> 4) & 3; }
    uint8_t eigY() const { return (m_word >> 6) & 3; }

    uint16_t dpiH() const { return 180 >> (eigX() & 3); }
    uint16_t dpiV() const { return 180 >> (eigY() & 3); }

    uint16_t modeFlags() const { return m_word & 0xff00u; }

    ModeWord modeWord() const { return ModeWord(m_word); }

private:
    uint32_t m_word;
};

// Encapsulates a pointer to a palette (OS::ColourPair*) and its size.
class Palette {
public:
    Palette() : m_pairs(nullptr), m_entries(0) { }

    Palette(const OS::PaletteBase* base, uint32_t entries) : m_pairs(base), m_entries(entries) { }

    Palette(const HeaderView& sprite)
        : m_pairs((const OS::ColourPair*)addr(sprite.ptr()))
        , m_entries(sprite.paletteSize() / 8)
    {
        if (!isValidPaletteSize(sprite.paletteSize()))
            invalidate();
    }

    bool isEmpty() const { return m_entries == 0; }

    const OS::ColourPair& operator[] (size_t i) const { return m_pairs[i]; };
    uint32_t size() const { return m_entries; }

    void invalidate() { m_pairs = nullptr; m_entries = 0; }

    static bool isValidPaletteSize(size_t size) {
        return size == 16*8 || size == 64*8 || size == 256*8;
    }

private:
    static const uint8_t* addr(const Header* sprite) {
        return ((const uint8_t*)sprite) + sizeof(Header);
    }

private:
    const OS::ColourPair*   m_pairs;
    uint32_t                m_entries;
};

// Context to store return redirection to the previous destination.
class VDUContext {
public:
    VDUContext() { unset(); }
    VDUContext(uint32_t reason, const Selector& selector, VDUSaveArea* saveArea) {
        u.s.reason = selector.areaValue() | reason;
        u.s.area = selector.area();
        u.s.id = selector.id().name(); // either.
        u.s.saveArea = saveArea;
    }
    VDUContext(uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3) {
        u.v[0] = word0; u.v[1] = word1; u.v[2] = word2; u.v[3] = word3;
    }

    VDUContext(const VDUContext& rhs) {
        u.v[0] = rhs.u.v[0]; u.v[1] = rhs.u.v[1];
        u.v[2] = rhs.u.v[2]; u.v[3] = rhs.u.v[3];
    }

    bool operator== (const VDUContext& rhs) const {
        return u.s.reason == rhs.u.s.reason &&
               u.s.area == rhs.u.s.area &&
               u.s.id == rhs.u.s.id &&
               u.s.saveArea == rhs.u.s.saveArea;
    }

    // If there's no Sprite id then it's redirecting to screen (unset).
    bool isUnset() const { return u.v[2] == 0; }

    void unset() { u.v[0] = 0; }
    uint32_t rawWord(uint32_t index) const { return u.v[index]; }
    void setRaw(uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3) {
        u.v[0] = word0; u.v[1] = word1; u.v[2] = word2; u.v[3] = word3;
    }

    // Set default "output directed to screen" parameters.
    // Does not change redirection (use restore()).
    void setToScreen();

    MyError restore() const;

    // A Sprite::Id is always stored in absolute form in a VDUContext.
    Sprite::Id spriteId() const { return Sprite::Id((const Sprite::Header*)u.s.id); }

private:
    union {
        uint32_t v[4];
        struct {
            uint32_t        reason; // inc. AreaValue
            Area*           area;
            const void*     id;
            VDUSaveArea*    saveArea;
        } s;
    } u;
};

class ScopedVDUContext : public VDUContext
{
public:
    ScopedVDUContext() : m_set(false) {}
    ~ScopedVDUContext() {
        if (m_set)      // If set, restore to previous (probably to screen).
            restore();
    }

    // Can be restored on-demand, or on automatically when leaving scope.
    MyError restore() { m_set = false; return VDUContext::restore(); }

    // Acorn's PDrivers don't use a save area. Can overload if necessary.
    MyError switchOutputToSprite(const Sprite::Selector& sprite);
    MyError switchOutputToMask(const Sprite::Selector& sprite);

private:
    bool m_set;
};

// Coords block for PlotTransformed. Entries are the screen coordinates of a
// parallelogram to transform the sprite, in OS::Units [24:8].
// https://www.riscosopen.org/wiki/documentation/show/OS_SpriteOp%2056
struct Coords {
    Geometry::Point<Base::Fixed<8> > points[4]; // Note Norcroft requires "> >".

    Geometry::Point<Base::Fixed<8> >& operator[](size_t i) { return points[i]; }
    const Geometry::Point<Base::Fixed<8> >& operator[](size_t i) const { return points[i]; }

//    Coords& operator-=(const Geometry::Offset<Base::Fixed<8> >& rhs) {
//        points[0] -= rhs; points[1] -= rhs; points[2] -= rhs; points[3] -= rhs;
//    }
};

enum TransformFlag {
    TransformFlag_None          = 0,
    TransformFlag_Parallelogram = 1u<<0, // Sprite::Coords in r3, otherwise Draw::Matrix.
    TransformFlag_SourceRect    = 1u<<1  // Sprite::PixelRect in r4.
};

// OS calls related to Sprites -------------------------------------------------
// Read System Sprite area information (OS_ReadDynamicArea 3)
MyError getSystemInfo(Area*& base, size_t& size, size_t& sizeLimit);
MyError getSystemBase(Area*& base);
MyError getSystemSize(size_t& size);

// OS_SpriteOp calls -----------------------------------------------------------
// Initialises sprite area
//
// In:      r0 - areaValue
//          r1 - area
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 9 (InitSpriteArea)

MyError initArea(Sprite::AreaValue areaValue, Sprite::Area* area);

// Creates sprite
//
// In:      r0 - selector.areaValue()
//          r1 - selector.area()
//          r2 - selector.id() - must be a string.
//          r3 - createPalette
//          r4 - width
//          r5 - height
//          r6 - mode
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0xF

MyError create(const Sprite::Selector& selector, bool createPalette, int width, int height, OS::Mode mode);

// Selects sprite
//
// In:      r0 - selector.areaValue()
//          r1 - selector.area()
//          r2 - selector.id()
//
// Out:     r2 - header (X version only)
//
// Returns: r2 - header (non-X version only)
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0x18

MyError select(const Sprite::Selector& selector, Sprite::Header*& header);

// Deletes sprite
//
// In:      r0 - selector.areaValue()
//          r1 - selector.area()
//          r2 - selector.id()
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0x19 (DeleteSprite)

MyError deleteSprite(const Sprite::Selector& selector);

// Creates mask
//
// In:      r0 - selector.areaValue()
//          r1 - selector.area()
//          r2 - selector.id()
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0x1D

MyError createMask(const Sprite::Selector& selector);

// Puts sprite at user coordinates
//
// In:      r0 - selector.areaValue()
//          r1 - selector.area()
//          r2 - selector.id()
//          r3 - x
//          r4 - y
//          r5 - action
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0x22

MyError putAtUserCoords(const Sprite::Selector& selector, int x, int y, OS::GCOLAction action);

// Returns any palette.
//
// Note that an invalid palette size does not return an error code.
//
// In:      r0 - selector.areaValue()
//          r1 - selector.area()
//          r2 - selector.id()
//
// Out:     r3 - palette (palette.size())
//          r4 - palette (&palette[0])
//          r5 - mode
//
// Calls SWI OS_SpriteOp (0x2E) with R3 = -1, R0 = 37 (0x25)

MyError readPalette(const Sprite::Selector& selector, Sprite::Palette& palette, OS::Mode& mode);

// Reads sprite information
//
// In:      r0 - selector.areaValue()
//          r1 - selector.area()
//          r2 - selector.id()
//
// Out:     r3 - width
//          r4 - height
//          r5 - mask
//          r6 - mode
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0x28

MyError readInfo(const Sprite::Selector& selector,
                 int& width, int& height, bool& mask, OS::Mode& mode);

// Reads save area size
//
// In:      r0 - selector.areaValue()
//          r1 - selector.area()
//          r2 - selector.id()
//
// Out:     r2 - saveAreaBytes
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0x3E

MyError readSaveAreaSize(const Sprite::Selector& selector, int& saveAreaBytes);

// Plots mask scaled
//
// In:      r0 - selector.areaValue()
//          r1 - selector.area()
//          r2 - selector.id()
//          r3 - x
//          r4 - y
//          r6 - factors (or nullptr for 1:1)
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0x32

MyError plotMaskScaled(const Sprite::Selector& selector,
                       int x, int y, Sprite::ScaleFactor const& factors);

MyError plotMaskScaled(const Sprite::Selector& selector, int x, int y);

// Puts sprite scaled
//
// In:      r0 - selector.areaValue()
//          r1 - selector.area()
//          r2 - selector.id()
//          r3 - x
//          r4 - y
//          r5 - action
//          r6 - factors
//          r7 - colTrans
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0x34

MyError putScaled(const Sprite::Selector& selector, int x, int y,
                  Sprite::PlotAction action,
                  Sprite::ScaleFactor const* factors,
                  const ColourTrans::Table* colTrans);

// Switches output to sprite
//
// In:      r0 - selector.areaValue()
//          r1 - selector.area()
//          r2 - selector.id()
//          r3 - saveArea
//
// Out:     r0 - context[0]
//          r1 - context[1]
//          r2 - context[2]
//          r3 - context[3]
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0x3C

MyError switchOutputToSprite(const Sprite::Selector& selector,
                             VDUSaveArea* saveArea,
                             VDUContext& context);

// Switches output to mask
//
// In:      r0 - selector.areaValue()
//          r1 - selector.area()
//          r2 - selector.id()
//          r3 - saveArea
//
// Out:     r0 - context0
//          r1 - context1
//          r2 - context2
//          r3 - context3
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0x3D (SwitchOutputToMask)

MyError switchOutputToMask(const Sprite::Selector& selector,
                           VDUSaveArea* saveArea,
                           VDUContext& context);

// Removes left-hand wastage from sprite
//
// In:      r0 - selector.areaValue()
//          r1 - selector.area()
//          r2 - selector.id()
//
// Calls SWI OS_SpriteOp (0x2E) with R0 = 0x36

MyError removeLeftHandWastage(const Sprite::Selector& selector);

} } // namespace riscos::Sprite

#endif
