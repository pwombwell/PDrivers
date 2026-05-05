#ifndef CORE_WORKSPACE_H
#define CORE_WORKSPACE_H

#include "Constants.h"
#include "EscapeState.h"
#include "FontBlock.h"
#include "FontPaintState.h"
#include "InterceptManager.h"
#include "Module.h"
#include "JobManager.h"
#include "MsgCode.h"
#include "OS.h"
#include "PDriverInfo.h"
#include "SpriteMonitor.h"

#include "RLibX/MessageTrans.h"

class CoreJobWS;
class JobWS;
struct UserRectangle;

class PDriverWS;

class CoreWS : public Module {
public:
    virtual ~CoreWS();

    static CoreWS& instance() { return *(CoreWS*)Module::instance(); }

    SpriteMonitor& spriteMonitor() { return m_spriteMonitor; }
    const SpriteMonitor& spriteMonitor() const { return m_spriteMonitor; }

    bool isCountingPass() const { return m_countingPass; }

private:
    // Module methods ---------------------------------------------------------
    // Called from Module_Initialise after create().
    virtual MyError initialise();

    // Called from Module_Finalise to determine if the module should finalise.
    // It refuses if there is an existing print job.
    virtual MyError preFinalise() /*override*/;

    virtual void handleServiceCall(uint32_t service, OS::Regs& regs);

private:
    SpriteMonitor m_spriteMonitor;

public:
    JobManager& jobMgr() { return m_jobManager; }
    const JobManager& jobMgr() const { return m_jobManager; }
    CoreJobWS* currentJob() { return m_jobManager.currentRef(); }
    CoreJobWS* currentJob() const { return m_jobManager.current(); }

    InterceptManager& interceptMgr() { return m_interceptMgr; }

    PDriverWS& toPDriverWS();
    const PDriverWS& toPDriverWS() const;

    /* current print job handle/workspace and list head */

    InterceptManager m_interceptMgr;

    MsgCode messages;

    // Global PDriver_Info.
    // Don't bother storing the 'length of name doofer' as it's faff.
    PDriverInfo globalInfo;

    /* Global PDriver_PageSize (keep adjacent, in order). */
    PDriverPageSize pageSize;

    /* Workspace used by PDriver_MiscOp to allocate records for fonts. */
    FontBlockList fontlist;

    /* Font_Paint intercept workspace. Coordinates are millipoints. */
    FontPaintState m_fontPaint;

    /* ColourTrans workspace */
    uint32_t ctrans_selgentab_R5;  // Flags. R5 for _SelectTable or _GenerateTable

    /* JPEG intercept workspace */
    bool jpeg_ctransflag;    // limits scope of ColourTrans intercept for JPEGs.
    uint32_t jpeg_maxmemory; // max memory SpriteExtend requires for any JPEGs.

    // Counting pass. True if asking for rectangles only to count such things
    // as JPEG memory requirements. I can't find this documented anywhere.
    bool m_countingPass;
    UserRectangle* counting_nextrect;

    /* mjs fudge */
    uint8_t pdriver_dp_max_mem_buff[32];

    /* Escape handling */
    EscapeState m_escapeState;

    FontPaintState& fontPaint() { return m_fontPaint; }
    const FontPaintState& fontPaint() const { return m_fontPaint; }

protected:
    CoreWS(void* pw)
        : Module(pw)
        , m_interceptMgr(*this)
        , m_escapeState(m_interceptMgr, messages)
        , m_jobManager(*this)
        , ctrans_selgentab_R5(0)
        , jpeg_ctransflag(false)
        , jpeg_maxmemory(0)
        , m_countingPass(false)
        , counting_nextrect(nullptr)
    {}

private:
    CoreWS(const CoreWS&);
    CoreWS& operator=(const CoreWS&);

    JobManager m_jobManager;
};

// Include from the implementation's directory.
#include "GlobalWS.h"

#endif
