#ifndef PDRIVERPS_GLOBALWS_H
#define PDRIVERPS_GLOBALWS_H

#include <stdint.h>

#include "PDriverPS.h"
#include "RLEncode.h"
#include "Common/Ascii85.h"
#include "Core/Workspace.h"

#include "RLibX/Sprite.h"

/* PDriverPS.GlobalWS -> C */

class ColourTransBlock32K;

// Not yet sure where this should go - presumably become an object.
struct SpriteWorkspace {
    // The use of this temporary workspace during sprite plotting.
    const Sprite::Header* spriteaddress;
    uint32_t spritelog2bpp;
};

class PDriverWS : public CoreWS
{
public:
    PDriverWS(void* pw) : CoreWS(pw) {}

    static PDriverWS& instance() { return *(PDriverWS*)Module::instance(); }

private:
    PDriverWS(const PDriverWS&);
    PDriverWS& operator=(const PDriverWS&);

public:
    char expansionbuffer[256];

    /* Temporary workspace for sprites, JPEGs, fonts, VDU 5. */
    struct GlobalTempWS {
        SpriteWorkspace spritePlotting;

        struct {
            Sprite::Area* jpeg_buffer_sprite;
            Sprite::VDUContext jpeg_buffersp_save;
        } jpegPlotting;


        struct {
            char chardefnblock[9];
        } vdu5Plotting;
    } globaltempws;

    // Global configuration (usually from !Printers).
    PS::Document::Flag  m_flags;
    PS::Document::Level m_level;

#if Medusa
    const Sprite::Header* spritemskaddress;
#endif

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
        SpriteOp::Header data;
        uint8_t dataArray[2 * 64 * (256 / 8)];
#endif
    } sprarea;

    /* Bounds for the position of each colour in current sprite chunk. */
    uint32_t colourbounds[0x101];
};

#endif
