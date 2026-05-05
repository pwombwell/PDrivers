#include "Core/PDriver.h"
#include "ColTrans.h"

#include "Core/ColourUtils.h"
#include "GlobalWS.h"

#include "RLib/OS.h"
#include "RLib/OS/Sprite.h"
#include "Norcroft/new.h"

enum { ThirtyTwo = 0x2E4B3233, ThirtyTwoPlus = 0x2B4B3233 };

// A ColourTrans 32K Plus header, potentially pointed to by m_header.
struct Header32kPlus {
    Header32kPlus(uint32_t nColour, uint32_t modeFlags, uint32_t log2bpp)
        : srcNColour(nColour), srcModeFlags(modeFlags), srcLog2BPP(log2bpp)
        , destCount(256), tableFormat(0), headerSize(sizeof(*this))
    { }

    uint32_t srcNColour;    // number of colours - 1
    uint32_t srcModeFlags;
    uint32_t srcLog2BPP;
    uint32_t destCount;     // Number of entries in destination palette
    uint16_t tableFormat;   // Zero for this format
    uint16_t headerSize;    // sizeof(Header32kPlus) == CTrans32Kplus_ExpectedSize

    uint8_t* tableData() const { return (uint8_t*)this + sizeof(*this); }
};

GreyTable32K::~GreyTable32K()
{
    // Contained by parent class, but it's not yet clear who owns that block.
    // By contrast, the table in this subclass was allocated by this.
    delete [] m_table;
}

bool GreyTable32K::init32KPlus(uint32_t numColours,
                               uint32_t modeFlags,
                               uint32_t log2bpp)
{
    // Allocate a table that's the Header32kPlus, plus a byte per colour.
    uint32_t totalBytes = numColours + sizeof(Header32kPlus);
    m_storage = new uint8_t[totalBytes];
    if (m_storage == nullptr)
        return false;

    // Placement new to construct the header.
    Header32kPlus* header = new (m_storage) Header32kPlus(numColours-1, modeFlags, log2bpp);
    setGuardWords(ThirtyTwoPlus);

    m_entries = header->tableData();
    setTable(reinterpret_cast<ColourTrans::ByteTable*>(m_entries));

    return true;
}

bool GreyTable32K::init32K(uint32_t numColours) {
    // Allocate a table that's a byte per colour.
    m_storage = new uint8_t[numColours];
    if (m_storage == nullptr)
        return false;

    setGuardWords(ThirtyTwo);

    m_entries = m_storage;
    setTable(reinterpret_cast<ColourTrans::ByteTable*>(m_entries));

    return true;
}

MyError GreyTable32K::makeTable(OS::Mode mode)
{
    Sprite::ModeWord modeWord(mode);
    Sprite::Type spriteType = modeWord.type();

    bool use32kPlus = false;
    if (spriteType != Sprite::Type_Bpp16 && spriteType != Sprite::Type_Bpp32)
        use32kPlus = true;

    MyError err;
    uint32_t nColour;
    if ((err = OS::xreadModeVariable(mode, OS::ModeVar_NColour, nColour)) != nullptr)
        return err;

    uint32_t modeFlags;
    if ((err = OS::xreadModeVariable(mode, OS::ModeVar_ModeFlags, modeFlags)) != nullptr)
        return err;

    uint32_t log2bpp;
    if ((err = OS::xreadModeVariable(mode, OS::ModeVar_Log2BPP, log2bpp)) != nullptr)
        return err;

    nColour++; // NColour is 1 less. Note 32bpp modes return -1 (Why?!)
    if (nColour == 0) {
        // 32bpp, so 16M colours.
        nColour = 32*1024;  // 16M can use a more reasonable 32k
        use32kPlus = false; //     and headerless '32K.'
        modeFlags &= ~ModeFlag_DataFormatSub_RGB; //and always BGR
    }

    if (nColour == 64*1024 && (modeFlags & ModeFlag_64k) != 0u)
        nColour = 32*1024;  // Discriminate 5:6:5 and 5:5:5

    uint32_t (*map_fn)(uint32_t, int);
    if (nColour < 32*1024)
        map_fn = sprite_map444;
    else if (nColour == 32u * 1024u)
        map_fn = sprite_map555;
    else
        map_fn = sprite_map565;

    bool rgbFlag = (modeFlags & ModeFlag_DataFormatSub_RGB) != 0;

    if (use32kPlus) {
        if (!init32KPlus(nColour, modeFlags, log2bpp))
            return MyError::OOM();
    } else if (!init32K(nColour))
        return MyError::OOM();

    uint8_t* tableEntry = entries();
    for (int32_t col = nColour - 1; col >= 0; --col) {
        uint32_t bgr = map_fn((uint32_t)col, rgbFlag);
        uint8_t grey = ColourUtils_rgbtogrey(bgr << 8);
        tableEntry[col] = grey;
    }

    return nullptr;
}

// This routine is used by the sprite plotting routines in this file and
// TransSprite.  It checks to see if bit 4 of R5 is set, and if so
// it arranges to use the sprite's palette in preference to the
// translation table, unless the palette is nonexistent or the wrong size.
//
// It does this by reading the sprite's palette, to return it, but sets it
// to nullptr if is the wrong size.
MyError sprite_checkR5bit4(const Sprite::Selector& sprite,
                           Sprite::PlotAction plotAction,
                           ColourMapping& mapping)
{
    if (!(plotAction & Sprite::PlotAction_UsePalette))
        return nullptr;

    MyError err;
    OS::Mode mode;
    Sprite::Palette palette;

    if ((err = Sprite::readPalette(sprite, mapping.palette, mode)) != nullptr)
        return err;

    uint32_t log2bpp;
    if ((err = OS::xreadModeVariable(mode, OS::ModeVar_Log2BPP, log2bpp)) != nullptr)
        return err;

     // If the sprite has no palette, or if it's 16 or 32bpp, then this is all
     // irrelevant - we use a different strategy so return.
    if (palette.size() == 0 || log2bpp == 4 || log2bpp == 5)
        return nullptr;

    uint32_t bpp = 1u << log2bpp; // bpp in sprite's mode
    uint32_t colours = 1u << bpp; // max num of colours in sprite's mode

    // FIXME: 256 colour sprites with 64-entry palettes are not supported.
    // (as in the ARM original)
    if (palette.size() != colours)
        return nullptr;

    mapping.kind = ColourMapping::Palette;
    mapping.palette = palette;

    return nullptr;
}

#if Medusa
uint32_t sprite_map444(uint32_t value, int rgb) {
    uint32_t r = (value >> 0) & 0xFu;
    uint32_t g = (value >> 4) & 0xFu;
    uint32_t b = (value >> 8) & 0xFu;
    if (rgb) {
        uint32_t tmp = r;
        r = b;
        b = tmp;
    }
    r = (r << 4) | r;
    g = (g << 4) | g;
    b = (b << 4) | b;
    return (b << 16) | (g << 8) | r;
}

uint32_t sprite_map555(uint32_t value, int rgb) {
    uint32_t r = (value >> 0) & 0x1Fu;
    uint32_t g = (value >> 5) & 0x1Fu;
    uint32_t b = (value >> 10) & 0x1Fu;
    if (rgb) {
        uint32_t tmp = r;
        r = b;
        b = tmp;
    }
    r = (r << 3) | (r >> 2);
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);
    return (b << 16) | (g << 8) | r;
}

uint32_t sprite_map565(uint32_t value, int rgb) {
    uint32_t r = (value >> 0) & 0x1Fu;
    uint32_t g = (value >> 5) & 0x3Fu;
    uint32_t b = (value >> 11) & 0x1Fu;
    if (rgb) {
        uint32_t tmp = r;
        r = b;
        b = tmp;
    }
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);
    return (b << 16) | (g << 8) | r;
}

#endif
