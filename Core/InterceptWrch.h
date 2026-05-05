#ifndef CORE_WRCH_H
#define CORE_WRCH_H

#include "InterceptManager.h"

#include "RLib/OS/Vector.h"

#include <stdint.h>

class CoreJobWS;
class CoreWS;

class InterceptWrch : public InterceptorBase {
public:
    static VectorReturn handler(OS::Regs& regs, CoreWS& ws);

private:
    InterceptWrch(CoreWS& ws, CoreJobWS& job, OS::Regs& regs)
        : m_ws(ws), m_job(job), m_regs(regs)
    { }

    // Continuation of `interceptwrch`, but as a class member.
    VectorReturn handler();

    VectorReturn finish(VectorReturn ret);
    VectorReturn passDownChar(uint8_t ch);
    VectorReturn passDownString(const uint8_t* str, uint32_t length);
    VectorReturn passControlSeq(uint8_t initial);
    VectorReturn controlSequence(uint8_t initial);
    MyError noPrinterControl();
    MyError noVDU4Chars();
    MyError clearBox();
    MyError noModeChanges();
    MyError cannotHandleVDU23();
    MyError deleteChar();
    MyError setDotPattern();
    MyError setCursorControl();
    MyError setFGTint();
    MyError setBGTint();
    MyError changeCharSize();
    VectorReturn vdu23_17();
    VectorReturn vdu23();
    MyError setClipBox();
    MyError plot();

private:
    CoreWS&     m_ws;
    CoreJobWS&  m_job;

    OS::Regs&   m_regs;
};


/* Deal with a VDU 26 sequence (set default box and cursors). */
MyError wrch_defaultbox(CoreJobWS& job);

namespace riscos { namespace Sprite { class Selector; } }

// Deal with OS_SpriteOp reason code &18 (24) (select sprite for VDU code).
// https://www.riscosopen.org/wiki/documentation/show/OS_SpriteOp%2024
//
// Sprite Area is always System Sprite area.
MyError wrch_selectsprite(const Sprite::Selector& sprite, CoreJobWS& job);

#endif
