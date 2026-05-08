#include "Core/PDriver.h"
#include "ManageJob.h"

#include "JobWS.h"
#include "Output.h"
#include "PDriverPDF.h"
#include "Private.h"

#include "Core/Job.h"
#include "Core/MsgCode.h"
#include "Core/OS.h"
#include "Core/Workspace.h"

#include <stddef.h>
#include <string.h>

static const char for_name[] = "PDriver$For";
static const char address_name[] = "PDriver$Address";

CoreJobWS* managejob_allocate(FileHandle file, bool illustration, CoreWS& ws)
{
    JobWS* job;

    if (ws.globalInfo.isColour())
        job = new ColourJob(file, illustration, ws);
    else
        job = new JobWS(file, illustration, ws);

    if (!job)
        return nullptr;

    return job;
}

void managejob_destroy(CoreJobWS* coreJob)
{
    delete coreJob;
}

MyError managejob_init(const char* title, CoreJobWS& coreJob)
{
    JobWS& job(toJobWS(coreJob));
    MyError err;

#if PSDebugManageJ
    Output& output(job.output());

    if ((err = output.str("% managejob_init\n")) != nullptr)
        return err;
#endif

    /* PDF output never uses CTRL-D/prologues. */
    job.jobverbose = false;
    job.jobaccents = false;

    job.m_context.reset();
    job.jobclipped = false;
    job.jobbaseclip = 0;

    /* No PostScript prologue in PDF output. */
    job.sendprologue = 0;

    /* Zero the font mapping table. */
    job.clearMappedFonts();

    /* VDU5 character cache starts empty. */
    memset(job.vducharsdefined, 0, sizeof(job.vducharsdefined));
    memset(job.vduchardata, 0, sizeof(job.vduchardata));

    if ((err = pdf_begin_document(job, title)) != nullptr)
        return err;

    return nullptr;
}

MyError managejob_finalise(CoreJobWS& coreJob)
{
    MyError err;
    JobWS& job(toJobWS(coreJob));

#if PSDebugManageJ
    Output& output(job.output());
    if ((err = output.str("% managejob_finalise\n")) != nullptr)
        return err;
#endif

    return pdf_end_document(job);
}

MyError managejob_suspend(CoreJobWS& job)
{
    Output& output(toJobWS(job).output());
    return output.flush();
}

MyError managejob_resume(CoreJobWS& job)
{
    return nullptr;
}

MyError managejob_abort(CoreJobWS& job)
{
    // Set the file extent to 0 if the job is aborted.
    return nullptr; // FIXME: OS::setExtent(job.jobhandle, 0);
}
