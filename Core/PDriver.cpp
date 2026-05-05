#include "swis.h"
#include "PDriver.h"

#include "Device.h"
#include "InterceptWrch.h"
#include "Job.h"
#include "Workspace.h"

#include "RLibX/Font/ReadDefn.h"

#include <stdlib.h>
#include <string.h>

const PDriverInfo& PDriver::info(uint32_t& version)
{
    CoreWS& ws = CoreWS::instance();
    CoreJobWS* job = ws.currentJob();

    // If no print job active, use global values, else local
    PDriverInfo* info = job ? &job->info : &ws.globalInfo;

    // The original ARM code effectively initialises the name on whichever
    // info block is being queried, but it only records the length globally.
    // Keep the non-leaking behaviour of setNameToNone(), while updating the
    // queried block consistently.
    if (info->printerName() == nullptr)
        info->setNameToNone(ws.messages);

    version = (PrinterNumber << 16) + VersionNumber;

    return *info;
}

MyError PDriver::setInfo(SetPDriverInfo& info)
{
    CoreWS& ws = CoreWS::instance();
    MyError err;

    if ((err = configure_vetinfo(info)) != nullptr)
        return err;

    // Only allow colour/monochrome bit modification
    PDriverInfo::Features existing = ws.globalInfo.features();
    PDriverInfo::Features requested = info.features;

    info.features = (existing & ~PDriverInfo::Colour_Colour) |
                    (requested & PDriverInfo::Colour_Colour);

    ws.globalInfo = info;

    return nullptr;
}

MyError PDriver::selectJob(FileHandle fileHandle, const char* title,
                           FileHandle& previousHandle)
{
    CoreWS& ws = CoreWS::instance();

    return ws.jobMgr().selectJob(fileHandle, title, false, previousHandle);
}

FileHandle PDriver::currentJob()
{
    JobManager& jobMgr = CoreWS::instance().jobMgr();

    CoreJobWS* job = jobMgr.current();
    if (!job)
        return 0;

    return job->jobhandle;
}

MyError PDriver::endJob(FileHandle fileHandle)
{
    JobManager& jobMgr = CoreWS::instance().jobMgr();

    return jobMgr.endJob(fileHandle);
}

MyError PDriver::abortJob(FileHandle fileHandle)
{
    JobManager& jobMgr = CoreWS::instance().jobMgr();

    return jobMgr.abortJob(fileHandle);
}

MyError PDriver::reset()
{
    JobManager& jobMgr = CoreWS::instance().jobMgr();

    return jobMgr.reset();
}

MyError PDriver::checkFeatures(OS::Regs& regs)
{
    CoreWS& ws = CoreWS::instance();
    const CoreJobWS* job = ws.currentJob();

    PDriverInfo::Features mask = (PDriverInfo::Features)regs[0];
    PDriverInfo::Features value = (PDriverInfo::Features)regs[1];

    const PDriverInfo* info = job ? &job->info : &ws.globalInfo;

    if ((info->features() & mask) != (value & mask))
        return configure_makeerror(mask, value, info->features(), ws);

    return nullptr;
}

const PDriverPageSize& PDriver::pageSize()
{
    CoreWS& ws = CoreWS::instance();
    CoreJobWS* job = ws.currentJob();

    if (job)
        return job->pageSize;

    return ws.pageSize;
}

void PDriver::setPageSize(const PDriverPageSize& pageSize)
{
    CoreWS& ws = CoreWS::instance();

    ws.pageSize = pageSize;
}


MyError PDriver::giveRectangle(uint32_t id,
                               const Rect<OS::Unit>& box,
                               const Draw::DimensionlessTransform& transform,
                               const Point<OS::Millipoint>& bottomLeft,
                               uint32_t bgColour)
{
    MyError err;
    CoreWS& ws = CoreWS::instance();
    CoreJobWS* job = ws.currentJob();

    if (job == nullptr)
        return ws.messages.lookupError(ErrorBlock_PrintNoJobSelected);

    if (job->hasPersistentError())
        return job->checkPersistentError();

    if (job->copiestodo != 0) {
        err = ws.messages.lookupError(ErrorBlock_PrintPrintingPage);
        return job->makePersistentError(err);
    }

#if CaptureTrace
    (void)capturetrace_log(*job,
                           "trace_giverectangle %u %ld %ld %ld %ld "
                           "%ld %ld %ld %ld %ld %ld %u",
                           (unsigned)id,
                           (long)box.x0.raw(),
                           (long)box.y0.raw(),
                           (long)box.x1.raw(),
                           (long)box.y1.raw(),
                           (long)transform.a.raw(),
                           (long)transform.b.raw(),
                           (long)transform.c.raw(),
                           (long)transform.d.raw(),
                           (long)bottomLeft.x.raw(),
                           (long)bottomLeft.y.raw(),
                           (unsigned)bgColour);
#endif

    if (!job->addNewUserRect(id, box, transform, bottomLeft, bgColour))
        return job->makePersistentError(MyError::OOM());

    return nullptr;
}

static MyError getrectangle_shared(Rect<OS::Unit>& rect,
                                   uint32_t& copiesRemaining,
                                   uint32_t& rectId,
                                   ScopedEscapeEnable& esc,
                                   CoreJobWS& job)
{
    MyError err;

    debugLog("[getrectangle_shared] copiedRemaining:%d countingPass:%d", copiesRemaining, CoreWS::instance().isCountingPass());

    if ((err = pagebox_nextbox(copiesRemaining, rectId, rect, job)) != nullptr)
        return job.makePersistentError(err);

#if CaptureTrace
    (void)capturetrace_log(job,
                           "trace_pagebox_nextbox_core %u %u %ld %ld %ld %ld "
                           "%ld %ld %ld %ld %ld %ld",
                           (unsigned)copiesRemaining,
                           (unsigned)rectId,
                           (long)rect.x0.raw(),
                           (long)rect.y0.raw(),
                           (long)rect.x1.raw(),
                           (long)rect.y1.raw(),
                           (long)job.usersoffset.dx.raw(),
                           (long)job.usersoffset.dy.raw(),
                           (long)job.usersbox.width.raw(),
                           (long)job.usersbox.height.raw(),
                           (long)job.usersbottomleft.x.raw(),
                           (long)job.usersbottomleft.y.raw());
#endif

    if ((err = esc.disableAndCheck()) != nullptr)
        return job.makePersistentError(err);

    if (copiesRemaining == 0) {
        job.clearRectangles();

        job.disabled = Disabled_NoPage;
        job.copiestodo = 0;
        copiesRemaining = 0;

        return nullptr;
    }

    if (CoreWS::instance().isCountingPass()) {
        job.disabled = Disabled_None;
        job.copiestodo = copiesRemaining;
        copiesRemaining |= (1u<<24);

        return nullptr;
    }

    Offset<OS::Unit> offset = job.usersoffset;
    rect.offsetBy(offset);

#if CaptureTrace
    (void)capturetrace_log(job,
                           "trace_getrectangle_return %u %u %ld %ld %ld %ld",
                           (unsigned)copiesRemaining,
                           (unsigned)rectId,
                           (long)rect.x0.raw(),
                           (long)rect.y0.raw(),
                           (long)rect.x1.raw(),
                           (long)rect.y1.raw());
#endif

    if ((err = wrch_defaultbox(job)) != nullptr)
        return job.makePersistentError(err);

    job.setDefaultDotPattern();
    job.currentsprite = Sprite::Selector(Sprite::AreaValue(-1), nullptr, Sprite::Id());
    if ((err = pagebox_cleartobg(job)) != nullptr)
        return job.makePersistentError(err);

    job.screenVars.bgmode = OS::GCOLAction(0);
    uint32_t gcol = (uint32_t)job.screenVars.bggcol;
    gcol = (gcol & 0x00FFFFFFu) | 0x80000000u;
    job.screenVars.bggcol = gcol;
    job.screenVars.bgrgb = job.usersbg;

    if ((err = job.vdu5().init(job)) != nullptr)
        return job.makePersistentError(err);

    job.disabled = Disabled_None;
    job.copiestodo = copiesRemaining;

    debugLog("get_rectangle returns [%d,%d][%d,%d]", rect.x0, rect.y0, rect.x1, rect.y1);

    return nullptr;
}

MyError PDriver::drawPage(uint32_t& copiesLeft,
                          Rect<OS::Unit>& rect,
                          uint32_t sequence,
                          const char* pageName,
                          uint32_t& rectId)
{
    MyError err;
    CoreWS& ws = CoreWS::instance();

    if (ws.currentJob() == nullptr)
        return ws.messages.lookupError(ErrorBlock_PrintNoJobSelected);

        CoreJobWS& job = *ws.currentJob();
    if (job.hasPersistentError())
        return job.getPersistentError();

    if (job.copiestodo != 0)
        return job.makePersistentError(OS::ErrorView(ErrorBlock_PrintPrintingPage));

    if ((copiesLeft & 0xfe000000u) != 0)
        return job.makePersistentError(OS::ErrorView(ErrorBlock_PrintInvalidCopies));

    uint32_t flags = copiesLeft & 0xFF000000u;
#if CaptureTrace
    (void)capturetrace_log(job,
                           "trace_drawpage_entry %u %u %u '%s'",
                           (unsigned)copiesLeft,
                           (unsigned)flags,
                           (unsigned)sequence,
                           pageName != nullptr ? pageName : "<null>");
#endif
    copiesLeft &= 0x00FFFFFFu;
    ws.m_countingPass = (flags & 0x01000000u) ? true : false;

    uint32_t pages = job.numberofpages + 1;
    job.numberofpages = pages;
    if (pages != 1 && job.illustrationjob)
        return job.makePersistentError(OS::ErrorView(ErrorBlock_PrintNotOnePage));

    ScopedEscapeEnable esc(ws.m_escapeState);
    if ((err = esc.enable()) != nullptr)
        return job.makePersistentError(err);

        debugLog("[drawpage] 7");
    if ((err = pagebox_setup(copiesLeft, sequence, pageName, job)) != nullptr)
        return job.makePersistentError(err);

#if CaptureTrace
    (void)capturetrace_log(job,
                           "trace_drawpage_setup_done %u %u %u",
                           (unsigned)copiesLeft,
                           (unsigned)sequence,
                           (unsigned)ws.isCountingPass());
#endif

    debugLog("[drawpage] 8");

    return getrectangle_shared(rect, copiesLeft, rectId, esc, job);
}

// `getrectangle`
MyError PDriver::getRectangle(Rect<OS::Unit>& rect,
                              uint32_t& copiesRemaining,
                              uint32_t& rectId)
{
    MyError err;
    CoreWS& ws = CoreWS::instance();

    if (ws.currentJob() == nullptr)
        return ws.messages.lookupError(ErrorBlock_PrintNoJobSelected);

    CoreJobWS& job(*ws.currentJob());
    if (job.hasPersistentError())
        return job.getPersistentError();

    if (job.copiestodo == 0)
        return job.makePersistentError(OS::ErrorView(ErrorBlock_PrintNoCurrentPage));

debugLog("getRectangle 1");
    if ((err = vdu5_flush(job)) != nullptr)
        return job.makePersistentError(err);
debugLog("getRectangle 2");

    ScopedEscapeEnable esc(ws.m_escapeState);
    if ((err = esc.enable()) != nullptr)
        return job.makePersistentError(err);
debugLog("getRectangle 3");

    copiesRemaining = job.copiestodo;

    return getrectangle_shared(rect, copiesRemaining, rectId, esc, job);
}

MyError PDriver::cancelJob(OS::Regs& regs)
{
    CoreWS& ws = CoreWS::instance();

    uint32_t handle = regs[0];
    CoreJobWS* job;

    job = ws.jobMgr().findJob((FileHandle)handle);
    if (job == nullptr)
        return ws.messages.lookupError(ErrorBlock_PrintNoSuchJob);

    MyError err = ws.messages.lookupError(ErrorBlock_PrintCancelled);
    (void)job->makePersistentNoSuffix(err);

    return nullptr;
}

MyError PDriver::screenDump(OS::Regs& regs)
{
    return screendump_dump((int32_t)regs[0]).toKernelOSError();
}

MyError PDriver::enumerateJobs(FileHandle& prevJobHandle, CoreJobWS*& nextJob)
{
    CoreWS& ws = CoreWS::instance();
    JobManager& jobMgr(ws.jobMgr());

    if (prevJobHandle == 0) {
        nextJob = jobMgr.firstJob();
        return nullptr;
    }

    CoreJobWS* prevJob;

    prevJob = jobMgr.findJob(prevJobHandle);
    if (prevJob == nullptr)
        return ws.messages.lookupError(ErrorBlock_PrintNoSuchJob);

    nextJob = jobMgr.nextJob(prevJob);

    return nullptr;
}

MyError PDriver::cancelJobWithError(FileHandle toCancel, MyError withError)
{
    CoreWS& ws = CoreWS::instance();

    CoreJobWS* job = ws.jobMgr().findJob((FileHandle)toCancel);
    if (job == nullptr)
        return ws.messages.lookupError(ErrorBlock_PrintNoSuchJob);

    (void)job->makePersistentNoSuffix(withError);

    // This intentionally does not return the error, matching Acorn's behaviour.
    return nullptr;
}

MyError PDriver::selectIllustration(FileHandle handle,
                                    const char* title,
                                    FileHandle& previous)
{
    CoreWS& ws = CoreWS::instance();

    MyError err = ws.jobMgr().selectJob(handle, title, true, previous);
    if (err != nullptr)
        return err.toKernelOSError();

    return nullptr;
}

MyError PDriver::insertIllustration(FileHandle insertFrom,
                                    const Draw::Path* clipPath,
                                    const Point<OS::Unit>& bottomLeft,
                                    const Point<OS::Unit>& bottomRight,
                                    const Point<OS::Unit>& topLeft)
{
    return picture_insert(insertFrom, clipPath, bottomLeft, bottomRight, topLeft);
}

MyError PDriver::setDriver(const OS::Regs& regs)
{
    return configure_setdriver(regs, CoreWS::instance());
}

MyError PDriver::declareFont(uint32_t fontHandle, const char* name,
                             uint32_t flags)
{
    CoreWS& ws = CoreWS::instance();

    if (ws.currentJob() == nullptr)
        return ws.messages.lookupError(ErrorBlock_PrintNoJobSelected);

    CoreJobWS& job = *ws.currentJob();
    if (job.hasPersistentError())
        return job.getPersistentError();

    if (job.copiestodo != 0)
        return ws.messages.lookupError(ErrorBlock_PrintNoCurrentPage);

    debugLog("font_declare %d '%s'", fontHandle, name ? name : "<null>");

    // Font's identifier valid until this goes out of scope.
    // font_declare copies it.
    Font::ReadDefn defn;

    if (fontHandle != 0) {
        MyError err;
        uint32_t size_bytes = 0;

        if ((err = defn.init(fontHandle)) != nullptr)
            return err;

        name = defn.getIdentifier().text();
        debugLog("font_declare ReadDefn '%s'", name);
    }

    return font_declare(name, flags, job);
}
