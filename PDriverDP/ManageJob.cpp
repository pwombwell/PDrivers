#include "Core/PDriver.h"

#include "GlobalWS.h"
#include "JobWS.h"
#include "PDriverDP.h"
#include "Private.h"

#include "Core/Constants.h"
#include "Core/InterceptManager.h"
#include "Core/Job.h"
#include "Core/MsgCode.h"
#include "Core/OS.h"
#include "Core/Workspace.h"

#include "RLib/OS/Error.h"
#include "RLib/OS/File.h"
#include "RLibX/Sprite.h"

static MyError set_printer_ignore(JobWS& jobWS)
{
    uint32_t r0 = 182;
    uint32_t r1 = 255;
    uint32_t r2 = 0;
    MyError err = XOS_Byte(&r0, &r1, &r2);
    if (!err) {
        jobWS.output().setSavedPrinterIgnore((uint8_t)r1);
    }
    return err;
}

static MyError restore_printer_ignore(const JobWS& jobWS)
{
    uint32_t r0 = 182;
    uint32_t r1 = jobWS.output().savedPrinterIgnore();
    uint32_t r2 = 0;
    return XOS_Byte(&r0, &r1, &r2);
}

static MyError losebuffersprite(JobWS& job, DP::BufferSprite& buffer)
{
    ScopedPassthrough scopedPT(PDriverWS::instance().m_interceptMgr, Passthrough_Sprite);

    if (!buffer.hasName())
        return nullptr;

    Sprite::Selector selector(myspriteop(job, buffer));
    MyError err = Sprite::deleteSprite(selector);
    buffer.reset();
    return err;
}

static MyError managejob_abortalt(JobWS& job, uint32_t fatalityFlags)
{
    MyError firstErr = restore_printer_ignore(job);
    MyError err = restore_output_state(job);
    if (!firstErr && err) {
        firstErr = err;
    }

#if Libra1
    /* Attempt to shrink the RMA for old strip types. */
    {
        int32_t change = -1000 * 1024;
        (void)XOS_ChangeDynamicArea(1, change);
    }
#endif

    PDumperCall call = {0};
    call.r0 = (uint32_t)(uintptr_t)&job.config().pdumperWord();
    call.r1 = job.jobhandle;
    call.r2 = job.config().stripType();
    call.r3 = fatalityFlags;
#if Libra1
    if (fatalityFlags & (1u << 24)) {
        call.r4 = (uint32_t)(uintptr_t)job.config().configBytes();
    }
#endif
    err = CallPDumperForJob(job, PDumperReason_AbortJob, call);
    if (!firstErr && err) {
        firstErr = err;
    }

#if Libra1
    {
        uint32_t fatality = fatalityFlags & ~(1u << 24);
        if (fatality == 2)
            OS::setExtent(job.jobhandle, 0);
    }
#endif

    /* Delete buffer sprites. */
    {
        uint32_t passes = job.config().numPasses();
        if (passes == 0u)
            passes = 1u;
        if (job.config().stripType() == 3)
            passes += 1;

        for (uint32_t index = 1; index <= passes; ++index) {
            (void)losebuffersprite(job, job.output().lineBuffer(index));
        }
    }
    job.output().lineBuffer(0).reset();

    (void)losebuffersprite(job, job.output().rotationBuffer());

    if (job.output().vduSaveArea() != nullptr) {
        (void)rma_free(job.output().vduSaveArea());
        job.output().vduSaveArea() = nullptr;
    }

    if (job.output().ownsSpriteAreaAllocation()) {
        (void)rma_free((const void *)(uintptr_t)job.output().spriteArea());
        job.output().spriteAreaRef() = nullptr;
    }

    if (job.output().sAreaChange() != 0) {
        int32_t change = -(int32_t)job.output().sAreaChange();
        (void)XOS_ChangeDynamicArea(3, change);
        job.output().sAreaChange() = 0;
    }

    job.output().markSpriteAreaUnavailable();
    (void)ColourTrans::invalidateCache();

    return firstErr;
}

CoreJobWS* managejob_allocate(FileHandle file, bool illustration, CoreWS& ws)
{
    PDriverWS& dpWS = ws.toPDriverWS();

    if (dpWS.printer_pdumper_pointer == nullptr) {
        const char *cmd = dpWS.pending_pdumper_command;
        if (cmd && cmd[0] != '\0') {
            MyError cliErr = XOS_CLI(cmd);
            if (cliErr) {
                return nullptr;
            }
        }
        if (dpWS.printer_pdumper_pointer == nullptr)
            return nullptr;
    }

    JobWS* newJob = new JobWS(file, illustration, ws);
    if (newJob == nullptr)
        return nullptr;

    newJob->config().copyFrom(dpWS);

    return newJob;
}

void managejob_destroy(CoreJobWS* coreJobWS)
{
    if (coreJobWS == nullptr)
        return;

    delete &toJobWS(*coreJobWS);
}

// `font_fontslost`
void font_fontslost(CoreJobWS& coreJob)
{
    JobWS& jobWS = toJobWS(coreJob);
    memset(jobWS.slavedFonts(), 0, 256u);
}

MyError managejob_init(const char *title, CoreJobWS& coreJobWS)
{
    (void)title;
    JobWS& job(toJobWS(coreJobWS));

    job.output().vduSaveArea() = nullptr;
    job.output().savedVduState().unset(); // no redirection yet
    job.output().currentRectRef() = nullptr;
    job.output().currentBuffer() = nullptr;
    for (uint32_t i = 0; i < max_passes + 2; ++i)
        job.output().lineBuffer(i).reset();
    job.output().rotationBuffer().reset();
    job.output().setDumpPassImageCount(0);
    for (uint32_t i = 0; i < max_passes + 1; ++i)
        job.output().dumpPassImages()[i] = nullptr;
    job.output().setBufferMarked(false);
    job.plotWS().clearDashStyle();
    job.output().sAreaChange() = 0;
    job.config().setPass(0);

    (void)font_fontslost(coreJobWS);

    job.output().markSpriteAreaUnavailable();

    int32_t halftone = Divide((int32_t)coreJobWS.info.pixelResX(),
                              (int32_t)coreJobWS.info.htoneResX());
    if (halftone >= 256) {
        return CoreWS::instance().messages.lookupError(ErrorBlock_PrintBadHalftone);
    }
    job.output().setHalftoneX((uint8_t)halftone);

    halftone = Divide((int32_t)coreJobWS.info.pixelResY(),
                      (int32_t)coreJobWS.info.htoneResY());
    if (halftone >= 256) {
        return CoreWS::instance().messages.lookupError(ErrorBlock_PrintBadHalftone);
    }
    job.output().setHalftoneY((uint8_t)halftone);

    PDumperCall call = {0};
    call.r0 = (uint32_t)(uintptr_t)&job.config().pdumperWord();
    call.r1 = (uint32_t)coreJobWS.jobhandle;
    call.r2 = job.config().stripType();

    return CallPDumperForJob(job, PDumperReason_StartJob, call);
}

MyError managejob_finalise(CoreJobWS& coreJobWS)
{
    return managejob_abortalt(toJobWS(coreJobWS), 1);
}

MyError managejob_suspend(CoreJobWS& coreJob)
{
    JobWS& job(toJobWS(coreJob));

    MyError err = restore_printer_ignore(job);
    if (err) {
        (void)job.makePersistentError(err);
        return err;
    }

    err = restore_output_state(job);
    if (err) {
        (void)job.makePersistentError(err);
        return err;
    }

    job.output().savedVduState().unset();

    return nullptr;
}

MyError managejob_resume(CoreJobWS& coreJob)
{
    JobWS& job(toJobWS(coreJob));

    if (job.output().hasSpriteAreaSelection()) {
        MyError err = redirect_output(job, CoreWS::instance());
        if (err) {
            (void)job.makePersistentError(err);
            return nullptr;
        }
    }

    MyError err = set_printer_ignore(job);
    if (err) {
        (void)job.makePersistentError(err);
    }
    return nullptr;
}

MyError managejob_abort(CoreJobWS& coreJobWS)
{
    uint32_t fatality = 2;
#if Libra1
    fatality |= (1u << 24);
#endif

    return managejob_abortalt(toJobWS(coreJobWS), fatality);
}
