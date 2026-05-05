#ifndef CORE_FONTSWI_H
#define CORE_FONTSWI_H

#include "FontPaint.h"
#include "InterceptManager.h"

class CoreJobWS;
class CoreWS;
class FontPaintStream;

class InterceptFont : public InterceptorBase {
public:
    static MyError intercept(OS::Regs& regs, CoreWS& ws);

private:
    InterceptFont(OS::Regs& regs, CoreWS& ws, CoreJobWS& job)
        : m_regs(regs), m_ws(ws), m_job(job)
    { }

    MyError paint();
    MyError paint(FontHandle font,
                  const uint8_t* string,
                  FontPaintFlag flags,
                  FontPoint position,
                  const FontPaintCoords* coordBlock,
                  const int32_t* matrix,
                  uint32_t length);
    MyError setFontColours(int32_t bg,
                           int32_t fg,
                           int32_t offset);
    MyError setPaletteSWI(int32_t bgIndex,
                          int32_t fgIndex,
                          int32_t offset,
                          uint32_t bgRgb,
                          uint32_t fgRgb);

    static MyError loseFont(FontHandle font, CoreWS& ws);

    MyError setColours(uint32_t bg,
                       uint32_t fg,
                       uint32_t offset);
    MyError setPalette(uint32_t bgIndex,
                       uint32_t fgIndex,
                       int32_t offset,
                       uint32_t bgRgb,
                       uint32_t fgRgb);
    MyError paintNextPosition(const uint8_t* chunk,
                              uint32_t length);
    MyError paintPrintable(FontPaintStream& stream,
                           bool isUtf8,
                           uint32_t pass);
    MyError paintNoCoordsBlock(FontPaintFlag flags,
                               OS::Millipoint x);
    MyError finish(MyError err);
    MyError finishPaint(MyError err);

private:
    OS::Regs&   m_regs;
    CoreWS&     m_ws;
    CoreJobWS&  m_job;
};

#endif
