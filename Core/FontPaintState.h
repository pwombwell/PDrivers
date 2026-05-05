#ifndef CORE_FONTPAINTSTATE_H
#define CORE_FONTPAINTSTATE_H

#include "PDriver.h"

#include "Constants.h"

#include "RLib/Draw.h"
#include "RLib/Geometry.h"

using namespace riscos::Geometry;

typedef Offset<OS::Millipoint> FontOffset;
typedef Point<OS::Millipoint> FontPoint;
typedef Geometry::Rect<OS::Millipoint> FontRect;
typedef Geometry::Size<OS::Millipoint> FontSize;

struct FontPaintAdjustments
{
    Offset<OS::Millipoint> m_spaceAdd;
    Offset<OS::Millipoint> m_charAdd;
};

class FontPaintState
{
public:
    FontPaintState()
        : m_initialFont(0)
        , m_stringAddress(nullptr)
        , m_initialFlags(FontPaintFlag_None)
        , m_startPosition(0, 0)
        , m_paintPosition(0, 0)
        , m_paintEndPosition(0, 0)
        , m_font(0)
        , m_flags(FontPaintFlag_None)
        , m_underlinePosition(Base::Fixed<8>::fromRaw(0))
        , m_underlineThickness(Base::Fixed<8>::fromRaw(0))
        , m_adjustments()
        , m_ruboutStart(0)
        , m_ruboutEnd(0)
        , m_lastCharacter(0)
        , m_maxHorizontalOffset(0)
        , m_maxVerticalOffset(0)
        , m_spaceCount(0)
        , m_initialMatrix(Draw::Matrix::identity())
        , m_matrix(Draw::Matrix::identity())
    {
        m_ruboutBox.set(0, 0, 0, 0);
    }

    void setPaintEndOffset(FontOffset offset) {
        m_paintEndPosition = m_paintPosition + offset;;
    }

    void setCurrentFlagsFromMatrix() {
        if (m_matrix.isIdentity())
            m_flags = m_initialFlags;
        else
            m_flags = m_initialFlags | FontPaintFlag_Matrix;
    }

    void clipRuboutEndToPaintEnd(FontPaintFlag& initialFlags) {
        if (!(initialFlags & FontPaintFlag_Rubout))
            return;

        OS::Millipoint ruboutEnd = m_ruboutBox.x1;
        OS::Millipoint paintEnd = m_paintEndPosition.x;
        if (ruboutEnd > paintEnd) {
            ruboutEnd = paintEnd;
            initialFlags &= ~FontPaintFlag_Rubout;
        }

        m_ruboutEnd = ruboutEnd;
    }

    void finishPrintableChunk(FontPaintFlag initialFlags) {
        m_initialFlags = initialFlags;
        m_paintPosition = m_paintEndPosition;
        m_ruboutStart = m_ruboutEnd;
    }

    void setAdjustments(const FontPaintAdjustments& adjustments) {
        m_adjustments = adjustments;
    }

    void setSpaceAdjustment(OS::Millipoint spaceAddX) {
        m_adjustments.m_spaceAdd.dx = spaceAddX;
        m_adjustments.m_spaceAdd.dy = 0;
        m_adjustments.m_charAdd.dx = 0;
        m_adjustments.m_charAdd.dy = 0;
    }

    void setRuboutBox(const FontRect& ruboutBox) {
        m_ruboutBox = ruboutBox;
    }

    void setRuboutBox(int32_t left, int32_t bottom, int32_t right, int32_t top) {
        m_ruboutBox.set(left, bottom, right, top);
    }

    void setScanResults(OS::Millipoint maxHorizontalOffset,
                        OS::Millipoint maxVerticalOffset,
                        uint32_t spaceCount)
    {
        m_maxHorizontalOffset = maxHorizontalOffset;
        m_maxVerticalOffset = maxVerticalOffset;
        m_spaceCount = (int32_t)spaceCount;
    }

    void setInitialMatrix(const int32_t* rawMatrix) {
        if (rawMatrix == nullptr) {
            m_initialMatrix.setIdentity();
        } else {
            m_initialMatrix = Draw::Matrix::fromRaw6(rawMatrix);
        }
    }

    void setInitialState(FontHandle initialFont,
                         const uint8_t* stringAddress,
                         FontPaintFlag initialFlags,
                         const FontPoint& startPosition)
    {
        m_initialFont = initialFont;
        m_stringAddress = stringAddress;
        m_initialFlags = initialFlags;
        m_startPosition = startPosition;
    }

    void beginPass() {
        m_font = m_initialFont;
        m_matrix = m_initialMatrix;
        m_underlinePosition = Base::Fixed<8>::fromRaw(0);
        m_underlineThickness = Base::Fixed<8>::fromRaw(0);
        m_paintPosition = m_startPosition;
        m_ruboutStart = m_ruboutBox.x0;
    }

    void setUnderline(Base::Fixed<8> position, Base::Fixed<8> thickness) {
        // Unit is 1/256th of the font size when underline was set.
        // Changing font does not change underline settings.
        m_underlinePosition = position;
        m_underlineThickness = thickness;
    }

    void setCurrentFont(FontHandle font) {
        m_font = font;
    }

    void setCurrentMatrix(const Draw::Matrix& matrix) {
        m_matrix = matrix;
    }

    /* Font_Paint coordinates here are millipoints relative to the user's box. */
    FontHandle          m_initialFont;
    const uint8_t*      m_stringAddress;
    FontPaintFlag       m_initialFlags;
    FontPoint           m_startPosition;

    FontPoint           m_paintPosition;
    FontPoint           m_paintEndPosition;

    FontHandle          m_font;
    FontPaintFlag       m_flags;

    Base::Fixed<8>      m_underlinePosition;    // 1/256th of font size when specified.
    Base::Fixed<8>      m_underlineThickness;

    FontPaintAdjustments m_adjustments;
    FontRect            m_ruboutBox;            // Rubout box (if needed) in millipoints

    OS::Millipoint      m_ruboutStart;          // Start point of the current rubout section
    OS::Millipoint      m_ruboutEnd;            // End point of current rubout section

    int32_t             m_lastCharacter;        // Last character used
    OS::Millipoint      m_maxHorizontalOffset;  // Maximum horizontal offset
    OS::Millipoint      m_maxVerticalOffset;    // Maximum veritcal offset
    int32_t             m_spaceCount;           // Number of spaces within the line

    Draw::Matrix        m_initialMatrix;
    Draw::Matrix        m_matrix;
};

#endif
