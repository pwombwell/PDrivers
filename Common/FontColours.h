#ifndef COMMON_FONTCOLOURS_H
#define COMMON_FONTCOLOURS_H

#include <stdint.h>

enum FontColourFlags
{
    FontColourFlag_BackgroundUsesGcol = 1u,
    FontColourFlag_ForegroundUsesGcol = 2u
};

class FontColourState
{
public:
    FontColourState()
        : m_flags(0),
          m_backgroundRgb(0),
          m_foregroundRgb(0),
          m_backgroundGcol(0),
          m_foregroundGcol(0),
          m_foregroundOffset(0)
    {
    }

    bool backgroundUsesGcol() const
    {
        return (m_flags & FontColourFlag_BackgroundUsesGcol) != 0;
    }

    bool foregroundUsesGcol() const
    {
        return (m_flags & FontColourFlag_ForegroundUsesGcol) != 0;
    }

    void setBackgroundGcol(uint32_t gcol)
    {
        m_flags |= FontColourFlag_BackgroundUsesGcol;
        m_backgroundGcol = gcol;
    }

    void setForegroundGcol(uint32_t gcol)
    {
        m_flags |= FontColourFlag_ForegroundUsesGcol;
        m_foregroundGcol = gcol;
    }

    void setBackgroundRgb(uint32_t bbGGRR00)
    {
        m_flags &= ~FontColourFlag_BackgroundUsesGcol;
        m_backgroundRgb = bbGGRR00;
    }

    void setForegroundRgb(uint32_t bbGGRR00)
    {
        m_flags &= ~FontColourFlag_ForegroundUsesGcol;
        m_foregroundRgb = bbGGRR00;
    }

    void setForegroundOffset(int32_t offset)
    {
        m_foregroundOffset = offset;
    }

    uint32_t backgroundRgb() const
    {
        return m_backgroundRgb;
    }

    uint32_t foregroundRgb() const
    {
        return m_foregroundRgb;
    }

    uint32_t backgroundGcol() const
    {
        return m_backgroundGcol;
    }

    uint32_t foregroundGcol() const
    {
        return m_foregroundGcol;
    }

    uint32_t foregroundGcolWithOffset() const
    {
        return m_foregroundGcol + (uint32_t)m_foregroundOffset;
    }

private:
    uint32_t m_flags;
    uint32_t m_backgroundRgb;
    uint32_t m_foregroundRgb;
    uint32_t m_backgroundGcol;
    uint32_t m_foregroundGcol;
    int32_t  m_foregroundOffset;
};

class FontColourContext
{
public:
    FontColourState& current()
    {
        return m_current;
    }

    const FontColourState& current() const
    {
        return m_current;
    }

    void save()
    {
        m_saved = m_current;
    }

    void restore()
    {
        m_current = m_saved;
    }

private:
    FontColourState m_current;
    FontColourState m_saved;
};

#endif
