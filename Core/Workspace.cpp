#include "swis.h"

#include "Job.h"
#include "Device.h"
#include "PDriver.h"
#include "Workspace.h"

#include "RLib/Constants/Service.h"

#include <string.h>

extern "C" void pdriver_entry(void);

inline MyError declareDriver(void* pw)
{
    return _swix(PDriver_DeclareDriver, _INR(0,2),
                 &pdriver_entry, pw, PrinterNumber);
}

// Not sure this is the right place for this.
static void notifyFontsLost(CoreWS& ws)
{
    JobManager& jobMgr(ws.jobMgr());

    for (CoreJobWS* job = jobMgr.firstJob();
         job != nullptr;
         job = jobMgr.nextJob(job))
    {
        font_fontslost(*job);
    }
}

CoreWS::~CoreWS()
{
    // Destructors can not return an error, but that works well here - doing
    // so would abort the finalisation process and keep the module alive
    // without any global Module object.
    (void)_swix(XPDriver_RemoveDriver, _IN(0), PrinterNumber);
    configure_finalise(*this);
    messages.close();
}

MyError
CoreWS::initialise()
{
    MyError err;

    // Device.h call.
    if ((err = configure_init(*this)) != nullptr)
        return err;

    if ((err = declareDriver(privateWord())) != nullptr) {
        configure_finalise(*this);
        return err;
    }

    // Intercept SpriteV so that we can maintain information about where output
    // is redirected. Note that this is completely independent of the interception
    // done during a print job.
    if ((err = m_spriteMonitor.start()) != nullptr) {
        (void)_swix(XPDriver_RemoveDriver, _IN(0), PrinterNumber);
        configure_finalise(*this);
        return err;
    }

    return nullptr;
}

MyError
CoreWS::preFinalise()
{
    if (m_jobManager.hasJobs())
        return messages.lookupError(ErrorBlock_PrintInUse).toKernelOSError();

    return nullptr;
}

void CoreWS::handleServiceCall(uint32_t service, OS::Regs& regs)
{
    switch (service) {
    case Service_Reset:
        m_interceptMgr.init();
        (void)m_spriteMonitor.start();
        notifyFontsLost(*this);
        (void)PDriver::reset();
        break;

    case Service_PDriverStarting:
        (void)declareDriver(privateWord());
        break;

    case Service_ModeChange:
        m_spriteMonitor.setToScreen();
        (void)m_interceptMgr.adjust();
        break;

    case Service_WimpReportError:
        m_interceptMgr.setWimpReportError(bool(regs[0]));

        if (currentJob() != nullptr) {
            if (regs[0] != 0)
                (void)managejob_suspend(*CoreWS::instance().currentJob());
            else
                (void)managejob_resume(*CoreWS::instance().currentJob());
        }

        (void)m_interceptMgr.adjust();
        break;

    case Service_PDriverGetMessages:
        messages.serviceMessages(regs);
        break;
    }
}




PDriverWS& CoreWS::toPDriverWS()
{
    return (PDriverWS&)*this;
}

const PDriverWS& CoreWS::toPDriverWS() const
{
    return (const PDriverWS&)*this;
}
