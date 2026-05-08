#include "Core/PDriver.h"
#include "ManageJob.h"

#include "GlobalWS.h"
#include "JobWS.h"
#include "Output.h"
#include "PDriverPS.h"
#include "PageBox.h"
#include "Private.h"

#include "Core/Job.h"
#include "Core/MsgCode.h"
#include "Core/OS.h"
#include "Core/Workspace.h"

#include <stddef.h>
#include <string.h>

extern const char epilogue_filename[];

inline bool varExists(const char* name) {
    return OS::readVarValSize(name, OS::VarType_String) != 0;
}

enum FontSupplied { FontSupplied_yes, FontSupplied_no };
static MyError managejob_outputfontcomment(FontSupplied supplied,
                                           Output& output,
                                           JobWS& job)
{
    debugLog("managejob fontcomment supplied:%u job:%p list:%p",
             supplied == FontSupplied_yes ? 1 : 0,
             &job,
             &job.jobfontlist);
    MyError err;

    if ((err = output.str("%%Document")) != nullptr)
        return err;
    if (supplied == FontSupplied_yes) {
        if ((err = output.str("Supplied")) != nullptr)
            return err;
    }
    if ((err = output.str("Fonts: ")) != nullptr)
        return err;

    bool firstLine = true;
    // Original asm called PDriver_MiscOp. I have no idea why since that goes
    // straight to the given Job's font list.
    for (FontNameEntry* font = job.jobfontlist.head(); font != nullptr; font = font->next())
    {
        FontNameEntry::Word word = font->word();
        debugLog("managejob fontcomment consider block:%p riscos:%s alien:%s word:0x%x supplied:%u",
                 font,
                 font->riscos(),
                 font->alien() != nullptr ? font->alien() : "<null>",
                 word,
                 supplied == FontSupplied_yes ? 1 : 0);

        bool wanted = word == PDriverMiscOp_PS_DSF;
        if (!wanted && supplied == FontSupplied_no)
            wanted = word == PDriverMiscOp_PS_DF;


        if (wanted) {
            debugLog("managejob fontcomment emit riscos:%s alien:%s word:0x%x supplied:%u",
                     font->riscos(),
                     font->alien() != nullptr ? font->alien() : "<null>",
                     word,
                     supplied == FontSupplied_yes ? 1u : 0u);
            if (!firstLine) {
                if ((err = output.str("%%+ ")) != nullptr)
                    return err;
            }

            firstLine = false;

            if ((err = output_string(font->riscos(), 32, 126, output)) != nullptr)
                return err;
            if ((err = output.str('\n')) != nullptr)
                return err;
        }
    }

    return output.str('\n');
}

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

MyError managejob_init(const char *title, CoreJobWS& coreJobWS)
{
    MyError err;
    JobWS& job = toJobWS(coreJobWS);
    PDriverWS& psWS = PDriverWS::instance();

    // We want a CTRL-D if it's not an illustration job but not if it is.
    PS::Document::Flag flags = psWS.m_flags;
    if (!job.illustrationjob)
        flags = flags | PS::Document::Flag_UseCtrlD;
    else
        flags = flags & ~PS::Document::Flag_UseCtrlD;

    job.document() = PS::Document(flags, psWS.m_level);

    Output& output(job.output());

#if PSDebugManageJ
    if ((err = output.str("% managejob_init\n")) != nullptr)
        return err;
#endif

    /* Output print job header comments. */
    if ((err = output.str("%!PS-Adobe-2.0")) != nullptr)
        return err;
    if (coreJobWS.illustrationjob != 0) {
        if ((err = output.str(" EPSF-1.2")) != nullptr)
            return err;
    }
    if ((err = output.str('\n')) != nullptr)
        return err;

    if ((err = output.str("%%Creator: RISC OS PDriverPS $Module_FullVersion")) != nullptr)
        return err;
    if ((err = output.str('\n')) != nullptr)
        return err;

    if ((err = output.str("%%CreationDate: ")) != nullptr)
        return err;
    if ((err = output_variable("Sys$Time", output)) != nullptr)
        return err;
    if ((err = output.str(' ')) != nullptr)
        return err;
    if ((err = output_variable("Sys$Date", output)) != nullptr)
        return err;
    if ((err = output.str(' ')) != nullptr)
        return err;
    if ((err = output_variable("Sys$Year", output)) != nullptr)
        return err;
    if ((err = output.str('\n')) != nullptr)
        return err;

    /* Next the title string pointed to by R1, if it exists. */
    if (title != nullptr) {
        if ((err = output.str("%%Title: ")) != nullptr)
            return err;
        if ((err = output_string(title, 32, 126, output)) != nullptr)
            return err;
        if ((err = output.str('\n')) != nullptr)
            return err;
    }

    /* Now the intended recipient in PDriver$For, if it exists. */
    if (varExists("PDriver$For")) {
        if ((err = output_gstring("%%For: <PDriver$For>", output)) != nullptr)
            return err;
        if ((err = output.str('\n')) != nullptr)
            return err;
    }

    /* And the intended address/routing in PDriver$Address, if that exists. */
    if (varExists("PDriver$Address")) {
        if ((err = output_gstring("%%Routing: <PDriver$Address>", output)) != nullptr)
            return err;
        if ((err = output.str('\n')) != nullptr)
            return err;
    }

    if ((err = output.str("%%Pages: (atend)")) != nullptr)
        return err;
    if ((err = output.str('\n')) != nullptr)
        return err;
    if ((err = output.str("%%BoundingBox: (atend)")) != nullptr)
        return err;
    if ((err = output.str('\n')) != nullptr)
        return err;
    if ((err = output.str("%%DocumentFonts: (atend)")) != nullptr)
        return err;
    if ((err = output.str('\n')) != nullptr)
        return err;
    if ((err = output.str("%%DocumentSuppliedFonts: (atend)")) != nullptr)
        return err;
    if ((err = output.str('\n')) != nullptr)
        return err;

    return nullptr;
}

MyError managejob_finalise(CoreJobWS& coreJob)
{
    JobWS& job = toJobWS(coreJob);
    CoreWS& ws = CoreWS::instance();
    PS::Document& document(job.document());
    MyError err;

    Output& output(job.output());

#if PSDebugManageJ
    if ((err = output.str("% managejob_finalise\n")) != nullptr)
        return err;
#endif

    /* Start the trailer section. */
    if ((err = output.str("%%Trailer")) != nullptr)
        return err;
    if ((err = output.str('\n')) != nullptr)
        return err;

    /* Give the number of pages produced. */
    if ((err = output.str("%%Pages: ")) != nullptr)
        return err;
    if ((err = output.num(job.numberofpages)) != nullptr)
        return err;
    if ((err = output.str('\n')) != nullptr)
        return err;

    /* Give the resulting bounding box (or null if nothing printed). */
    Rect<PS::Unit> bounds(document.boundingBoxOrEmpty());

    if ((err = output.str("%%BoundingBox: ")) != nullptr)
        return err;
    if ((err = output.writeCoordPair(bounds.bottomLeft())) != nullptr)
        return err;
    if ((err = output.writeCoordSpace(bounds.x1)) != nullptr)
        return err;
    if ((err = output.writeCoord(bounds.y1)) != nullptr)
        return err;
    if ((err = output.str('\n')) != nullptr)
        return err;

    /* Output the DocumentFonts stuff (if not verbose). */
    if (document.fonts().hasDeclaredFontEntries()) {
        if ((err = managejob_outputfontcomment(FontSupplied_no, output, job)) != nullptr)
            return err;
        if ((err = managejob_outputfontcomment(FontSupplied_yes, output, job)) != nullptr)
            return err;
    }

    /* Output the Epilogue file (if present) - ignore errors. */
    if (err == nullptr) {
        (void)pagebox_insertfile(epilogue_filename, job);
    }

    /* And terminate with a CTRL-D if required. */
    if (document.useCtrlD()) {
        if ((err = output.byte(4)) != nullptr)
            return err;
    }

    /* Make sure output is flushed. */
    return output.flush();
}

MyError managejob_suspend(CoreJobWS& job)
{
    // Must not produce printer output, yet this clearly can...
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
