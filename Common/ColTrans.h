#ifndef COMMON_COLTRANS_H
#define COMMON_COLTRANS_H
#pragma once

#include "RLib/ColourTrans.h"
#include "RLib/Geometry/Size.h"
#include "RLib/Geometry/Rect.h"
#include "RLib/OS/Sprite.h"
#include "RLibX/Sprite.h"

class PDriverVWorkspace;

// Fake a 3-word ColourTrans 32K block which is responsible for its own
// allocation via malloc.
struct GreyTable32K : ColourTrans::Table32K {
    GreyTable32K() : m_storage(nullptr), m_entries(nullptr) {}
    ~GreyTable32K();

    // 32K case.
    bool init32K(uint32_t numColours);

    MyError makeTable(OS::Mode mode);

    uint8_t* entries() const { return m_entries; }

private:
    // 32K+ case.
    bool init32KPlus(uint32_t numColours, uint32_t modeFlags, uint32_t log2bpp);

private:
    uint8_t* m_storage;
    uint8_t* m_entries; // Easy access to colour data, skipping any header.
};

struct ColourMapping {
    // Colour mapping starts off as a ColourTrans lookup (or none).
    // sprite_checkR5bit4 can convert it to a Palette, if there is one.
    // makeTable can convert it to a GeneratedGreyTable.
    ColourMapping(const ColourTrans::Table* table)
        : kind(table == nullptr ? None : ColourTrans)
        , table(table)
    {}

    MyError makeTable(Sprite::ModeWord modeWord) {
        kind = GeneratedGreyTable;
        return generated.makeTable(modeWord);
    }

    const uint8_t* byteTable() const {
        return static_cast<const ColourTrans::ByteTable*>(table)->bytes();
    }

    enum Kind {
        None,
        ColourTrans,
        Palette,
        GeneratedGreyTable
    };

    Kind                        kind;

    // `palette` and `table` are initially passed in by the caller.
    Sprite::Palette             palette;   // Used in preference to colourtrans table.
    const ColourTrans::Table*   table;     // Passed in by caller.

    GreyTable32K                generated; // Only to be used if palette == nullptr.
};

MyError sprite_checkR5bit4(const Sprite::Selector& sprite,
                           Sprite::PlotAction plotAction,
                           ColourMapping& mapping);

#if Medusa
uint32_t sprite_map565(uint32_t value, int rgb);
uint32_t sprite_map444(uint32_t value, int rgb);
uint32_t sprite_map555(uint32_t value, int rgb);
#endif

#endif
