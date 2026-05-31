#include "kernel.h"

#include "Core/PDriver.h"

#include "GlobalWS.h"
#include "JobWS.h"
#include "Private.h"

#include "Core/Constants.h"
#include "Core/Device.h"
#include "Core/Job.h"
#include "Core/MsgCode.h"
#include "Core/OS.h"
#include "Core/Workspace.h"

#include "RLib/OS/Error.h"

extern const uint32_t Module_Version;

extern const uint32_t PDriverMiscOp_DriverSpecific;
extern const uint32_t PDumperMiscOp_DumperSpecific;

static MyError miscop_removepdumper(CoreWS& ws, OS::Regs& args);

static MyError miscop_addpdumper(CoreWS& ws, OS::Regs& args)
{
    PDriverWS *dp_ws = (PDriverWS *)&ws;

    uint32_t pdumper_number = args[1];
    uint32_t required_version = args[2];
    void *workspace = (void *)(uintptr_t)args[3];
    void *branch_table = (void *)(uintptr_t)args[4];
    uint32_t branch_mask = args[5];
    StripTypeMask strip_mask = StripTypeMask(args[6]);

    if (required_version > Module_Version) {
        return ws.messages.lookupError(ErrorBlock_PDumperTooOld);
    }

    MyError err = miscop_removepdumper(ws, args);
    if (err && err.errnum() != ErrorBlock_PDumperUndeclared.errnum.errnum) {
        return err;
    }

    PDumper* iter = dp_ws->pdumper_list.head();
    while (iter) {
        if (iter->number == pdumper_number) {
            return ws.messages.lookupError(ErrorBlock_PDumperDuplicateModule);
        }
        iter = iter->next();
    }

    PDumper* pdumper = new PDumper;
    if (pdumper == nullptr)
        return MyError::OOM();

    pdumper->number = pdumper_number;
    pdumper->version = required_version;
    pdumper->workspace = workspace;
    pdumper->branch_table = branch_table;
    pdumper->branches = branch_mask;
    pdumper->striptypemask = strip_mask;

    dp_ws->pdumper_list.addHead(pdumper);

    if (dp_ws->printer_pdumper_number == pdumper_number) {
        dp_ws->printer_pdumper_pointer = pdumper;

        if (dp_ws->pending_info_flag != 0) {
            OS::Regs sd;
            sd[1] = dp_ws->printer_pdumper_number;
            sd[2] = (uint32_t)(uintptr_t)dp_ws->pending_pdumper_command;
            sd[3] = (uint32_t)(uintptr_t)dp_ws->pending_info_data;
            sd[4] = (uint32_t)(uintptr_t)&dp_ws->printer_dump_depth;
            sd[5] = dp_ws->printer_configureword;
            if ((err = configure_setdriver(sd, ws)) != nullptr)
                return err;
            dp_ws->pending_info_flag = 0;
        }
    }

    return nullptr;
}

static MyError miscop_removepdumper(CoreWS& ws, OS::Regs& args)
{
    PDriverWS *dp_ws = (PDriverWS *)&ws;
    uint32_t pdumper_number = args[1];

    /* Check if the dumper is in use by any job. */
    CoreJobWS* job = ws.jobMgr().firstJob();
    while (job) {
        JobWS& dpJob = toJobWS(*job);

        if (dpJob.config().pdumperNumber() == pdumper_number) {
            return ws.messages.lookupError(ErrorBlock_PDumperInUse);
        }

        job = ws.jobMgr().nextJob(job);
    }

    PDumper* iter = dp_ws->pdumper_list.head();
    while (iter) {
        if (iter->number == pdumper_number) {
            break;
        }
        iter = iter->next();
    }

    if (!iter)
        return ws.messages.lookupError(ErrorBlock_PDumperUndeclared);

    dp_ws->pdumper_list.remove(iter);

    if (dp_ws->printer_pdumper_number == pdumper_number) {
        dp_ws->printer_pdumper_number = -1;
        dp_ws->printer_pdumper_pointer = nullptr;
        dp_ws->pending_pdumper_command[0] = 0;
    }

    return nullptr;
}

static void miscop_getpdumperstriptypemask(CoreWS& ws, OS::Regs& args)
{
    PDriverWS *dp_ws = (PDriverWS *)&ws;
    uint32_t pdumper_number = args[1];

    PDumper* iter = dp_ws->pdumper_list.head();
    while (iter) {
        if (iter->number == pdumper_number) {
            args[0] = iter->striptypemask;
            return;
        }
        iter = iter->next();
    }
}

MyError miscop_decode(uint32_t reason, OS::Regs& args, CoreWS& ws)
{
    uint32_t r0 = reason & ~TopBit;

    uint32_t pdumper_base = PDumperMiscOp_DumperSpecific - PDriverMiscOp_DriverSpecific;
    if (r0 >= pdumper_base) {
        PDumperCall call = {0};
        call.r0 = r0;
        call.r1 = args[1];
        call.r2 = args[2];
        call.r3 = args[3];
        call.r4 = args[4];
        call.r5 = args[5];
        call.r6 = args[6];
        call.r7 = args[7];

        MyError err = nullptr;
        if (ws.currentJob() != nullptr)
{            JobWS* job = (JobWS*)ws.currentJob();
            err = CallPDumperForJob(*job, PDumperReason_MiscOp, call);
        } else {
            err = CallPDumper(ws, PDumperReason_MiscOp, call);
        }

        args[0] = call.r0;
        args[1] = call.r1;
        args[2] = call.r2;
        args[3] = call.r3;
        args[4] = call.r4;
        args[5] = call.r5;
        args[6] = call.r6;
        args[7] = call.r7;

        return err;
    }

    switch (r0) {
    case 0:
        return miscop_addpdumper(ws, args);
    case 1:
        return miscop_removepdumper(ws, args);
    case 2:
        miscop_getpdumperstriptypemask(ws, args);
        return nullptr;
    default:
        return ws.messages.lookupError(ErrorBlock_PrintBadMiscOp);
    }
}
