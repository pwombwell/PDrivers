#ifndef COMMON_GRAPHICSCONTEXT_H
#define COMMON_GRAPHICSCONTEXT_H

#include "RLib/RLib.h"

#include <stdint.h>

struct GraphicsContext {
    uint32_t m_colour;

#if PSTextSpeedUps
    FontHandle m_font;
#endif
};

class GraphicsContextStack
{
public:
    GraphicsContextStack() {
        reset();
    }

    void reset()
    {
        m_current = &m_contexts[0];

        // Current colour and font are unknown.
        m_contexts[0].m_colour = 0xffffffffu;
#if PSTextSpeedUps
        m_contexts[0].m_font = 0xffffffffu;
#endif
    }

    uint32_t currentColour() const { return m_current->m_colour; }
#if PSTextSpeedUps
    FontHandle currentFont() const { return m_current->m_font; }
#endif

    GraphicsContext& current() { return *m_current; }
    const GraphicsContext& current() const { return *m_current; }

    bool push() {
#if DevelopmentChecks
        if (m_current >= &m_contexts[ContextCount - 1])
            return false;
#endif
        GraphicsContext* current = m_current;
        m_current++;
        *m_current = *current; // copy previous entry
        return true;
    }

    bool pop() {
#if DevelopmentChecks
        if (m_current <= &m_contexts[0])
            return false;
#endif
        m_current--;
        return true;
    }

    void invalidate(FontHandle font) {
#if PSTextSpeedUps
        for (GraphicsContext* ctx = &m_contexts[0]; ctx <= m_current; ctx++) {
            if (ctx->m_font == font)
                ctx->m_font = 0xffffffffu;
        }
#endif
    }

private:
    enum { ContextCount = 7 };

    GraphicsContext* m_current;
    GraphicsContext  m_contexts[ContextCount];
};

#endif
