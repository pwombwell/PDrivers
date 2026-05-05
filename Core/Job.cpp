#include "PDriver.h"
#include "Job.h"

#include "Workspace.h"

#include <string.h>

CoreJobWS::CoreJobWS(FileHandle file, bool illustration, CoreWS& ws)
    : jobhandle(file)
    , copiestodo(0)
    , usersbg(0)
    , jobspriteparams()
    , info(ws.globalInfo)
    , pageSize(ws.pageSize)
    , numberofpages(0)
    , disabled(Disabled_NoPage)
    , illustrationjob(illustration)
#if DoFontSpriteVdu
    , doingfontplot(false)
#endif
    , fontplotseqlen(0)
    , m_cursorControl(0)
    , dottedlength(0)
    , currentsprite()
    , fontcoloffset(0)
    , m_persistentError(false)
{
    jobspriteparams.setToScreen();

    memset(textbuffer, 0, sizeof(textbuffer));
    memset(fontpalette, 0xff, sizeof(fontpalette));
}

CoreJobWS::~CoreJobWS()
{
    clearRectangles();
}

void CoreJobWS::clearRectangles()
{
    UserRectangle* rect;

    while ((rect = rectlist.detachHead()) != nullptr)
        delete rect;
}

bool CoreJobWS::addNewUserRect(uint32_t id,
                               const Rect<OS::Unit>& box,
                               const Draw::DimensionlessTransform& transform,
                               const Point<OS::Millipoint>& bottomLeft,
                               uint32_t bgColour)
{
    UserRectangle* rect = new UserRectangle(id, box, transform,
                                            bottomLeft, bgColour);
    if (!rect)
        return false;

    rectlist.addTail(rect);

    return true;
}

// `defaultdotpattern`
void CoreJobWS::setDefaultDotPattern()
{
    memset(dottedpattern, 0xAA, sizeof(dottedpattern));
    dottedlength = sizeof(dottedpattern);
}

void CoreJobWS::setDotPattern(const uint8_t* pattern)
{
    memcpy(dottedpattern, pattern, sizeof(dottedpattern));
}
