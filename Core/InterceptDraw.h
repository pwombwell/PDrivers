#ifndef CORE_DRAW_H
#define CORE_DRAW_H

#include "InterceptManager.h"

#include "RLib/OS/Vector.h"

class CoreJobWS;
class CoreWS;

class InterceptDraw : public InterceptorBase {
public:
    static VectorReturn intercept(OS::Regs& regs, CoreWS& ws);

private:
    InterceptDraw(CoreWS& ws, CoreJobWS& job)
        : m_ws(ws), m_job(job)
    { }

    VectorReturn fill(OS::Regs& regs);
    VectorReturn stroke(OS::Regs& regs);
    VectorReturn processPath(OS::Regs& regs);
    VectorReturn pass();
    VectorReturn drawReturn(MyError err);

private:
    CoreWS&    m_ws;
    CoreJobWS& m_job;
};

#endif
