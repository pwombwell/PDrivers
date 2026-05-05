#ifndef COMMON_COLOUR_PIXEL_VALUES_H
#define COMMON_COLOUR_PIXEL_VALUES_H

#include <stdint.h>

class ColourPixelValues {
public:
    ColourPixelValues();

    uint32_t get(uint8_t i) const { return m_table[i]; }

    uint8_t rgbToPixelValue(uint32_t bbGGRR00);

private:
    uint32_t m_table[256];
};

#endif
