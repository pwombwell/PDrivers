#ifndef PS_FONT_CHUNK_PAINTER_H
#define PS_FONT_CHUNK_PAINTER_H

#include "Common/FontChunkPainter.h"

#include "RLib/OS.h"
#include "RLib/Geometry/Point.h"
#include "RLibX/OS.h"
#include "RLibX/Font.h"

class JobWS;

class PSFontChunkPainter : public FontChunkRenderer
{
public:
    PSFontChunkPainter(JobWS& job);

    // `font_passstart`
    MyError startPass(uint32_t pass) { return m_painter.startPass(pass); }

    // `font_passend`
    MyError endPass(uint32_t pass) { return m_painter.endPass(pass); }

    // `font_paintchunk`
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
    MyError outputMatrix();

    MyError justifying(const uint8_t* chunk, size_t length,
                       const Font::Point& position);

    MyError notJustifying(const uint8_t* chunk, size_t length,
                          const Font::Point& position);

private:
    JobWS&                  m_job;
    FontChunkPainter        m_painter;

#if !PSTextSpeedUps
    FontHandle              m_currentPSFont;    // `currentPSfont`
#endif
};

#endif
