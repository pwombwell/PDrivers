#ifndef CORE_FONTPAINT_H
#define CORE_FONTPAINT_H

#include "PDriver.h"

#include "FontPaintState.h"
#include "RLib/Geometry/Rect.h"

enum FontPaintControlCode {
    FontPaintControl_End = 0,
    FontPaintControl_HorizontalMove = 9,
    FontPaintControl_LineFeed = 10,
    FontPaintControl_VerticalMove = 11,
    FontPaintControl_CarriageReturn = 13,
    FontPaintControl_OneColour = 17,
    FontPaintControl_AllColours = 18,
    FontPaintControl_AbsoluteColours = 19,
    FontPaintControl_Comment = 21,
    FontPaintControl_Underline = 25,
    FontPaintControl_SetFont = 26,
    FontPaintControl_Matrix4 = 27,
    FontPaintControl_Matrix6 = 28
};

/* Structure representing R5 on entry to Font_Paint. */
struct FontPaintCoords
{
    FontPaintAdjustments m_adjustments;
    FontRect             m_ruboutBox;
};

#endif
