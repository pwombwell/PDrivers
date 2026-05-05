#include "kernel.h"
#include <stdio.h>

#include "PDriver.h"
#include "JobManager.h"

#include "Device.h"
#include "FontBlock.h"
#include "InterceptManager.h"
#include "Job.h"
#include "OS.h"
#include "Workspace.h"

#include "RLibX/Utils/String.h"

#include <string.h>

JobManager::JobManager(CoreWS& ws)
    : m_ws(ws)
    , m_current(nullptr)
{
}

CoreJobWS* JobManager::findJob(FileHandle handle)
{
    CoreJobWS* job = m_jobs.first();

    while (job) {
        if (job->jobhandle == handle)
            return job;
    }

    return nullptr;
}

MyError JobManager::deleteJob(CoreJobWS* job)
{
    if (job == nullptr)
        return nullptr;

    debugLog("JobManager::deleteJob %u", job->jobhandle);

    // Lose slaved fonts
    MyError err = font_losefont(0, *job);
    if (err != nullptr)
        return err;

    // Remove rectangles list
    job->clearRectangles();

    // And, finally, remove the fonts attached to the job
    job->jobfontlist.removeFonts();

    if (m_current == job) {
        debugLog("m_current was job. filehandle:%u", job->jobhandle);
        m_current = nullptr;
    }

    // Must call delete, which will release the job from the list
    // in the DListHook destructor.
    managejob_destroy(job);

    return nullptr;
}

MyError JobManager::selectJob(FileHandle newHandle,
                              const char* title,
                              bool illustration,
                              FileHandle& previous)
{
    MyError err;

    if (m_current && m_current->jobhandle == newHandle)
        return nullptr;

    ScopedEscapeEnable esc(CoreWS::instance().m_escapeState);
    CoreJobWS* prevJob = m_current;
    CoreJobWS* newJob;

    previous = prevJob ? prevJob->jobhandle : 0;

    if (prevJob != nullptr) {
        err = managejob_suspend(*prevJob);
#if DevelopmentChecks
        if (err != nullptr) {
            (void)ws.interceptMgr().adjust();
            return err;
        }
#endif
    }

    if (newHandle == 0) {
        m_current = nullptr;
        return m_ws.interceptMgr().adjust();
    }


    CoreJobWS* existingJob = findJob(newHandle);
    if (existingJob != nullptr) {
        m_current = existingJob;
        (void)managejob_resume(*existingJob);
        return m_ws.interceptMgr().adjust();
    }

    newJob = managejob_allocate(newHandle, illustration, m_ws);
    if (newJob == nullptr) {
        err = MyError::OOM();
        goto selectJobPreviousJob;
    }

    (void)newJob->info.setName(m_ws.globalInfo.printerName());

    m_jobs.push_back(*newJob);
    m_current = newJob;

    err = newJob->screenVars.readForCurrentMode();
    if (err == nullptr)
        err = newJob->vdu5().init(*newJob);
    if (err != nullptr)
        goto selectJobDeallocateAndPreviousJob;

    {
        FontHandle font;
        uint32_t currentFg = 0;
        uint32_t currentBg = 0;
        uint32_t offset = 0;

        err = XFont_CurrentFont(font, currentFg, currentBg, offset);
        if (err == nullptr)
            err = font_fg(currentFg, *newJob);
        if (err == nullptr)
            err = font_bg(currentBg, *newJob);
        if (err == nullptr) {
            newJob->fontcoloffset = (int32_t)offset;
            err = font_coloffset((int32_t)offset, *newJob);
        }
    }
    if (err != nullptr)
        goto selectJobDeallocateAndPreviousJob;

    if ((err = esc.enable()) != nullptr)
        goto selectJobDeallocateAndPreviousJob;

    if (err == nullptr)
        err = managejob_init(title, *newJob);
    if (err == nullptr)
        err = newJob->jobfontlist.duplicate(m_ws.fontlist);
    if (err == nullptr)
        err = esc.disableAndCheck();

    if (err != nullptr)
        goto selectJobDeallocateAndPreviousJob;

    return m_ws.interceptMgr().adjust();

selectJobDeallocateAndPreviousJob:
    if (newJob != nullptr)
        (void)deleteJob(newJob);

selectJobPreviousJob:
    m_current = prevJob;
    if (prevJob != nullptr)
        (void)managejob_resume(*prevJob);

    {
        MyError adjustErr = m_ws.interceptMgr().adjust();
        if (err == nullptr && adjustErr != nullptr)
            err = adjustErr;
    }

    return err;
}

MyError JobManager::endJob(FileHandle fileHandle)
{
    MyError err;

    CoreJobWS* jobPtr;

    jobPtr = findJob(fileHandle);
    if (jobPtr == nullptr) {
        (void)m_ws.interceptMgr().adjust();

        return m_ws.messages.lookupError(ErrorBlock_PrintNoSuchJob);
    }

    CoreJobWS& job(*jobPtr);

    bool wasCurrentJob = m_current == &job;
    if (job.hasPersistentError())
        return job.getPersistentError();

    if (job.illustrationjob && job.numberofpages != 1)
        return job.makePersistentError(OS::ErrorView(ErrorBlock_PrintNotOnePage));

    if (job.copiestodo != 0)
        return job.makePersistentError(OS::ErrorView(ErrorBlock_PrintPrintingPage));

    ScopedEscapeEnable esc(m_ws.m_escapeState);
    if ((err = esc.enable()) != nullptr)
        return job.makePersistentError(err);

    // 'esc' will go out of scope, but not report its error, matching asm.
    if ((err = managejob_finalise(job)) != nullptr)
        return job.makePersistentError(err);

    if ((err = esc.disableAndCheck()) != nullptr)
        return job.makePersistentError(err);

    if ((err = deleteJob(&job)) != nullptr)
        return job.makePersistentError(err);
    // Oh, for lambdas... So many makePersistentErrors.
    
    if (wasCurrentJob)
        return m_ws.interceptMgr().adjust();

    return nullptr;
}

MyError JobManager::abortJob(FileHandle fileHandle)
{
    MyError err;

    CoreJobWS* job;

    job = findJob(fileHandle);
    if (job == nullptr)
        return m_ws.messages.lookupError(ErrorBlock_PrintNoSuchJob);

    // Call PDriver via Device.h.
    MyError abortErr = managejob_abort(*job);

    MyError deleteErr = deleteJob(job);

    debugLog("deleted job.. m_current:%p", m_current);

    err = abortErr ? abortErr : deleteErr;

    if (!err)
        m_ws.m_countingPass = false;

    MyError adjustErr = m_ws.interceptMgr().adjust();
    debugLog("abortJob returning with err:%p adjustErr:%p", err.message(), adjustErr.message());

    if (!err && adjustErr)
        return adjustErr;

    return err;
}

MyError JobManager::reset()
{
    MyError err = nullptr;

    while (m_jobs.empty() && !err) {
        CoreJobWS* job = m_jobs.first();

        MyError abortErr = managejob_abort(*job);
        MyError deleteErr = deleteJob(job);

        err = abortErr ? abortErr : deleteErr;
    }

    m_current = nullptr;

    MyError adjustErr = m_ws.interceptMgr().adjust();
    if (!err && adjustErr)
        err = adjustErr;

    if (!err)
        m_ws.fontlist.removeFonts();

    return err;
}
