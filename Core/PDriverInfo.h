#ifndef CORE_PDRIVER_INFO_H
#define CORE_PDRIVER_INFO_H

#include <stddef.h>
#include <stdint.h>

#include "RLib/RLib.h"
#include "RLib/Geometry.h"
#include "RLibX/OS.h"
#include "RLibX/Utils/EnumBitmaskOps.h"

using namespace riscos::Geometry;

class MsgCode;
class SetPDriverInfo;

// Represents the PDriver_SetInfo registers.
class PDriverInfo {
public:
    PDriverInfo()
        : m_pixelResX(0)
        , m_pixelResY(0)
        , m_features((Features)0)
        , m_printerName(nullptr)
        , m_htoneResX(0)
        , m_htoneResY(0)
        , m_printer(0)
    {}
    PDriverInfo(const PDriverInfo& rhs);
    ~PDriverInfo();

    PDriverInfo& operator=(const SetPDriverInfo& rhs);

    const char* setName(const char*);

    // The token "none", not nullptr. This doesn't return errors, but instead
    // ensures the string is nullptr.
    uint32_t setNameToNone(MsgCode& msgs);

    bool isColour() const { return !!(m_features & Colour_Colour); }

    enum Features {
        Colour_Greys             = 0, // arbitrary grey levels
        Colour_Colour            = 1, // arbitrary colours  (actually a bitfield - all odd nums)
        Colour_FixedRangeColour  = 3, // fixed colour range - not very likely
        Colour_BlackAndWhite     = 4, // black and white
        Colour_PrimaryColours    = 5, // eight primary colours
        Colour_FiniteColourSet   = 7, // finite colour range - for instance XY plotter

        Plotting_FilledShapes        = 1u<<8,  // set => no   clear => yes
        Plotting_ThickLines          = 1u<<9,  // set => no   clear => yes
        Plotting_OverwritingPossible = 1u<<10, // set => no   clear => yes
        Plotting_TransformedSprites  = 1u<<11, // set => yes  clear => no
        Plotting_TransformedFonts    = 1u<<12, // set => yes  clear => no
        Plotting_DrawPageFlags       = 1u<<13, // set = PDriver_DrawPage has flags byte in R0, clear = not

        Features_ScreenDumps         = 1u<<24, // set => yes  clear => no
        Features_ArbitraryTransforms = 1u<<25, // set => yes  clear => no
        Features_InsertIllustration  = 1u<<26, // set => yes  clear => no
        Features_MiscOp              = 1u<<27, // set => yes  clear => no
        Features_SetDevice           = 1u<<28, // set => yes  clear => no
        Features_DeclareFont         = 1u<<29, // set => yes  clear => no

        Mask_Colour          = 0xFFu<<0,
        Mask_Plotting        = 0xFFu<<8,
        Mask_Reserved        = 0xFFu<<16,
        Mask_Features        = 0xFFu<<24
    };

    uint32_t    pixelResX() const    { return m_pixelResX; }
    uint32_t    pixelResY() const    { return m_pixelResY; }
    Features    features() const     { return m_features; }
    const char* printerName() const  { return m_printerName; }
    uint32_t    htoneResX() const    { return m_htoneResX; }
    uint32_t    htoneResY() const    { return m_htoneResY; }
    uint32_t    printer() const      { return m_printer; }

private:
    // Pixel resolution of printer (pixels/inch).
    uint32_t    m_pixelResX;
    uint32_t    m_pixelResY;

    Features    m_features;

    // Printer name.
    char*       m_printerName;

    // Halfone resolution (repeats/inch). If no halftoning is to be done,
    // these should equal pixelRes above.
    uint32_t    m_htoneResX;
    uint32_t    m_htoneResY;

    // Pointer to printer block.
    // In the original asm, this is incorrectly used to store printerName
    // when allocating a new job. I suspect `info_nameblock` was intended
    // to be used instead (albeit with a simpler memory lifecycle design).
    uint32_t    m_printer;
};
DEFINE_ENUM_BITWISE_OPERATORS(PDriverInfo::Features);

// Represents the PDriver_PageSize registers.
struct PDriverPageSize {
    Size<OS::Millipoint> size;
    Rect<OS::Millipoint> rect;
};

struct SetPDriverInfo {
    SetPDriverInfo(uint32_t pixelResX, uint32_t pixelResY, PDriverInfo::Features features,
                   const char* printerName,
                   uint32_t htoneResX, uint32_t htoneResY, uint32_t printer)
        : pixelResX(pixelResX)
        , pixelResY(pixelResY)
        , features(features)
        , printerName(printerName)
        , htoneResX(htoneResX)
        , htoneResY(htoneResY)
        , printer(printer)
    {}

    uint32_t                pixelResX;
    uint32_t                pixelResY;
    PDriverInfo::Features   features;
    const char*             printerName;
    uint32_t                htoneResX;
    uint32_t                htoneResY;
    uint32_t                printer;
};
#endif
