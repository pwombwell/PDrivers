#ifndef PDF_FONT_CHUNK_PAINTER_H
#define PDF_FONT_CHUNK_PAINTER_H

#include "Common/FontChunkPainter.h"

class JobWS;

namespace PDF {
class FontRecord;
}

#ifdef PSTextSpeedUps
#if PSTextSpeedUps
#define PDFTextSpeedUps 1
#endif
#endif

#ifndef PDFTextSpeedUps
#define PDFTextSpeedUps 0
#endif

class PDFFontChunkPainter : public FontChunkRenderer
{
public:
    PDFFontChunkPainter(JobWS& job);

    MyError startPass(uint32_t pass) { return m_painter.startPass(pass); }
    MyError endPass(uint32_t pass) { return m_painter.endPass(pass); }
    MyError paint(const uint8_t* chunk,
                  size_t length,
                  const Geometry::Point<OS::Millipoint>& position,
                  uint32_t pass)
    {
        return m_painter.paint(chunk, length, position, pass);
    }

    FontColourContext& colours() { return m_painter.colours(); }
    FontColourState& currentColourState() { return m_painter.currentColourState(); }

public:
    virtual void resetCurrentFont();
    virtual bool changeFont(FontHandle font);
    virtual MyError selectFont(FontHandle font, const Font::ReadDefn& defn);
    virtual MyError fillRect(const Font::Rect& rect, uint32_t bbGGRR00);
    virtual MyError paintText(const FontChunkPaint& paint, uint32_t bbGGRR00);

private:
    JobWS&              m_job;
    FontChunkPainter    m_painter;
    PDF::FontRecord*    m_fontRecord;
    int32_t             m_fontSize16;

#if PDFTextSpeedUps
#else
    FontHandle          m_currentPDFFont;
#endif
};

#endif
