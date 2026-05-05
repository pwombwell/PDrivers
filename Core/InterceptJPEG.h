#ifndef CORE_JPEGSWI_H
#define CORE_JPEGSWI_H

#include "InterceptManager.h"

class CoreWS;
class CoreJobWS;

class InterceptJPEG : public InterceptorBase {
public:
    InterceptJPEG(CoreWS& ws, CoreJobWS& job)
        : m_ws(ws), m_job(job)
    { }

    MyError intercept(const OS::Regs& regs);

private:
    MyError plotScaled(const uint8_t* jpeg, uint32_t len,
                       uint32_t flags,
                       OS::Unit x, OS::Unit y,
                       const Sprite::ScaleFactor* scale);

    MyError plotTransformed(const uint8_t* jpeg, uint32_t len,
                            uint32_t flags, const int32_t* matrixOrCoords);

    MyError unsupported();

    MyError persistentReturn(MyError err) { return errorReturn(err); }
    MyError errorReturn(MyError err);

private:
    CoreWS&     m_ws;
    CoreJobWS&  m_job;
};

#endif
