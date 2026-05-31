#ifndef PDRIVERDP_CONSTANTS_H
#define PDRIVERDP_CONSTANTS_H

/* PDriverDP.Constants -> C */

/* Offset from dump depth to the start of the string info block. */
#define dp_data_dlm 12u

// Pixels are represented at 2x OS units.
// shift by 1 to convert pixels <-> OS units
#define bufferpix_l2size 1u
#define bufferpix_mask ((1u << bufferpix_l2size) - 1)

#endif
