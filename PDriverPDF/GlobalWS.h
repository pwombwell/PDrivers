#ifndef PDRIVERPDF_GLOBALWS_H
#define PDRIVERPDF_GLOBALWS_H

#include <stdint.h>

#include "PDriverPDF.h"
#include "Core/Workspace.h"

#include "Core/InterceptColTrans.h"
#include "RLib/OS/Sprite.h"
#include "RLibX/OS.h"

/* PDriverPDF.GlobalWS -> C */

class ColourTransBlock32K;

class PDriverWS : public CoreWS
{
public:
    PDriverWS(void* pw) : CoreWS(pw) {}

    static PDriverWS& instance() { return *(PDriverWS*)Module::instance(); }

private:
    PDriverWS(const PDriverWS&);
    PDriverWS& operator=(const PDriverWS&);



public:
    // Needs tidying below:


    char expansionbuffer[256];

    /* Temporary workspace for JPEGs, fonts, VDU 5. */
    struct GlobalTempWS {

        struct {
            Sprite::Area* jpeg_buffer_sprite;
            Sprite::VDUContext jpeg_buffersp_save;
        } jpegPlotting;

        struct {
#if !PSTextSpeedUps
            FontHandle currentPSfont;
#endif

            uint32_t pendingrubout_rgb;
            OS::Millipoint pendingrubout_left;
            uint32_t enumeration_buf[3];

        } fontPlotting;

        struct {
            char chardefnblock[9];
        } vdu5Plotting;
    } globaltempws;

    /* Temporary sprite area. */
    struct {
        Sprite::Area area;

#if Medusa
        // Space for two unmasked sprites, each
        // 64 rows x 256 bits, plus their sprite headers.
        Sprite::Header data;
        uint8_t dataArray[64 * (256 / 8)];
        Sprite::Header mskdata;
        uint8_t mskdataArray[64 * (256 / 8)];
#else
        // Space for sprite data and mask, each
        // 64 rows by 256 bits, plus sprite header.
        Sprite::Header data;
        uint8_t dataArray[2 * 64 * (256 / 8)];
#endif
    } sprarea;

    // An array containing bounds for the position of each colour in the current
    // sprite chunk, used to cut down the amount of output produced. The data
    // concerned is squashed into a word per possible colour, the four bytes of
    // the word containing the left, bottom, right and top bounds in most to
    // least significant order.
    uint32_t colourbounds[0x101];
};

#endif
