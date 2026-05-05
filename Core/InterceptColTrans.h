#ifndef CORE_COLTRANS_H
#define CORE_COLTRANS_H

#include "InterceptManager.h"

#include "RLib/ColourTrans.h"
#include "RLib/OS/Vector.h"

class CoreWS;
class JobWS;

class InterceptColTrans : public InterceptorBase {
public:
    static VectorReturn intercept(OS::Regs& regs, CoreWS& ws);

    // Deal with calling the PaletteV and then read palette.
    // Errors not correctly handled in ARM code - this gracefully ignores them.
    static uint32_t readPalette(uint32_t pixval, uint32_t type);

private:
    InterceptColTrans(CoreWS& ws, CoreJobWS& job)
        : m_ws(ws), m_job(job)
    { }

    VectorReturn selectTable(OS::Regs& regs);
    VectorReturn setGCOL(OS::Regs& regs);
    VectorReturn retCN(OS::Regs& regs);
    VectorReturn retModeCN(OS::Regs& regs);
    VectorReturn setOppGCOL(OS::Regs& regs);
    VectorReturn retOppCN(OS::Regs& regs);
    VectorReturn retModeOppCN(OS::Regs& regs);
    VectorReturn setFontCols(OS::Regs& regs);
    VectorReturn generateTable(OS::Regs& regs);

    VectorReturn tableGenerate(OS::Regs& regs);
    VectorReturn pass(OS::Regs& regs) { return pass(); }
    VectorReturn pass();

private:
    CoreWS&     m_ws;
    CoreJobWS&  m_job;

    typedef VectorReturn (InterceptColTrans::*Handler)(OS::Regs&);

    static const Handler s_table[];
};

#endif
