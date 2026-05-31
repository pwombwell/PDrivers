#include "Core/PDriver.h"

#include "GlobalWS.h"
#include "PDriverDP.h"
#include "Private.h"

#include "Core/Job.h"
#include "Core/MsgCode.h"
#include "Core/OS.h"
#include "Core/Workspace.h"

#include "RLib/Constants/Service.h"
#include "RLib/OS/Env.h"
#include "RLib/OS/Error.h"

#include <string.h>

#include "cmhg.h"
#include "swis.h"

_kernel_oserror* config_callback_handler(_kernel_swi_regs *r, void *pw)
{
    _swix(OS_ServiceCall, _IN(1), Service_PDumperStarting);

    return nullptr;
}

static const uint8_t setdriver_table[] = {
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 48, 255
};

static const char title_string[] = "PDriverDP";

MyError configure_init(CoreWS& ws)
{
    PDriverWS *dp_ws = (PDriverWS *)&ws;

    dp_ws->printer_pdumper_pointer = nullptr;
    dp_ws->printer_stringblocksize = 0;
    dp_ws->pending_info_flag = 0;
    dp_ws->pending_pdumper_command[0] = 0;
    dp_ws->printer_pdumper_number = -1;

    // mis-use private word to store the service call to be sent.
    (void)OS::addCallback(config_callback, (void*)Service_PDumperStarting);

    const uint32_t pixelResX = 120;
    const uint32_t pixelResY = 180;
    const PDriverInfo::Features features = PDriverInfo::Plotting_TransformedSprites |
                                           PDriverInfo::Plotting_TransformedFonts |
                                           PDriverInfo::Plotting_DrawPageFlags |
                                           PDriverInfo::Features_MiscOp |
                                           PDriverInfo::Features_SetDevice;
    const char* printerName = nullptr;
    const uint32_t htoneResX = 120;
    const uint32_t htoneResY = 180;
    const uint32_t printer = 0;
    ws.globalInfo = SetPDriverInfo(pixelResX, pixelResY, features, printerName,
                                   htoneResX, htoneResY, printer);

    ws.pageSize.size.width = 594960;
    ws.pageSize.size.height = 841920;
    ws.pageSize.rect.x0 = 17280;
    ws.pageSize.rect.y0 = 72000 / 2;
    ws.pageSize.rect.x1 = 577680;
    ws.pageSize.rect.y1 = 841920 - 36000;

    dp_ws->printer_dump_depth = 24;
    dp_ws->printer_interlace = 0;
    dp_ws->printer_x_interlace = 0;
    dp_ws->printer_passes_per_line = 255;

    return nullptr;
}

void configure_finalise(CoreWS& ws)
{
    _swix(OS_ServiceCall, _IN(1), Service_PDumperDying);
}

MyError configure_vetinfo(SetPDriverInfo& info)
{
#if !Libra1
    PDriverInfo::Features features = info.features;

    if (features.isColour()) {
        return PDriverWS::instance().messages.lookupError(ErrorBlock_PrintNoColour, title_string);
    }
#else
    UNUSED(info);
#endif

    return nullptr;
}

MyError configure_makeerror(PDriverInfo::Features mask,
                            PDriverInfo::Features value,
                            PDriverInfo::Features features,
                            CoreWS& ws)
{
    UNUSED(mask);
    uint32_t mismatch = value ^ features;

    // Ignore colour restrictions and plotting capability mismatches.
    mismatch &= ~0x6; // don't really have good names for these bitfields
    mismatch &= ~(PDriverInfo::Plotting_FilledShapes |
                  PDriverInfo::Plotting_ThickLines |
                  PDriverInfo::Plotting_OverwritingPossible);

    if (mismatch == 0)
        return nullptr;

    // `error_table`
    if (!!(mismatch & PDriverInfo::Colour_Colour)) // bit 0
        return ws.messages.lookupError(ErrorBlock_PrintNoColour, title_string);

    if (!!(mismatch & PDriverInfo::Features_ScreenDumps)) // bit 24
        return ws.messages.lookupError(ErrorBlock_PrintNoScreenDump, title_string);

    if (!!(mismatch & PDriverInfo::Features_ArbitraryTransforms)) // bit 25
        return ws.messages.lookupError(ErrorBlock_PrintBadTransform, title_string);

    if (!!(mismatch & PDriverInfo::Features_InsertIllustration)) // bit 26
        return ws.messages.lookupError(ErrorBlock_PrintNoIncludedFiles, title_string);

    // Everything else.
    return ws.messages.lookupError(ErrorBlock_PrintBadFeatures, title_string);
}

MyError configure_setprinter(CoreWS& ws)
{
    return ws.messages.lookupError(ErrorBlock_PrintBadSetPrinter);
}

MyError configure_setdriver(const OS::Regs& regs, CoreWS& ws)
{
    PDriverWS* dp_ws = (PDriverWS *)&ws;

    uint32_t pdumper_number = regs[1];
    const char* command = regs.as<const char *>(2);
    const uint8_t* pdumper_data = regs.as<const uint8_t*>(3);
    const uint8_t* config_block = regs.as<const uint8_t*>(4);

    dp_ws->printer_configureword = regs[5];

    if (config_block)
        memcpy(&dp_ws->printer_dump_depth, config_block, 256);

    // Accumulate extended dump string lengths.
    uint8_t* base = &dp_ws->printer_dump_depth;
    uint32_t total_len = 0;
    for (size_t i = 0; i < sizeof(setdriver_table); ++i) {
        uint8_t offset = setdriver_table[i];
        if (offset == 255)
            break;

        uint8_t entry = base[offset];
        if (entry == 0)
            continue;

        uint32_t str_off = (uint32_t)entry + dp_data_dlm;
        if (base[str_off] != 0)
            continue;

        uint32_t len3 = (*(uint32_t *)(base + str_off)) >> 8;
        total_len += len3;
    }

    if (total_len != 0) {
        if (total_len > dp_ws->printer_stringblocksize) {
            if (dp_ws->printer_stringblocksize != 0)
                (void)rma_free(dp_ws->printer_stringblockptr);

            void *block;
            MyError err = rma_claim(total_len, block);
            if (err)
                return err;

            dp_ws->printer_stringblockptr = block;
            dp_ws->printer_stringblocksize = total_len;
        }

        uint8_t* dest = (uint8_t*)dp_ws->printer_stringblockptr;
        for (size_t i = 0; i < sizeof(setdriver_table); ++i) {
            uint8_t offset = setdriver_table[i];
            if (offset == 255)
                break;

            uint8_t entry = base[offset];
            if (entry == 0)
                continue;

            uint32_t str_off = (uint32_t)entry + dp_data_dlm;
            if (base[str_off] != 0)
                continue;

            uint32_t word = *(uint32_t *)(base + str_off);
            uint32_t len3 = word >> 8;
            uint32_t *src_ptr = (uint32_t *)(base + str_off + 4);
            const uint8_t *src = (const uint8_t *)(uintptr_t)(*src_ptr);
            *src_ptr = (uint32_t)(uintptr_t)dest;
            if (src && len3) {
                memcpy(dest, src, len3);
                dest += len3;
            }
        }
    }

    // Look for the requested PDumper in the list.
    PDumper* pdumper = dp_ws->pdumper_list.head();
    while (pdumper) {
        if (pdumper->number == pdumper_number) {
            break;
        }
        pdumper = pdumper->next();
    }

    if (pdumper) {
        dp_ws->printer_pdumper_number = pdumper_number;
        dp_ws->printer_pdumper_pointer = pdumper;

        PDumperCall call = {0};
        call.r1 = pdumper_number;
        call.r2 = (uint32_t)(uintptr_t)command;
        call.r3 = (uint32_t)(uintptr_t)pdumper_data;
        call.r4 = (uint32_t)(uintptr_t)config_block;
        call.r5 = dp_ws->printer_configureword;

        MyError err = CallPDumper(ws, PDumperReason_SetDriver, call);
        dp_ws->pending_info_flag = 0;
        return err;
    }

    dp_ws->printer_pdumper_number = pdumper_number;
    dp_ws->printer_pdumper_pointer = nullptr;
    dp_ws->pending_info_flag = (uint32_t)-1;

    if (command) {
        size_t i = 0;
        for (; i < sizeof(dp_ws->pending_pdumper_command); ++i) {
            char c = command[i];
            if (c < 32 || c == '\0') {
                dp_ws->pending_pdumper_command[i] = 0;
                break;
            }
            dp_ws->pending_pdumper_command[i] = (uint8_t)c;
        }
        if (i >= sizeof(dp_ws->pending_pdumper_command)) {
            return ws.messages.lookupError(ErrorBlock_PrintOverflow);
        }
    } else {
        dp_ws->pending_pdumper_command[0] = 0;
    }

    if (pdumper_data) {
        memcpy(dp_ws->pending_info_data,
               pdumper_data,
               sizeof(dp_ws->pending_info_data));
    }

    return nullptr;
}
