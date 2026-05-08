#include "Core/PDriver.h"

#include "Colour.h"
#include "Output.h"
#include "PDriverPDF.h"
#include "Private.h"

#include "Core/Device.h"
#include "Core/Job.h"
#include "Core/Workspace.h"

#include <stddef.h>

/* Page control and box generation routines for the PDF printer driver. */

static MyError pagebox_clearclips(JobWS& jobWS)
{
    MyError err = nullptr;
    Output& output(jobWS.output());

    if ((err = ensure_OScoords(output, jobWS)) != nullptr)
        return err;

    if (jobWS.jobclipped) {
        if ((err = output.str("Q\n")) != nullptr)
            return err;

        jobWS.jobclipped = false;
    }
    while (jobWS.jobbaseclip != 0) {
        if ((err = output.str("Q\n")) != nullptr)
            return err;

        jobWS.jobbaseclip -= 1;
    }
    return nullptr;
}


static MyError output_user_transform(CoreJobWS& coreJob,
                                     Output& output)
{
    /* Match the user rectangle transform established by pagebox_nextbox. */
    MyError err;
    if ((err = output.writeReal(coreJob.userstransform.a.raw(), 65536)) != nullptr)
        return err;
    if ((err = output.str(' ')) != nullptr)
        return err;
    if ((err = output.writeReal(coreJob.userstransform.b.raw(), 65536)) != nullptr)
        return err;
    if ((err = output.str(' ')) != nullptr)
        return err;
    if ((err = output.writeReal(coreJob.userstransform.c.raw(), 65536)) != nullptr)
        return err;
    if ((err = output.str(' ')) != nullptr)
        return err;
    if ((err = output.writeReal(coreJob.userstransform.d.raw(), 65536)) != nullptr)
        return err;
    if ((err = output.str(' ')) != nullptr)
        return err;
    if ((err = output.writeReal(coreJob.usersbottomleft.x.raw(), 400)) != nullptr)
        return err;
    if ((err = output.str(' ')) != nullptr)
        return err;
    if ((err = output.writeReal(coreJob.usersbottomleft.y.raw(), 400)) != nullptr)
        return err;
    return output.str(" cm\n");
}

MyError pagebox_setup(int32_t copies,
                      uint32_t page_sequence,
                      const char *page_number,
                      CoreJobWS& coreJob)
{
    CoreWS& ws = CoreWS::instance();
    JobWS& jobWS = (JobWS&)coreJob;
    (void)copies;
    (void)page_sequence;
    (void)page_number;

    ws.m_countingPass = false;

#if PSDebugPageBox
    Output& output(jobWS.output());
    MyError err = output.str("% pagebox_setup\n");
    if (err) {
        return err;
    }
#endif

    jobWS.currentrect = coreJob.rectlist.head();
    jobWS.jobbaseclip = 0;
    jobWS.jobclipped = false;
    jobWS.coordsystem = 0;

    return pdf_begin_page(Rect<PDF::Point>(0, 0,
                                           coreJob.pageSize.size.width,
                                           coreJob.pageSize.size.height),
                          jobWS);
}

MyError pagebox_nextbox(uint32_t& copies,
                        uint32_t& rectId,
                        Geometry::Rect<OS::Unit>& box,
                        CoreJobWS& coreJob)
{
    MyError err;
    JobWS& job = (JobWS&)coreJob;
#if PSDebugPageBox
    Output& output(job.output());
    if ((err = output.str("% pagebox_nextbox\n")) != nullptr)
        return err;
#endif

    UserRectangle *rect = job.currentrect;
    if (rect != nullptr)
        job.currentrect = rect->next();

    if (rect == nullptr) {
        if ((err = pagebox_clearclips(job)) != nullptr)
            return err;
        if ((err = pdf_end_page(job)) != nullptr)
            return err;

        copies = 0;

        return nullptr;
    }

    vdu5_changed(coreJob, (uint32_t)-1);

    job.usersbg = rect->rectanglebg;

    job.usersbottomleft = rect->rectbottomleft;

    job.userstransform = rect->recttransform;
    job.usersoffset = rect->rectoffset;
    job.usersbox = rect->rectbox;

    rectId = rect->rectangleid;
    box = Geometry::Rect<OS::Unit>(Offset<OS::Unit>(0,0), rect->rectbox);

    return nullptr;
}

MyError pagebox_setmaxbox(CoreJobWS& coreJob)
{
    MyError err;
    JobWS& jobWS = (JobWS&)coreJob;
    Output& output(jobWS.output());
#if PSDebugPageBox
    if ((err = output.str("% pagebox_setmaxbox\n")) != nullptr)
        return err;
#endif

    if ((err = pagebox_clearclips(jobWS)) != nullptr)
        return err;

    if ((err = output.str("q\n")) != nullptr)
        return err;

    /* The PS backend treats setmaxbox as a clip reset.  Keep explicit
       pagebox_setnewbox clips separate so ordinary output is not cropped
       to the often-tight user rectangle. */
    if ((err = output_user_transform(coreJob, output)) != nullptr)
        return err;

    jobWS.jobbaseclip = 1;
    return nullptr;
}

MyError pagebox_cleartobg(CoreJobWS& coreJob)
{
    MyError err;
#if PSDebugPageBox
    JobWS& job = (JobWS&)coreJob;
    Output& output(job.output());
    if ((err = output.str("% pagebox_cleartobg\n")) != nullptr)
        return err;
#endif

    if ((err = colour_setrealrgb(coreJob.usersbg, coreJob)) != nullptr)
        return err;

    return plot_fillclipbox(coreJob);
}

MyError pagebox_setnewbox(const Rect<OS::Unit>& box, CoreJobWS& coreJob)
{
    JobWS& jobWS = (JobWS&)coreJob;
#if PSDebugPageBox
    {
        Output& output(jobWS.output());
        MyError err = output.str("% pagebox_setnewbox\n");
        if (err) {
            return err;
        }
    }
#endif

    MyError err;

    if ((err = pagebox_setmaxbox(coreJob)) != nullptr)
        return err;

    Output& output(jobWS.output());
    if ((err = output.str("q\n")) != nullptr)
        return err;

    if ((err = output.writeRect(box)) != nullptr)
        return err;
    if ((err = output.str("W\nn\n")) != nullptr)
        return err;

    jobWS.jobclipped = true;
    return nullptr;
}

/* PDF backend does not emit PS prologues. */
