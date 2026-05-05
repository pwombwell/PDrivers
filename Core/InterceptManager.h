#ifndef INTERCEPT_H
#define INTERCEPT_H

#include "RLib/RLib.h"
#include "RLibX/Utils/EnumBitmaskOps.h"

// Values for the 'intercepting' and 'shouldintercept' flags.
enum Intercept /* uint8_t */ { // FIXME: Rename InterceptFlag(s)?
    Intercept_None      = 0,
    Intercept_Wrch      = 1u<<0,
    Intercept_ColTrans  = 1u<<1,
    Intercept_Draw      = 1u<<2,
    Intercept_Sprite    = 1u<<3,
    Intercept_Byte      = 1u<<4,
    Intercept_Font      = 1u<<5,
    Intercept_JPEG      = 1u<<6,
    Intercept_All       = (1u<<7)-1
};
DEFINE_ENUM_BITWISE_OPERATORS(Intercept);

/* Values for the 'passthrough' flags. */
enum Passthrough /* : uint8_t */ {
    Passthrough_None        = 0,
    Passthrough_Wrch        = Intercept_Wrch,
    Passthrough_ColTrans    = Intercept_ColTrans,
    Passthrough_Draw        = Intercept_Draw,
    Passthrough_Sprite      = Intercept_Sprite,
    Passthrough_Byte        = Intercept_Byte
};
DEFINE_ENUM_BITWISE_OPERATORS(Passthrough);

class CoreWS;

class InterceptorBase {
public:
};

class InterceptManager {
public:
    InterceptManager(CoreWS& ws) : m_ws(ws) { init(); }

    // Routine to initialise the interception data to its standard state just
    // after the printer driver is initialised or after a soft reset. Assumptions
    // made:
    //   (a) Output is directed to the screen at the time.
    //   (b) We are not in a Wimp error report.
    //   (c) No interceptions are currently in effect.
    // All three of these assumptions are necessarily true on a soft reset, and
    // (c) is necessarily true on initialisation. It is a mistake to initialise
    // the PDriver module when (a) or (b) is not true.
    //   NB does not return errors. V insignificant on exit.
    void init();

    // Routine to adjust the interceptions being made at a time that the
    // 'shouldintercept' byte has not changed, but other circumstances (e.g.
    // existence of an active print job, output to sprite status, Wimp error
    // reporting) have.
    //   If an error occurs during OS_Claim or OS_Release, an attempt will be made
    // to clear all interceptions. The error will be reported only if V was clear
    // on entry (i.e. there was no error). Under all other circumstances, the
    // routine preserves all registers and flags.
    MyError adjust();

    // Routine to change the current interceptions to fit the circumstances. The
    // rules are:
    //   (a) If a Wimp error is currently being reported, turn off all
    //       interceptions.
    //   (b) If there is no active print job, turn off all interceptions.
    //   (c) If there is an active print job and output is neither directed to
    //       the screen nor to that job's sprite, turn off all interceptions.
    //   (d) Otherwise set up the interceptions indicated by 'shouldintercept'.
    //
    // R3 contains the interceptions wanted on entry.
    //   If an error occurs during OS_Claim or OS_Release, an attempt will be made
    // to clear all interceptions. The error will be reported only if V was clear
    // on entry (i.e. there was no error). Under all other circumstances, the
    // routine preserves all registers and flags.
    MyError change(Intercept wanted);

    Passthrough getPassthrough() const { return m_passthrough; }
    void setPassthrough(Passthrough v) { m_passthrough = v; }
    void addPassthrough(Passthrough v) { m_passthrough |= v; }
    bool hasPassthrough(Passthrough v) const { return !!(m_passthrough & v); }

    void setWimpReportError(bool open) { m_wimpReportFlag = open; }
    bool hasWimpReportError() const { return m_wimpReportFlag; }

private:
    MyError change(Intercept wanted, bool recursed);

private:
    CoreWS&     m_ws;

    // Which vectors (and font calls) we are currently intercepting.
    Intercept   m_intercepting;

    // Which vectors (and font calls) we want to be intercepting at present.
    Intercept   m_shouldIntercept;

    // Whether we are passing various types of call through to their normal handlers.
    Passthrough m_passthrough;

    // Whether this print job is suspended because of the Wimp reporting an error.
    bool        m_wimpReportFlag;
};

class ScopedPassthrough {
public:
    ScopedPassthrough(InterceptManager& manager, Passthrough type) : m_manager(manager)
{
        m_start = manager.getPassthrough();
        manager.addPassthrough(type);
    }
    ~ScopedPassthrough() { m_manager.setPassthrough(m_start); }

private:
    InterceptManager&   m_manager;
    Passthrough         m_start;
};

#endif
