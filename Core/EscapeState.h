#ifndef ESCAPE_STATE_H
#define ESCAPE_STATE_H

#include <stdint.h>

#include "RLib/RLib.h"

class InterceptManager;
class MsgCode;

// The old ESCAPE state (for transfer between 'enableescapes' and
// 'disableandcheckescapes').
class EscapeState {
public:
    EscapeState(InterceptManager& interceptMgr, MsgCode& msgCode)
        : m_interceptMgr(interceptMgr)
        , m_msgCode(msgCode)
        , m_depth(0)
        , m_oldState(0)
        , m_oldEffect(0)
    { }

    // Subroutine to set up to pass OS_WriteC's through and then enable ESCAPEs.
    //   These effects only happen if there have been equal numbers of calls to
    // 'enableescapes' and 'disableandcheckescapes' before this call. But note
    // that the nesting counter is only a byte, so don't nest things badly!
    MyError enable();

    // Subroutine to set up to intercept OS_WriteC's, then disable ESCAPEs, then
    // if an ESCAPE condition has occurred, acknowledge it and generate an ESCAPE
    // error. If no ESCAPE condition has occurred, any error state present when
    // the routine was called is preserved.
    //   These effects only happen if there has been one more call to
    // 'enableescapes' than to 'disableandcheckescapes' before this call. But
    // note that the nesting counter is only a byte, so don't nest things badly!
    MyError disableAndCheck();

private:
    InterceptManager&   m_interceptMgr;
    MsgCode&        m_msgCode;

    uint8_t         m_depth; // Used to avoid multiple changes.
    uint8_t         m_oldState;
    uint8_t         m_oldEffect;

    struct EnvironmentHandler {
        EnvironmentHandler() : address(nullptr), r12(nullptr), buffer(nullptr) { }

        uint32_t* address;
        uint32_t* r12;
        uint32_t* buffer;
    };
    EnvironmentHandler  m_previous;
};

// Code should explicitly re-enable Escapes (and return an error if pressed),
// but this cleans up under error conditions without reporting the escape -
// the existing error will take precedence.
// (it'll clean up in good cases too if you forget to re-enable.)
class ScopedEscapeEnable
{
public:
    /* explicit */ScopedEscapeEnable(EscapeState& esc)
        : m_escapeState(esc), m_active(false)
    {}

    MyError enable() {
        MyError err = m_escapeState.enable();
        if (err == nullptr)
            m_active = true;
        return err;
    }

    MyError disableAndCheck() {
        if (!m_active)
            return nullptr;

        m_active = false;
        return m_escapeState.disableAndCheck();
    }

    // Silently disable (no checking as no way to return an error)
    ~ScopedEscapeEnable() {
        if (m_active)
            (void)m_escapeState.disableAndCheck();
    }

private:
    EscapeState&    m_escapeState;
    bool            m_active;
};

#endif
