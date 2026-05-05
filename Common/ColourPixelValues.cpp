#include "ColourPixelValues.h"

#include "stdint.h"

// `managejob_allocate_inittable`
ColourPixelValues::ColourPixelValues()
{
    // Subroutine to initialise m_table if colour.
    // It constructs the red component from the bottom two bits of index
    // by multiplying by &55, green from the next two bits, and blue from
    // the next two bits.
    // Entry is set to 'undefined' if the top two bits of index are not set.

    for (int32_t i = 255; i >= 0; --i) { 
        uint32_t colour;

        colour  = (((i >> 0) & 3) * 0x55) << 8;
        colour |= (((i >> 2) & 3) * 0x55) << 16;
        colour |= (((i >> 4) & 3) * 0x55) << 24;

        // Check final top two bits to ensure they are set.
        if (((i >> 6) & 3) != 3)
            colour |= 0xff; // set to undefined

        m_table[i] = colour;
    }
}

// Routine to convert an RGB value into a "pixel value" suitable for use in
// sprite translation tables.
// For colour, a table of pixel values is used. If the requested colour
// appears in the table, it is returned. If it doesn't and there is still
// space in the table, a new entry is allocated for the new colour. Otherwise,
// the closest entry in the table is returned - "closest" being measured the
// same way as the ColourTrans module measures it.

// `colour_rgbtopixval_coloured` from PDriverPS/Colour.s
uint8_t
ColourPixelValues::rgbToPixelValue(uint32_t bbGGRR00)
{
    // We first try to find an exact match or, failing that, an empty slot in
    // m_table.

    bbGGRR00 &= ~0xff;  // Following code would be upset by a non-zero bottom byte.
    int32_t empty = -1; // No empty slot found yet.

    // `colour_rgbtopixval_search1`
    for (int32_t i = 255; i >= 0; --i) {
        uint32_t entry = m_table[i];

        if (entry == bbGGRR00)
            return (uint8_t)i;

        if ((entry & 0xff) != 0)
            empty = i;    // Remember slot if empty.
    }

    // Tests for empty slot. Store new entry and return if there's an empty slot.
    if (empty >= 0) {
        m_table[empty] = bbGGRR00;
        return (uint8_t)empty;
    }

    // There is no exact match and no empty slot. Find the closest match.
    uint32_t green = (bbGGRR00 >> 16) & 0xff;
    uint32_t red =   (bbGGRR00 >> 8)  & 0xff;
    uint32_t blue =  (bbGGRR00 >> 24) & 0xff;

    uint32_t best_dist = 0xffffffffu;
    int32_t best_idx = 0;

    for (int32_t i = 255; i >= 0; --i) {
        uint32_t entry = m_table[i];

        uint32_t g = (entry >> 16) & 0xff; // this entry's green
        uint32_t b = (entry >> 24) & 0xff; // this entry's blue
        uint32_t r = (entry >> 8)  & 0xff; // this entry's red

        uint32_t distG = green - g;
        uint32_t distB = blue  - b;
        uint32_t distR = red   - r;

        uint32_t dist = (3 * distG * distG) + (distB * distB) + (2 * distR * distR);

        if (dist < best_dist) {
            // If closer than before, record new closest colour.
            best_dist = dist;
            best_idx = i;
        }
    }

    return (uint8_t)best_idx;
}
