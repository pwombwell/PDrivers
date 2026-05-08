#include "Core/PDriver.h"

#include "GlobalWS.h"

#include "Core/Device.h"
#include "Core/Job.h"
#include "Core/MsgCode.h"
#include "Core/OS.h"
#include "Core/Workspace.h"

#include "RLib/OS/Error.h"

#include <string.h>

extern const uint32_t VarType_String;

static const char prologue_name[] = "PDriver$PSprologue";
static const char prologue_val[] = "Printers:ps.PSfiles.PSprolog";
static const char prologue2_name[] = "PDriver$PSprologue2";
static const char prologue2_val[] = "Printers:ps.PSfiles.PSprolog2";
static const char epilogue_name[] = "PDriver$PSepilogue";
static const char epilogue_val[] = "Printers:ps.PSfiles.PSepilog";
static const char extra_name[] = "PDriver$PSextra";
static const char extra_val[] = "";

static const char title_string[] = "PDriverPS";

static MyError ensureSystemVariable(const char* name, const char*value)
{
    // Test if a var is set, if not, set it.
    uint32_t len = OS::readVarValSize(name, OS::VarType_String);

    // If not set, set it to the default.
    if (len == 0)
        return OS::setVarVal(name, value, strlen(value), OS::VarType_String);

    return nullptr;
}

MyError configure_init(CoreWS& ws)
{
    MyError err;
    PDriverWS* psWS = (PDriverWS *)&ws;

    // Set initial data for PDriver_Info and PDriver_PageSize calls.
    const uint32_t pixelResX = 300;
    const uint32_t pixelResY = 300;
    const PDriverInfo::Features features = PDriverInfo::Colour_Greys |
                                           PDriverInfo::Plotting_TransformedSprites |
                                           PDriverInfo::Plotting_TransformedFonts |
                                           PDriverInfo::Features_ArbitraryTransforms |
                                           PDriverInfo::Features_MiscOp |
                                           PDriverInfo::Features_SetDevice |
                                           PDriverInfo::Features_DeclareFont |
                                           PDriverInfo::Plotting_DrawPageFlags;
    const char* printerName = nullptr;
    const uint32_t htoneResX = 40;
    const uint32_t htoneResY = 40;
    const uint32_t printer = 0;
    ws.globalInfo = SetPDriverInfo(pixelResX, pixelResY, features, printerName,
                                   htoneResX, htoneResY, printer);

    debugLog("ps configure_init features:0x%x", ws.globalInfo.features());

    // Set up page size info.
    ws.pageSize.size = Font::Size(594960, 841920);
    ws.pageSize.rect = Font::Rect(17280, 22080, 577680, 819840);

    // Initialise to not adding CTRL-D's to the ends of jobs, to not doing
    // auto-accent generation, and to Level1.
    psWS->m_flags = PS::Document::Flag_None;
    psWS->m_level = PS::Document::Level_L1;

    /* Set up default values (if necessary) for the environment variables. */
    if ((err = ensureSystemVariable(prologue_name, prologue_val)) != nullptr)
        return err;
    if ((err = ensureSystemVariable(prologue2_name, prologue2_val)) != nullptr)
        return err;
    if ((err = ensureSystemVariable(epilogue_name, epilogue_val)) != nullptr)
        return err;
    if ((err = ensureSystemVariable(extra_name, extra_val)) != nullptr)
        return err;

    /* Initialise the temporary sprite area. */
    psWS->sprarea.area.view().initialiseCheekily(sizeof(psWS->sprarea));

    return nullptr;
}

void configure_finalise(CoreWS& ws)
{
    (void)ws;
    /* Debug close only in assembler builds; nothing to do here. */
}

MyError configure_setprinter(CoreWS& ws)
{
    return ws.messages.lookupError(ErrorBlock_PrintBadSetPrinter);
}

// Entry: R0: bit 0 set if CTRL-D is to be added to the end of the output.
//        R0: bit 1 set if a verbose prologue is required.
//        R0: bit 2 set if auto-accent generation is required
//        R0: bit 3-7 define target 'level'
//            currently, driver just checks bit 3; set if target is Level 2 (else Level 1)
MyError configure_setdriver(const OS::Regs& regs, CoreWS& ws)
{
    PDriverWS& psWS = *(PDriverWS *)&ws;

    PS::Document::Flag mask = PS::Document::Flag_UseCtrlD |
                              PS::Document::Flag_Verbose |
                              PS::Document::Flag_Accents;

    PS::Document::Flag flags = regs.as<PS::Document::Flag>(0);
    psWS.m_flags = flags & mask;

    psWS.m_level = (PS::Document::Level)(regs[0] >> 3);

    return nullptr;
}

MyError configure_vetinfo(SetPDriverInfo& info)
{
    // Nothing to vet.
    return nullptr;
}

MyError configure_makeerror(PDriverInfo::Features mask,
                            PDriverInfo::Features value,
                            PDriverInfo::Features features,
                            CoreWS& ws)
{
    PDriverInfo::Features mismatch = value ^ features;

    // If configured for colour, ignore colour request mismatch.
    if (!!(features & PDriverInfo::Colour_Colour))
        mismatch &= ~PDriverInfo::Colour_Colour;

    // Ignore colour restriction mismatches.
    mismatch &= ~(PDriverInfo::Features(6));

    // Ignore plot capability mismatches.
    PDriverInfo::Features ignore = PDriverInfo::Plotting_FilledShapes |
                                   PDriverInfo::Plotting_ThickLines |
                                   PDriverInfo::Plotting_OverwritingPossible;
    mismatch &= ~ignore;

    // Ignore transform ability mismatches.
    mismatch &= ~PDriverInfo::Features_ArbitraryTransforms;

    if (!mismatch)
        return nullptr;

    /* Match the assembler's conditional selection of the error token. */
    if (!!(mismatch & PDriverInfo::Colour_Colour))
        return ws.messages.lookupError(ErrorBlock_PrintColourNotConfig, title_string);

    if (!!(mismatch & PDriverInfo::Features_ScreenDumps))
        return ws.messages.lookupError(ErrorBlock_PrintNoScreenDump, title_string);

    if (!!(mismatch & PDriverInfo::Features_InsertIllustration))
        return ws.messages.lookupError(ErrorBlock_PrintNoIncludedFiles, title_string);

    return ws.messages.lookupError(ErrorBlock_PrintBadFeatures, title_string);
}
