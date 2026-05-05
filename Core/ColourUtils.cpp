#include "PDriver.h"
#include "ColourUtils.h"

// A useful subroutine to convert an RGB value to a grey scale value.
// Formula used: grey = (77*red + 150*green + 28*blue)/255
// Entry: R1 = &BBGGRR00
// Exit:  R1 = grey (in range 0-255)
//
// Multiplications by 77 and 150 are done by multiply instructions to save
// code. The division by 255, with rounding, is done by a trick that works on
// numbers in the required range. This trick is:
//
// Provided 0 <= X <= &FF00, X DIV &FF = (&101*X + &100) DIV &10000.
//
// Proof: X/&FF = (&101*X)/&FFFF
//              = (&101*X)/&10000 + ((&101*X)/&FFFF)/&10000
//             <= (&101*X)/&10000 + &100/&10000, as X <= &FF00
//              = (&101*X + &100)/&10000
//              < (&101*X + &101)/&FFFF
//              = (&101*(X+1))/&FFFF
//              = (X+1)/&FF
// Therefore X DIV &FF <= (&101*X + &100) DIV &10000 <= (X+1) DIV &FF,
// with the second inequality being strict if X+1 is a multiple of &FF. But
// (X+1) DIV &FF = (X DIV &FF) + 1 if X+1 is a multiple of &FF and
// (X+1) DIV &FF = X DIV &FF otherwise, yielding the required result either
// way.
//
// alternative entry point for colour_rgbtogray that expects
// the colours to be in r1, r2 and r3 already
static inline uint8_t rgbcomponentstogrey(uint32_t blue,
                                          uint32_t green,
                                          uint32_t red)
{
    uint32_t x = 77u * red + 150u * green + 28u * blue;
    x += 0x7Fu;
    x = x + (x << 8) + 0x100u;
    return (uint8_t)((x >> 16) & 0xFFu);
}

uint8_t ColourUtils_rgbtogrey(uint32_t bbGGRR00) {
    uint32_t green = (bbGGRR00 >> 16) & 0xFFu;
    uint32_t red = (bbGGRR00 >> 8) & 0xFFu;
    uint32_t blue = (bbGGRR00 >> 24) & 0xFFu;
    return rgbcomponentstogrey(blue, green, red);
}
