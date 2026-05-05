#ifndef CORE_JOB_H
#define CORE_JOB_H

#include "Constants.h"
#include "Colour.h"
#include "FontBlock.h"
#include "PDriverInfo.h"
#include "ScreenVars.h"
#include "UserRectangle.h"
#include "VDU5.h"
#include "WrchState.h"

#include "RLib/Geometry.h"
#include "RLib/OS/Sprite.h"

#include "RLibX/Utils/DList.h"

class JobWS;
class CoreWS;

// Print job. Probably rename JobCore?
class CoreJobWS {
public:
    CoreJobWS(FileHandle file, bool illustration, CoreWS& ws);
    virtual ~CoreJobWS();

    JobWS& jobWS(); // Cast to subclass.

    bool hasPersistentError() const { return m_persistentError; }
    MyError getPersistentError() const { return checkPersistentError(); }

    // Subroutine to make a persistent error, normally adding a suffix.
    MyError makePersistentError(MyError srcError);

    MyError makePersistentError(OS::ErrorView token);

    // Subroutine to make a persistent error, not adding a suffix.
    MyError makePersistentNoSuffix(MyError srcError);

    // Subroutine to return a persistent error, if it exists.
    MyError checkPersistentError() const;

    void clearPersistentError() { m_persistentError = false; }

    void clearRectangles();

    bool addNewUserRect(uint32_t id,
                        const Rect<OS::Unit>& box,
                        const Draw::DimensionlessTransform& transform,
                        const Point<OS::Millipoint>& bottomLeft,
                        uint32_t bgColour);

    void setDotPattern(const uint8_t* pattern);
    void setDefaultDotPattern();

public:
    // Original ARM had union of textbuffer and errorbuffer, but
    // errorbuffer hasn't been used since pre-Medusa.
    char textbuffer[textbufferlen];

    // In the original ARM implementation, r11 actually points here, so
    // 'errorbuffer' is a -ve offset. Increases reach, I suppose.

    FileHandle jobhandle;
    Utils::DListHook<CoreJobWS> m_listHook; // `joblink`

    // The number of copies of the current page that remain to be done. When this
    // location is zero, it indicates that there is no current page.
    uint32_t copiestodo;
    Utils::List<UserRectangle> rectlist;

    // The background colour for the current rectangle.
    uint32_t usersbg;

    // The user's co-ordinates for the bottom left corner of the box currently
    // being plotted, and its size. Units are untransformed OS co-ordinates
    // (i.e. 1/180 inch if the transformation is the identity) in both cases.
    Offset<OS::Unit> usersoffset;
    Size<OS::Unit>   usersbox;

    // The user's transformation for the current rectangle, as supplied.
    // Dimensionless, fixed binary with 16 binary places.
    Draw::DimensionlessTransform userstransform;

    // Where the user wants the bottom left corner of the current rectangle to go.
    // Units are millipoints.
    Point<OS::Millipoint> usersbottomleft;

    // r0-r3 of internal OS_SpriteOp calls for interception checks.
    Sprite::VDUContext jobspriteparams;

    // PDriver_Info.
    PDriverInfo info;

    // PDriver_PageSize.
    PDriverPageSize pageSize;

    FontBlockList jobfontlist;

    uint32_t numberofpages;

    Offset<OS::Unit> origin;
    Point<OS::Unit> thispoint;
    Point<OS::Unit> oldpoint;
    Point<OS::Unit> olderpoint;
    Point<OS::Unit> oldestpoint;

    Rect<OS::Unit> graphicswindow;

    ScreenVars screenVars;

    uint8_t dottedpattern[8];

    VDU5 m_vdu5;

    WrchState m_wrch;

    Disabled disabled;
    bool illustrationjob;

#if DoFontSpriteVdu
    uint8_t doingfontplot;
#endif

    uint8_t fontplotseqlen;
    uint8_t m_cursorControl;
    uint8_t dottedlength;

    Sprite::Selector currentsprite;

    uint32_t fontcoloffset;

#if FontColourFixes
    int32_t fontpalette[32]; /* 2 * 16 words */
#else
    int32_t fontpalette[48]; /* 3 * 16 words */
#endif

    /* Printer-specific job workspace follows in memory. */

    VDU5& vdu5() { return m_vdu5; }
    const VDU5& vdu5() const { return m_vdu5; }

    WrchState& wrch() { return m_wrch; }
    const WrchState& wrch() const { return m_wrch; }

private:
    bool m_persistentError; // `persistenterror`
};

// Include from the implementation's directory.
#include "JobWS.h"

#endif
