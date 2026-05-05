#include "cmhg.h"

#include "PDriver.h"

#include "Colour.h"
#include "Constants.h"
#include "CtrlString.h"
#include "Device.h"
#include "InterceptOSByte.h"
#include "InterceptColTrans.h"
#include "InterceptDraw.h"
#include "InterceptFont.h"
#include "InterceptManager.h"
#include "InterceptFont.h"
#include "InterceptJPEG.h"
#include "InterceptSprite.h"
#include "InterceptWrch.h"
#include "Job.h"
#include "JobManager.h"
#include "MsgCode.h"
#include "OS.h"
#include "PDriver.h"
#include "UserRectangle.h"
#include "VDU5.h"
#include "Workspace.h"

#include "RLib/OS/Error.h"
#include "RLib/KernelWrapper.h"

#include <stddef.h>
#include <string.h>

#include <stdio.h>

enum PDriverMiscOp {
    PDriverMiscOp_AddFont       = 0,
    PDriverMiscOp_RemoveFonts   = 1,
    PDriverMiscOp_ListFonts     = 2
};

static FontBlockList& miscop_fontList(CoreWS& ws);

static MyError miscop_addfont(const OS::Regs& regs, CoreWS& ws)
{
    CtrlString riscosName(regs.as<const char*>(1));
    CtrlString alienName(regs.as<const char*>(2));
    FontBlockWord word = regs.as<FontBlockWord>(3);
    FontBlockAddFlag addFlags = regs.as<FontBlockAddFlag>(4);

    debugLog("miscop addfont riscos:%.*s alien:%.*s word:0x%x flags:0x%x currentJob:%p",
             riscosName.length(), riscosName.text(),
             alienName.length(), alienName.text(),
             word,
             addFlags,
             CoreWS::instance().currentJob());

    return miscop_fontList(ws).addFont(riscosName, alienName, word, addFlags);
}

static MyError miscop_removefonts(CoreWS& ws)
{
    miscop_fontList(ws).removeFonts();
    return nullptr;
}

static MyError miscop_enumfonts(OS::Regs& regs, CoreWS& ws)
{
    FontBlockEnumerationRecord* buffer = regs.as<FontBlockEnumerationRecord*>(1);
    uint32_t& size = regs[2];
    uint32_t& handle = regs[3]; // enumeration handle (aliases into regs)

    miscop_fontList(ws).enumerate(buffer, size, handle);

    // Update buffer (size and handle are updated, too).
    regs.set<FontBlockEnumerationRecord>(1, buffer);

    debugLog("enumfonts - returning handle: %u size:%u", handle, size);

    return nullptr;
}

_kernel_oserror* PDriver_Info_Handler(int number, _kernel_swi_regs* r, void* pw)
{
    OS::KernelRegsWrapper regs(r);

    uint32_t version;
    const PDriverInfo& info = PDriver::info(version);

    regs[0] = version;
    regs[1] = info.pixelResX();
    regs[2] = info.pixelResY();
    regs[3] = info.features();
    regs.set(4, info.printerName());
    regs[5] = info.htoneResX();
    regs[6] = info.htoneResY();
    regs[7] = info.printer();

    debugLog("info returning version:0x%x resX:%u resY:%u features:0x%x printer:%u name:%s",
             regs[0],
             regs[1],
             regs[2],
             regs[3],
             regs[7],
             regs.as<const char*>(4));

    return nullptr;
}

_kernel_oserror* PDriver_SetInfo_Handler(int number, _kernel_swi_regs* r, void* pw)
{
    OS::KernelRegsWrapper regs(r);

    debugLog("setInfo setting resX:%u resY:%u features:%x", regs[1], regs[2], regs[3]);

    uint32_t pixelResX(regs[1]);
    uint32_t pixelResY(regs[2]);
    PDriverInfo::Features features = regs.as<PDriverInfo::Features>(3);
    const char* printerName = regs.as<const char*>(4);
    uint32_t htoneResX = regs[5];
    uint32_t htoneResY = regs[6];
    uint32_t printer = regs[7];

    SetPDriverInfo info(pixelResX, pixelResY, features, printerName,
                        htoneResX, htoneResY, printer);

    return PDriver::setInfo(info).toKernelOSError();
}


_kernel_oserror* PDriver_SelectIllustration_Handler(int number,
                                                    _kernel_swi_regs* r,
                                                    void* pw)
{
    OS::KernelRegsWrapper regs(r);

    debugLog("selectIllustration  file:%u", regs[0]);

    FileHandle handle(regs[0]);
    const char* title = regs.as<const char*>(1);
    FileHandle previous;
    MyError err = PDriver::selectIllustration(handle, title, previous);
        debugLog("selectIllustration returned");

    if (err)
        return err.toKernelOSError();

    regs[0] = previous;
    debugLog("selectIllustration finishing previous:%u", regs[0]);

    return nullptr;
}

_kernel_oserror* PDriver_CurrentJob_Handler(int number,
                                            _kernel_swi_regs *r,
                                            void *pw)
{
    OS::KernelRegsWrapper regs(r);

    debugLog("currentjob");

    regs[0] = PDriver::currentJob();

    return nullptr;
}

_kernel_oserror* PDriver_EnumerateJobs_Handler(int number,
                                               _kernel_swi_regs *r,
                                               void *pw)
{
    OS::KernelRegsWrapper regs(r);

    debugLog("enumeratejobs from:%u", regs[0]);

    FileHandle jobHandle(regs[0]);
    CoreJobWS* nextJob;
    MyError err = PDriver::enumerateJobs(jobHandle, nextJob).toKernelOSError();
    if (err)
        return err.toKernelOSError();

    regs[0] = nextJob != nullptr ? nextJob->jobhandle : 0;

    return nullptr;
}

_kernel_oserror* PDriver_CheckFeatures_Handler(int number,
                                               _kernel_swi_regs* r,
                                               void* pw)
{
    OS::KernelRegsWrapper regs(r);

    debugLog("checkfeatures r0:%u r1:%u", regs[0], regs[1]);

    return PDriver::checkFeatures(regs).toKernelOSError();
}

_kernel_oserror* PDriver_PageSize_Handler(int number,
                                          _kernel_swi_regs* r,
                                          void* pw)
{
    OS::KernelRegsWrapper regs(r);

    const PDriverPageSize& pageSize = PDriver::pageSize();

    regs[1] = pageSize.size.width.raw();
    regs[2] = pageSize.size.height.raw();
    regs[3] = pageSize.rect.x0.raw();
    regs[4] = pageSize.rect.y0.raw();
    regs[5] = pageSize.rect.x1.raw();
    regs[6] = pageSize.rect.y1.raw();

    debugLog("pagesize returning w:%d h:%d",
             pageSize.size.width.raw(), pageSize.size.height.raw());

    return nullptr;
}

_kernel_oserror* PDriver_SetPageSize_Handler(int number,
                                             _kernel_swi_regs* r,
                                             void* pw)
{
    OS::KernelRegsWrapper regs(r);

    debugLog("setpagesize w:%d h:%d", regs[1], regs[2]);

    PDriverPageSize pageSize;
    pageSize.size.width = regs[1];
    pageSize.size.height = regs[2];
    pageSize.rect.x0 = regs[3];
    pageSize.rect.y0 = regs[4];
    pageSize.rect.x1 = regs[5];
    pageSize.rect.y1 = regs[6];

    PDriver::setPageSize(pageSize);

    return nullptr;
}

_kernel_oserror* PDriver_SelectJob_Handler(int number,
                                           _kernel_swi_regs* r,
                                           void* pw)
{
    OS::KernelRegsWrapper regs(r);

    debugLog("selectjob  file:%u", regs[0]);

    FileHandle fileHandle = FileHandle(regs[0]);
    const char* title = regs.as<const char*>(1);

    FileHandle previousHandle;
    MyError err = PDriver::selectJob(fileHandle, title, previousHandle);
    if (err)
        return err.toKernelOSError();

    regs[0] = previousHandle;

    debugLog("selectjob return previous:%u", regs[0]);

    return nullptr;
}

_kernel_oserror* PDriver_EndJob_Handler(int number,
                                        _kernel_swi_regs* r,
                                        void* pw)
{
    OS::KernelRegsWrapper regs(r);

    debugLog("endjob %u", regs[0]);

    _kernel_oserror* e = PDriver::endJob(FileHandle(regs[0])).toKernelOSError();

    debugLog("    endjob returning %p", e);

    return e;
}

_kernel_oserror* PDriver_AbortJob_Handler(int number,
                                          _kernel_swi_regs* r,
                                          void* pw)
{
    OS::KernelRegsWrapper regs(r);

    debugLog("abortjob %u", regs[0]);

    _kernel_oserror* e = PDriver::abortJob(FileHandle(regs[0])).toKernelOSError();

    debugLog("abortjob returning %p", e);

    return e;
}

_kernel_oserror* PDriver_Reset_Handler(int number,
                                       _kernel_swi_regs* r,
                                       void* pw)
{
    debugLog("reset");

    return PDriver::reset().toKernelOSError();
}

_kernel_oserror* PDriver_GiveRectangle_Handler(int number,
                                               _kernel_swi_regs* r,
                                               void* pw)
{
    OS::KernelRegsWrapper regs(r);

    debugLog("giverectangle id:%u", regs[0]);

    uint32_t id = regs[0];
    const Rect<OS::Unit>* box = regs.as<const Rect<OS::Unit>* >(1);
    const Draw::DimensionlessTransform* transform = regs.as<const Draw::DimensionlessTransform*>(2);
    const Point<OS::Millipoint>* bottomLeft = regs.as<const Point<OS::Millipoint>* >(3);
    const uint32_t bgColour = regs[4];

    // FIXME: Check for null and error?

    return PDriver::giveRectangle(id, *box, *transform, *bottomLeft, bgColour).toKernelOSError();
}

_kernel_oserror* PDriver_DrawPage_Handler(int number,
                                          _kernel_swi_regs* r,
                                          void* pw)
{
    OS::KernelRegsWrapper regs(r);

    debugLog("drawpage copies:%u seq:%u", regs[0] & 0xffffff, regs[2]);

    uint32_t copiesRemaining = regs[0];
    Rect<OS::Unit>* rect = regs.as<Rect<OS::Unit>* >(1);
    uint32_t sequenceNumber = regs[2];
    const char* realPageNumber = regs.as<const char*>(3);
    uint32_t rectId;

    if (rect == nullptr) {
        MsgCode& msg(CoreWS::instance().messages);
        return msg.lookupError(ErrorBlock_PrintRectanglesMiss).toKernelOSError();
    }

    MyError err = PDriver::drawPage(copiesRemaining, *rect, sequenceNumber, realPageNumber, rectId);
    if (err)
        return err.toKernelOSError();

    regs[0] = copiesRemaining;
    regs[2] = rectId;

    return nullptr;
}

_kernel_oserror* PDriver_GetRectangle_Handler(int number,
                                              _kernel_swi_regs* r,
                                              void* pw)
{
    OS::KernelRegsWrapper regs(r);

    debugLog("getrectangle");

    Rect<OS::Unit>* rect = regs.as<Rect<OS::Unit>* >(1);
    uint32_t copiesRemaining;
    uint32_t rectId;

    if (rect == nullptr) {
        MsgCode& msg(CoreWS::instance().messages);
        return msg.lookupError(ErrorBlock_PrintRectanglesMiss).toKernelOSError();
    }

    MyError err = PDriver::getRectangle(*rect, copiesRemaining, rectId);
    if (err)
        return err.toKernelOSError();

    debugLog("getrectangle returning copies remaining:%x", copiesRemaining);

    regs[0] = copiesRemaining;
    regs[2] = rectId;

    return nullptr;
}

_kernel_oserror* PDriver_CancelJob_Handler(int number,
                                           _kernel_swi_regs* r,
                                           void* pw)
{
    OS::KernelRegsWrapper regs(r);

    debugLog("canceljob %u", regs[0]);

    return PDriver::cancelJob(regs).toKernelOSError();
}

_kernel_oserror* PDriver_ScreenDump_Handler(int number,
                                            _kernel_swi_regs* r,
                                            void* pw)
{
    OS::KernelRegsWrapper regs(r);

    debugLog("screendump r0:%u", regs[0]);
    return PDriver::screenDump(regs).toKernelOSError();
}

_kernel_oserror* PDriver_DeclareFont_Handler(int number,
                                             _kernel_swi_regs* r,
                                             void* pw)
{
    OS::KernelRegsWrapper regs(r);

    debugLog("declarefont handle:%u name:%s flags:%u", regs[0], regs.as<const char*>(1), regs[2]);

    uint32_t fontHandle = regs[0];
    const char* name = regs.as<const char*>(1);
    uint32_t flags = regs[2];

    return PDriver::declareFont(fontHandle, name, flags).toKernelOSError();
}

static FontBlockList& miscop_fontList(CoreWS& ws)
{
    if (ws.currentJob() != nullptr)
        return ws.currentJob()->jobfontlist;

    return ws.fontlist;
}

_kernel_oserror* PDriver_SetPrinter_Handler(int number,
                                            _kernel_swi_regs* r,
                                            void* pw)
{
    debugLog("setprinter");

    return configure_setprinter(CoreWS::instance()).toKernelOSError();
}

_kernel_oserror* PDriver_CancelJobWithError_Handler(int number,
                                                    _kernel_swi_regs* r,
                                                    void *pw)
{
    OS::KernelRegsWrapper regs(r);

    debugLog("canceljobwitherror file:%u err:%p",
             regs[0],
             regs.as<void*>(1));

    FileHandle toCancel = regs[0];
    MyError withError(regs.as<_kernel_oserror*>(1));
    return PDriver::cancelJobWithError(toCancel, withError).toKernelOSError();
}

_kernel_oserror* PDriver_InsertIllustration_Handler(int number,
                                                    _kernel_swi_regs* r,
                                                    void* pw)
{
    OS::KernelRegsWrapper regs(r);

    debugLog("insertillustration file:%u", regs[0]);
    FileHandle handle = regs[0];
    const Draw::Path* clipPath = regs.as<const Draw::Path*>(1);
    const Point<OS::Unit> bottomLeft = Point<OS::Unit>(regs[2], regs[3]);
    const Point<OS::Unit> bottomRight = Point<OS::Unit>(regs[4], regs[5]);
    const Point<OS::Unit> topLeft = Point<OS::Unit>(regs[6], regs[7]);

    return PDriver::insertIllustration(handle, clipPath,
                                       bottomLeft, bottomRight, topLeft).toKernelOSError();
}

_kernel_oserror* PDriver_SetDriver_Handler(int number,
                                           _kernel_swi_regs* r,
                                           void* pw)
{
    OS::KernelRegsWrapper regs(r);

    debugLog("setdriver %u %u %u %u %u",
             regs[1], regs[2], regs[3], regs[4], regs[5]);
    (void)number;

    return PDriver::setDriver(regs).toKernelOSError();
}

// ----------------------------------------------------------------------------
//
//  SWI PDriver_MiscOp implementation
//
//    Entry: R0  = reason code
//                        0 => add font
//                        1 => remove fonts
//                        2 => enumerate fonts
//
//    Exit:  V set, r0 -> error block.
//           V clear, depends on reason code.
//
//  This code allows the handling of the MiscOp operations supported by the printer
//  drivers.  All reason codes with bit 31 clear are processed by the printer independant
//  code, others are passed to the printer dependant code to be processed.
//
//  When a job is selected then these functions take effect on that job, otherwise
//  they effect the current global settings.
//
// ----------------------------------------------------------------------------
_kernel_oserror* PDriver_MiscOp_Handler(int, _kernel_swi_regs* r, void* pw)
{
    CoreWS& ws = CoreWS::instance();
    OS::KernelRegsWrapper regs(r);

    uint32_t reason = regs[0];
    debugLog("miscop op:0x%x", reason);

    // If bit 31 set then pass to device specific code
    if ((reason & TopBit) != 0u)
        return miscop_decode(reason, regs, ws).toKernelOSError();

    MyError err;

    switch (reason) {
    case PDriverMiscOp_AddFont:
        err = miscop_addfont(regs, ws);
        break;

    case PDriverMiscOp_RemoveFonts:
        err = miscop_removefonts(ws);
        break;

    case PDriverMiscOp_ListFonts:
        err = miscop_enumfonts(regs, ws);
        break;

    default:
        err = ws.messages.lookupError(ErrorBlock_PrintBadMiscOp);
        break;
    }

    return err.toKernelOSError();
}

// cmhg entry point.
_kernel_oserror* PDriver_JPEGSWI_Handler(int, _kernel_swi_regs* r, void* pw)
{
    CoreWS& ws = CoreWS::instance();
    OS::KernelRegsWrapper regs(r);

    debugLog("jpegswi reason:%u", regs[8]);

    CoreJobWS& job = *ws.currentJob();

    InterceptJPEG intercept(ws, job);

    return intercept.intercept(regs).toKernelOSError();
}

// cmhg entry point.
_kernel_oserror* PDriver_FontSWI_Handler(int, _kernel_swi_regs* r, void* pw)
{
    OS::KernelRegsWrapper regs(r);

    CoreWS& ws = CoreWS::instance();
    debugLog("fontswi reason:%u r0:%u", regs[8], regs[0]);

    _kernel_oserror* e = InterceptFont::intercept(regs, ws).toKernelOSError();

    debugLog("fontswi %u returning... err=%s %p", regs[8], e ? e->errmess : "<null>", e);
    return e;
}

// Vector entry points ---------------------------------------------------------
// cmhg entry point.
_kernel_oserror* byte_vector_handler(_kernel_swi_regs* r, void* pw,
                                     vectortrap_f trap, void* trappw)
{
    OS::KernelRegsWrapper regs(r);

    InterceptOSByte handler;

    return handler.intercept(regs, trap, trappw).toCMHGTrap();
}

// cmhg entry point.
int colour_vector_handler(_kernel_swi_regs* r, void* pw)
{
    OS::KernelRegsWrapper regs(r);

    return InterceptColTrans::intercept(regs, CoreWS::instance()).toCMHG();
}

// cmhg entry point.
int draw_vector_handler(_kernel_swi_regs* r, void* pw)
{
    OS::KernelRegsWrapper regs(r);

    debugLog("draw_vector_handler");

    return InterceptDraw::intercept(regs, CoreWS::instance()).toCMHG();
}

// cmhg entry point.
int sprite_vector_handler(_kernel_swi_regs* r, void* pw)
{
    OS::KernelRegsWrapper regs(r);

    return InterceptSprite::intercept(regs, CoreWS::instance()).toCMHG();
}
