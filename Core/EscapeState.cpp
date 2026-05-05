#include "swis.h"
#include "cmhg.h"

#include "EscapeState.h"
#include "InterceptManager.h"
#include "MsgCode.h"
#include "OS.h"
#include "Workspace.h"

#include "RLibX/OSByte.h"

static const uint32_t OSChangeEnvironment_EscapeHandler = 9u;

_kernel_oserror *escape_handler_handler(_kernel_swi_regs *r, void *pw)
{
    // Do nothing.
    return nullptr;
}

// `enableescapes`
// Subroutine to set up to pass OS_WriteC's through and then enable ESCAPEs.
//   These effects only happen if there have been equal numbers of calls to
// 'enableescapes' and 'disableandcheckescapes' before this call. But note
// that the nesting counter is only a byte, so don't nest things badly!
MyError EscapeState::enable()
{
    MyError err;

    if (++m_depth != 1)
        return nullptr;

    InterceptManager& mgr(CoreWS::instance().interceptMgr());
    mgr.setPassthrough(Passthrough_Wrch);

    if ((err = OSByte::osbyte1(OSByte::Var_EscapeState, 0, 0, m_oldState)) != nullptr)
        return err;

    if ((err = OSByte::osbyte1(OSByte::Var_EscapeEffects, 0, 0, m_oldEffect)) != nullptr)
        return err;

    // Replace the current escape handler with one that does nothing (since
	// we are arranging things so we return escapes as a synchronous error)
    err = _swix(OS_ChangeEnvironment, _INR(0,3)|_OUTR(1,3),
                OSChangeEnvironment_EscapeHandler,
                (uintptr_t)&escape_handler_handler, 0, 0,
                &m_previous.address, &m_previous.r12, &m_previous.buffer);
        return err;

    return nullptr;
}

// `disableandcheckescapes`
// Subroutine to set up to intercept OS_WriteC's, then disable ESCAPEs, then
// if an ESCAPE condition has occurred, acknowledge it and generate an ESCAPE
// error. If no ESCAPE condition has occurred, any error state present when
// the routine was called is preserved.
//   These effects only happen if there has been one more call to
// 'enableescapes' than to 'disableandcheckescapes' before this call. But
// note that the nesting counter is only a byte, so don't nest things badly!
MyError EscapeState::disableAndCheck()
{
    MyError err;

    m_depth--;
    if (m_depth != 0)
        return nullptr;

    InterceptManager& mgr(CoreWS::instance().interceptMgr());
    mgr.setPassthrough(Passthrough_None);

    if ((err = OSByte::write(OSByte::Var_EscapeState, m_oldState)) != nullptr)
        return err;

    if ((err = OSByte::write(OSByte::Var_EscapeEffects, m_oldEffect)) != nullptr)
        return err;

    uint8_t escPending;
    if ((err = OSByte::osbyte1(OSByte::Op_AcknowledgeEscape, 0, 0, escPending)) != nullptr)
        return err;

    // Restore old escape handler
    if ((err = _swix(OS_ChangeEnvironment, _INR(0,3),
                     OSChangeEnvironment_EscapeHandler,
                     m_previous.address, m_previous.r12, m_previous.buffer)) != nullptr)
        return err;

    if (!escPending)
        return nullptr;

    return m_msgCode.lookupError(ErrorBlock_Escape);
}
