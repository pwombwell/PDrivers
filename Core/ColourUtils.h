#include "PDriver.h"

/* Convert &BBGGRR00 to grey scale using (77*R + 150*G + 28*B)/255. */
uint8_t ColourUtils_rgbtogrey(uint32_t bbGGRR00);
