#include "Core/PDriver.h"

#include "Colour.h"
#include "JobWS.h"
#include "PDriverDP.h"
#include "Private.h"

#include "Core/Constants.h"
#include "Core/Job.h"
#include "Core/MsgCode.h"
#include "Core/OS.h"
#include "Core/Workspace.h"

typedef DP::OutputState::RotationId RotationId;

// Ugly workaround for Norcroft not liking enums in classes (let alone enum classes).
static const RotationId RotationId_None = DP::OutputState::RotationId_None;
static const RotationId RotationId_SwappedAxes = DP::OutputState::RotationId_SwappedAxes;
static const RotationId RotationId_NegativeXScale = DP::OutputState::RotationId_NegativeXScale;
static const RotationId RotationId_NegativeYScale = DP::OutputState::RotationId_NegativeYScale;
static const RotationId RotationId_Rotate90 = DP::OutputState::RotationId_Rotate90;
static const RotationId RotationId_Rotate180 = DP::OutputState::RotationId_Rotate180;
static const RotationId RotationId_Rotate270 = DP::OutputState::RotationId_Rotate270;

extern const uint32_t SpriteType_New16bpp;
extern const uint32_t SpriteType_New32bpp;

static const uint8_t setbcolourandECF[] = {5, 18, 64, 0, 23, 5};
static const uint8_t colour_backtint[] = {3, 23, 17, 3};
static const char kPDriverDPMaxMemName[] = "PDriverDP$MaxMem";

enum {
    kMaxSystemSpriteAreaBytes = 4 * 1024 * 1024,
    kMinWimpLeaveBytes = 32 * 1024,
    kMinRmaLeaveBytes = 4 * 1024
};

// 33/36 = vdu_counted_string_size Libra1 vs not Libra1
// `set_sprite_output_state`
static const uint8_t kSetSpriteOutputState[] = {
    26,                         // full sprite
#if Libra1
    // Get rid of VDU 18 to stop black border see set_sprite_background
#else
    18, 0, 128,
#endif
    23,17,3,0, 0,0,0,0,0,0,     // full white background
    16,                         // CLG
    5,                          // set VDU 5 mode
    23,16,0x41,0, 0,0,0,0,0,0,  // cursor actions
#ifdef MonoBufferOK
    23,17,4,1, 0,0,0,0,0,0      // Archie mode ECFs
#endif
};

// `setbackground_colour`
MyError setBackgroundColour(uint32_t bbGGRR00, JobWS& job)
{
#ifdef Libra1
    if (job.config().stripType() == 4 || job.config().stripType() == 5) {
        uint32_t pixval = 0;
        MyError err = colour_rgbto256colpixval(bbGGRR00, pixval, job);
        if (err != nullptr)
            return err;
        return XOS_SetColour(16, pixval); // set bit 4 - alter background
    }
#endif

#ifdef MonoBufferOK
    uint32_t pixval = 0;
    MyError err = colour_rgbto256colpixval(bbGGRR00, pixval, job);
    if (err != nullptr)
        return err;

#ifdef NbppBufferOK
    bool useECF = job.config().outputBpp() != 8; // `job_output_bpp`
#else
    bool useECF = job.config().job_use_1bpp() != 8; // `job_use_1bpp`
#endif
    if (useECF) {
        const uint8_t* table = nullptr;
        uint32_t index = pixval;

        if (job.output().halftoneX() == 4) {
            table = fourbyfourECFbytes;
#ifdef PDumperColours
            index = (pixval >> 8) & 0xFFu;
#endif
        } else {
            table = eightbyeightECFbytes;
#ifdef PDumperColours
            index = (pixval >> 16) & 0xFFu;
#endif
        }

        const uint8_t* entry = table + (index * 8u);
        err = vdu_counted_string(setbcolourandECF);
        if (err != nullptr)
            return err;

        uint32_t word0 = (uint32_t)entry[0] | ((uint32_t)entry[1] << 8) |
                         ((uint32_t)entry[2] << 16) | ((uint32_t)entry[3] << 24);
        uint32_t word1 = (uint32_t)entry[4] | ((uint32_t)entry[5] << 8) |
                         ((uint32_t)entry[6] << 16) | ((uint32_t)entry[7] << 24);

        err = vdu_pair(word0 & 0xFFFFu);
        if (err != nullptr)
            return err;
        err = vdu_pair(word0 >> 16);
        if (err != nullptr)
            return err;
        err = vdu_pair(word1 & 0xFFFFu);
        if (err != nullptr)
            return err;
        return vdu_pair(word1 >> 16);
    }
#else
    uint32_t pixval = colour_rgbtopixval(bbGGRR00, ws);
#endif

#ifdef Libra1
    return XOS_SetColour(16, pixval); // set bit 4 - alter background
#else
    uint32_t gcol = (pixval & 0x87u) |
                    ((pixval & 0x70u) >> 1) |
                    ((pixval & 0x08u) << 3);

    MyError err = vdu_pair(ws, 18);
    if (err != nullptr)
        return err;
    err = vdu_char(ws, (uint8_t)((gcol >> 2) | 0x80u));
    if (err != nullptr)
        return err;
    err = vdu_counted_string(ws, kColourBackTint);
    if (err != nullptr)
        return err;
    err = vdu_char(ws, (uint8_t)((gcol << 6) & 0xFFu));
    if (err != nullptr)
        return err;
    err = vdu_pair(ws, 0);
    if (err != nullptr)
        return err;
    err = vdu_pair(ws, 0);
    if (err != nullptr)
        return err;
    return vdu_pair(ws, 0);
#endif
}

// Helper for original buffer-size calculations before `buffer_area_set`.
static uint32_t calculatePixelMemory(uint32_t lineLength, uint32_t stripRows,
                                     const JobWS& job)
{
    uint32_t pixelBytes = lineLength;

#ifdef MonoBufferOK
#ifdef NbppBufferOK
    if (job.config().outputBpp() != 8) {
        pixelBytes += 7;
        pixelBytes >>= 3;
    }
#endif
#endif

    pixelBytes += 3;
    pixelBytes &= ~3u;
    pixelBytes *= stripRows;

#if Libra1
    if (job.config().stripType() == 4)
        pixelBytes <<= 1;
    else if (job.config().stripType() == 5)
        pixelBytes <<= 2;
#endif

    return pixelBytes;
}

static bool hasExtra32bppBuffer(const JobWS& job)
{
#if MultiplePasses
    return job.config().stripType() == 3;
#else
    (void)job;
    return false;
#endif
}

static bool usesPassBufferCycling(const JobWS& job)
{
#if MultiplePasses
    return job.config().numPasses() > 1;
#else
    (void)job;
    return false;
#endif
}

// Helper for original multiple-pass sprite count calculations before `buffer_area_set`.
static uint32_t createdOutputBufferCount(const JobWS& job)
{
    uint32_t count = job.config().numPasses();
    if (hasExtra32bppBuffer(job))
        count += 1;

    return count;
}

static uint32_t pixelEquivalentBufferCount(const JobWS& job)
{
    uint32_t count = job.config().numPasses();
    if (hasExtra32bppBuffer(job))
        count += 4;

    return count;
}

static uint32_t spriteHeaderCount(const JobWS& job)
{
    uint32_t count = createdOutputBufferCount(job);
    if (job.output().rotationId() != 0u)
        count += 1;

    return count;
}

// Helper for original sprite control-block size calculations before `buffer_area_set`.
static uint32_t spriteControlBytes(const JobWS& job)
{
    return sizeof(Sprite::Area) + (job.output().sprHeadSize() * spriteHeaderCount(job));
}

static uint32_t spritePaletteBytes(const JobWS& job)
{
#if Libra1
    if (job.config().stripType() == 4 || job.config().stripType() == 5)
        return 0;
#endif

    return 8u << job.config().outputBpp();
}

// Helper for original sprite mode selection used by `create_buffer_sprite`.
static OS::Mode outputBufferMode(const JobWS& job)
{
#if Libra1
    if (job.config().stripType() == 4 || job.config().stripType() == 5) {
        uint32_t modeWord;
        if (job.config().stripType() == 4)
            modeWord = SpriteType_New16bpp << 27;
        else
            modeWord = SpriteType_New32bpp << 27;

        modeWord |= 90u << 14;
        modeWord |= 90u << 1;
        modeWord |= 1;
        return OS::Mode(modeWord);
    }
#endif

#ifdef MonoBufferOK
#ifdef NbppBufferOK
    return OS::Mode(job.config().outputBpp() == 8 ? 21 : 18);
#else
    return OS::Mode(job.config().use1bpp() == 0 ? 21 : 18);
#endif
#else
    return OS::Mode(21);
#endif
}

static uint32_t currentStripRows(const JobWS& job)
{
    return job.output().clipSize().height.raw() >> bufferpix_l2size;
}

static void setBufferName(DP::BufferSprite& buffer, uint32_t index)
{
    char* name = buffer.nameBuffer();
    for (uint32_t i = 0; i < 12; ++i)
        name[i] = '0';
    name[12] = '\0';

    uint32_t pos = 12;
    do {
        --pos;
        name[pos] = (char)('0' + (index % 10));
        index /= 10;
    } while (index != 0 && pos > 0);
}

static MyError deleteBufferSprite(JobWS& job,
                                           DP::BufferSprite& buffer,
                                           CoreWS& ws)
{
    if (!buffer.hasName())
        return nullptr;

    ScopedPassthrough scopedPT(ws.m_interceptMgr, Passthrough_Sprite);
    MyError err = Sprite::deleteSprite(buffer.selector(job.output().spriteArea()));
    buffer.reset();
    return err;
}

static MyError releaseBufferState(JobWS& job, CoreWS& ws)
{
    MyError firstErr = restore_output_state(job);

    for (uint32_t i = 1; i <= createdOutputBufferCount(job); ++i) {
        MyError err = deleteBufferSprite(job, job.output().lineBuffer(i), ws);
        if (firstErr == nullptr && err != nullptr)
            firstErr = err;
    }
    job.output().lineBuffer(0).reset();

    {
        MyError err = deleteBufferSprite(job, job.output().rotationBuffer(), ws);
        if (firstErr == nullptr && err != nullptr)
            firstErr = err;
    }

    job.output().currentBuffer() = nullptr;

    if (job.output().vduSaveArea() != nullptr) {
        MyError err = rma_free(job.output().vduSaveArea());
        if (firstErr == nullptr && err != nullptr)
            firstErr = err;
        job.output().vduSaveArea() = nullptr;
    }

    if (job.output().ownsSpriteAreaAllocation()) {
        MyError err = rma_free(job.output().spriteArea());
        if (firstErr == nullptr && err != nullptr)
            firstErr = err;
    }

    if (job.output().sAreaChange() != 0u) {
        (void)XOS_ChangeDynamicArea(3, -(int32_t)job.output().sAreaChange());
        job.output().sAreaChange() = 0;
    }

    job.output().markSpriteAreaUnavailable();
    job.output().savedVduState().unset();
    job.output().setBufferMarked(false);
    job.output().setDumpPassImageCount(0);
    for (uint32_t i = 0; i < max_passes + 1; ++i)
        job.output().dumpPassImages()[i] = nullptr;

    return firstErr;
}

// `create_buffer_sprite`
static MyError createBufferSprite(JobWS& job,
                                           DP::BufferSprite& buffer,
                                           uint32_t index,
                                           uint32_t widthPixels,
                                           uint32_t heightPixels,
                                           CoreWS& ws)
{
    setBufferName(buffer, index);

    ScopedPassthrough scopedPT(ws.m_interceptMgr, Passthrough_Sprite);
    return Sprite::create(buffer.selector(job.output().spriteArea()),
                          spritePaletteBytes(job) != 0,
                          (int)widthPixels,
                          (int)heightPixels,
                          outputBufferMode(job));
}

// Tail of `buffer_area_set`: redirect output and emit `set_sprite_output_state`.
static MyError prepareCurrentBuffer(JobWS& job, CoreWS& ws)
{
    MyError err = redirect_output(job, ws);
    if (err != nullptr)
        return err;

#if Libra1
    if ((err = setBackgroundColour(0xffffff00u, job)) != nullptr)
        return err;
#endif

    if ((err = vdu_counted_string(kSetSpriteOutputState)) != nullptr)
        return err;

    job.disabled |= Disabled_NullClip;
    job.output().setBufferMarked(false);
    return nullptr;
}

static uint32_t scaledMpToPixels(int32_t valueMp, uint32_t dpi)
{
    if (valueMp <= 0 || dpi == 0u)
        return 0;

    int32_t product = valueMp * (int32_t)dpi;
    return (uint32_t)Divide(product, 72000);
}

static uint32_t scaledMpToPixels(OS::Millipoint valueMp, uint32_t dpi)
{
    return scaledMpToPixels(valueMp.raw(), dpi);
}

static Rect<OS::Millipoint> rectFromCorners(const Point<OS::Millipoint>& a,
                                            const Point<OS::Millipoint>& b)
{
    Rect<OS::Millipoint> rect(a, a);
    rect.unionWith(Rect<OS::Millipoint>(b, b));
    return rect;
}

static bool rectTransformState(const UserRectangle& rect,
                               RotationId& rotationId,
                               int32_t& xScale,
                               int32_t& yScale)
{
    int32_t xx = rect.recttransform.a.raw();
    int32_t yx = rect.recttransform.b.raw();
    int32_t xy = rect.recttransform.c.raw();
    int32_t yy = rect.recttransform.d.raw();

    bool diagonalZero = (xx == 0 && yy == 0);
    bool offDiagonalZero = (yx == 0 && xy == 0);
    if (diagonalZero == offDiagonalZero)
        return false;

    if (diagonalZero) {
        if (yx == 0 || xy == 0)
            return false;

        rotationId = RotationId_Rotate90;
        xScale = yx;
        yScale = -xy;
    } else {
        if (xx == 0 || yy == 0)
            return false;

        rotationId = RotationId_None;
        xScale = xx;
        yScale = yy;
    }

    if (xScale < 0) {
        // rotationId |= RotationId_NegativeXScale; // (norcroft)
        rotationId = RotationId(rotationId | RotationId_NegativeXScale);
        xScale = -xScale;
    }
    if (yScale < 0) {
        // rotationId |= RotationId_NegativeYScale; // (norcroft)
        rotationId = RotationId(rotationId | RotationId_NegativeYScale);
        yScale = -yScale;
    }

    return true;
}

static MyError computePrintArea(JobWS& job, CoreWS& ws)
{
    Rect<OS::Millipoint>& area = job.output().printArea();
    area = Rect<OS::Millipoint>(OS::Millipoint(0x7FFFFFFF),
                                OS::Millipoint(0x7FFFFFFF),
                                OS::Millipoint((int32_t)0x80000000u),
                                OS::Millipoint((int32_t)0x80000000u));
    job.output().setRotationId(DP::OutputState::RotationId_None);

    // vet the rectangle list for bad rotations, figure out how wide a sprite
    // we need.
    for (UserRectangle* rect = job.rectlist.head(); rect != nullptr; rect = rect->next()) {
        RotationId rotationId = RotationId_None;
        int32_t xScale = 0;
        int32_t yScale = 0;
        if (!rectTransformState(*rect, rotationId, xScale, yScale))
            return ws.messages.lookupError(ErrorBlock_PrintBadTransform);

        if (!!(rotationId & DP::OutputState::RotationId_SwappedAxes))
            job.output().setRotationId(RotationId(RotationId_SwappedAxes |
                                       RotationId_NegativeXScale)); // norcroft

        OS::Millipoint transformedX, transformedY;
        transform_point(rect->recttransform,
                        rect->rectbox.width,
                        rect->rectbox.height,
                        transformedX,
                        transformedY);
        Point<OS::Millipoint> transformed(rect->rectbottomleft.x + transformedX,
                                          rect->rectbottomleft.y + transformedY);
        area.unionWith(rectFromCorners(rect->rectbottomleft, transformed));
    }

    if (area.isEmpty()) {
        area = Rect<OS::Millipoint>(job.pageSize.rect);
        return nullptr;
    }

    area.clipWith(job.pageSize.rect);

    return nullptr;
}

static void initialiseCurrentRectangle(JobWS& job)
{
    job.output().currentRectRef() = job.rectlist.head();
}

// `start_page`
static MyError startPage(int32_t copies, JobWS& job)
{
    PDumperCall call = {0};
    call.r0 = (uint32_t)copies;
    call.r1 = job.jobhandle;
    call.r2 = job.config().stripType();
    call.r3 = job.output().topMargin();
    call.r4 = (uint32_t)(uintptr_t)&job.config().pdumperWord();
    call.r5 = (uint32_t)(uintptr_t)job.config().configBytes();
    call.r6 = job.config().leftMargin();
    call.r7 = job.info.pixelResX() | (job.info.pixelResY() << 16);

    MyError err = CallPDumperForJob(job, PDumperReason_StartPage, call);
    if (err == nullptr)
        job.output().setCurrentLine(-(int32_t)job.output().topMargin());

    return err;
}

// `end_page`
static MyError endPage(uint32_t copiesIn,
                                uint32_t* copiesOut,
                                JobWS& job)
{
    PDumperCall call = {0};
    call.r0 = copiesIn;
    call.r1 = job.jobhandle;
    call.r2 = job.config().stripType();
    call.r3 = (uint32_t)(uintptr_t)&job.config().pdumperWord();
    call.r4 = (uint32_t)(uintptr_t)job.config().configBytes();

    MyError err = CallPDumperForJob(job, PDumperReason_EndPage, call);
    if (err != nullptr)
        return err;

    *copiesOut = copiesIn - 1;
    return nullptr;
}

// `pagebox_real_setup`
static MyError setupPageMetrics(JobWS& job, CoreWS& ws)
{
    MyError err = computePrintArea(job, ws);
    if (err != nullptr)
        return err;

    const Rect<OS::Millipoint>& printArea = job.output().printArea();
    OS::Millipoint heightMp = printArea.height();
    OS::Millipoint topBlankMp = job.pageSize.rect.y1 - printArea.y1;
    OS::Millipoint leftBlankMp = printArea.x0 - job.pageSize.rect.x0;
    OS::Millipoint widthMp = printArea.width();

    if (heightMp <= OS::Millipoint(0) || widthMp <= OS::Millipoint(0))
        return ws.messages.lookupError(ErrorBlock_PrintRectanglesMiss);

    job.output().setTotalHeight(scaledMpToPixels(heightMp, job.info.pixelResY()));
    job.output().setTopMargin(scaledMpToPixels(topBlankMp, job.info.pixelResY()));
    job.config().setLeftMargin(scaledMpToPixels(leftBlankMp, job.info.pixelResX()));
    job.output().setLineLength(scaledMpToPixels(widthMp, job.info.pixelResX()));

    return nullptr;
}

// Helper for original dump-depth calculations before `buffer_area_set`.
static uint32_t dumpDepthRows(const JobWS& job)
{
    uint32_t dumpDepth = job.config().dumpDepth();
    return dumpDepth != 0 ? dumpDepth : 1;
}

// Helper for original maximum-worthwhile strip calculation before `buffer_area_set`.
static uint32_t usefulStripGroups(const JobWS& job)
{
    int32_t strips = Divide((int32_t)job.output().totalHeight(),
                            (int32_t)dumpDepthRows(job));
    if (strips < 0)
        strips = 0;

    return (uint32_t)strips + 1u;
}

// Helper for original bytes-per-strip calculation before `buffer_area_set`.
static uint32_t bytesPerDumpGroup(const JobWS& job)
{
    uint32_t bytes = calculatePixelMemory(job.output().lineLength(),
                                          dumpDepthRows(job),
                                          job);

#if MultiplePasses
    bytes *= pixelEquivalentBufferCount(job);
#endif

    if (job.output().rotationId() != 0u)
        bytes += calculatePixelMemory(job.output().lineLength(),
                                      dumpDepthRows(job),
                                      job);

    return bytes;
}

static uint32_t maximumUsefulAreaBytes(const JobWS& job)
{
    uint32_t headerBytes = sizeof(Sprite::Area) +
                           (job.output().sprHeadSize() * (max_passes + 2));
    if (job.output().rotationId() != 0)
        headerBytes += job.output().sprHeadSize();

    return headerBytes + (bytesPerDumpGroup(job) * usefulStripGroups(job));
}

static uint32_t minimumAreaBytes(const JobWS& job, const CoreWS& ws)
{
    return spriteControlBytes(job) + bytesPerDumpGroup(job) + ws.jpeg_maxmemory;
}

static void applyConfiguredMaxMemoryLimit(uint32_t minimumBytes, uint32_t& maximumBytes)
{
    char buffer[32];
    int32_t used = sizeof(buffer) - 1;
    uint32_t maxMemKBytes = 0;

    int32_t context;
    OS::VarType type(OS::VarType_String);
    if (OS::xreadVarVal(kPDriverDPMaxMemName, buffer, used, 0, type, used, context, type) != nullptr)
        return;
    if (used <= 0 || used >= (int32_t)sizeof(buffer) - 1)
        return;

    buffer[used] = '\0';
    if (XOS_ReadUnsigned(10, buffer, maxMemKBytes) != nullptr)
        return;

    uint32_t maxMemBytes = maxMemKBytes << 10;
    if (maxMemBytes < minimumBytes)
        maxMemBytes = minimumBytes;
    if (maximumBytes > maxMemBytes)
        maximumBytes = maxMemBytes;
}

static bool tryExpandSystemSpriteArea(uint32_t minimumBytes,
                                      uint32_t maximumBytes,
                                      JobWS& job,
                                      CoreWS& ws,
                                      uint32_t& stripGroupsOut)
{
    uint32_t pageSize = 0;
    if (XOS_ReadMemMapInfo(pageSize) != nullptr)
        return false;

    uint32_t availableBytes = minimumBytes;
    uint32_t slot = 0;
    if (XWimp_ClaimFreeMemory(1, availableBytes, slot) != nullptr || slot == 0u)
        return false;
    (void)pageSize;
    (void)XWimp_ClaimFreeMemory(0, availableBytes, slot);

    uint32_t leaveFreeBytes = availableBytes >> 4;
    if (leaveFreeBytes < kMinWimpLeaveBytes)
        leaveFreeBytes = kMinWimpLeaveBytes;

    uint32_t claimBytes = availableBytes > leaveFreeBytes ? availableBytes - leaveFreeBytes : 0u;
    if (claimBytes > maximumBytes)
        claimBytes = maximumBytes;
    if (claimBytes < minimumBytes)
        claimBytes = minimumBytes;
    if (claimBytes <= ws.jpeg_maxmemory)
        return false;

    claimBytes -= ws.jpeg_maxmemory;

    size_t currentSize;
    if (Sprite::getSystemSize(currentSize) != nullptr)
        return false;

    if ((uint32_t)currentSize >= kMaxSystemSpriteAreaBytes)
        return false;
    if ((uint32_t)currentSize + claimBytes > kMaxSystemSpriteAreaBytes)
        claimBytes = kMaxSystemSpriteAreaBytes - (uint32_t)currentSize;
    if (claimBytes == 0u)
        return false;

    if (XOS_ChangeDynamicArea(3, (int32_t)claimBytes) != nullptr)
        return false;

    size_t grownSize;
    if (Sprite::getSystemSize(grownSize) != nullptr)
        return false;

    uint32_t actualChange = grownSize > currentSize ? (uint32_t)(grownSize - currentSize) : 0u;
    if (actualChange <= spriteControlBytes(job)) {
        if (actualChange != 0u)
            (void)XOS_ChangeDynamicArea(3, -(int32_t)actualChange);
        return false;
    }

    uint32_t pixelBytes = actualChange - spriteControlBytes(job);
    stripGroupsOut = pixelBytes / bytesPerDumpGroup(job);
    if (stripGroupsOut == 0u) {
        (void)XOS_ChangeDynamicArea(3, -(int32_t)actualChange);
        return false;
    }

    uint32_t usedPixelBytes = stripGroupsOut * bytesPerDumpGroup(job);
    uint32_t spareBytes = pixelBytes - usedPixelBytes;
    if (spareBytes != 0u) {
        (void)XOS_ChangeDynamicArea(3, -(int32_t)spareBytes);
        actualChange -= spareBytes;
    }

    job.output().spriteAreaRef() = nullptr;
    job.output().sAreaChange() = actualChange;
    return true;
}

static MyError claimPrivateSpriteArea(uint32_t maximumBytes,
                                               JobWS& job,
                                               CoreWS& ws,
                                               uint32_t& stripGroupsOut)
{
    uint32_t largestFree = 0;
    uint32_t totalFree = 0;
    MyError err = XOS_ModuleRMADesc(largestFree, totalFree);
    if (err != nullptr)
        return err;

    uint32_t leaveFreeBytes = totalFree >> 2;
    if (leaveFreeBytes < kMinRmaLeaveBytes)
        leaveFreeBytes = kMinRmaLeaveBytes;

    uint32_t claimableBytes = totalFree > leaveFreeBytes ? totalFree - leaveFreeBytes : 0u;
    if (largestFree > claimableBytes)
        largestFree = claimableBytes;

    if (largestFree > maximumBytes)
        largestFree = maximumBytes;
    if (largestFree <= spriteControlBytes(job) + ws.jpeg_maxmemory)
        return ws.messages.lookupError(ErrorBlock_PrintNoFreeMemory);

    uint32_t pixelBytes = largestFree - spriteControlBytes(job) - ws.jpeg_maxmemory;
    stripGroupsOut = pixelBytes / bytesPerDumpGroup(job);

    while (stripGroupsOut > 0u) {
        uint32_t areaBytes = spriteControlBytes(job) +
                             (stripGroupsOut * bytesPerDumpGroup(job));
        void* block = nullptr;
        err = rma_claim(areaBytes, block);
        if (err == nullptr) {
            Sprite::Area* area = (Sprite::Area*)block;
            area->view().initialiseCheekily(areaBytes);
            job.output().spriteAreaRef() = area;
            job.output().sAreaChange() = 0;
            return nullptr;
        }

        --stripGroupsOut;
    }

    return ws.messages.lookupError(ErrorBlock_PrintNoFreeMemory);
}

// `buffer_area_set`
// got number of dump depth height strips in buffer in R6: create sprites
static MyError createConfiguredBuffers(uint32_t stripGroups,
                                                JobWS& job,
                                                CoreWS& ws)
{
    uint32_t stripRows = stripGroups * dumpDepthRows(job);
    uint32_t lineLength = job.output().lineLength(); // pixels
    uint32_t nameIndex = 1;

    job.output().lineBuffer(0).reset();
    for (uint32_t i = 1; i < max_passes + 2; ++i)
        job.output().lineBuffer(i).reset();

    job.output().rotationBuffer().reset();
    job.output().setDumpPassImageCount(0);

    if (job.output().rotationId() != 0u) {
        MyError err = createBufferSprite(job, job.output().rotationBuffer(),
                                                  nameIndex++, stripRows, lineLength, ws);
        if (err != nullptr)
            return err;
    }

    for (uint32_t pass = 1; pass <= job.config().numPasses(); ++pass) {
        MyError err = createBufferSprite(job, job.output().lineBuffer(pass),
                                                  nameIndex++, lineLength, stripRows, ws);
        if (err != nullptr)
            return err;
    }

    if (hasExtra32bppBuffer(job)) {
        MyError err = createBufferSprite(job,
                                         job.output().lineBuffer(job.config().numPasses() + 1u),
                                         nameIndex++, lineLength << 2, stripRows, ws);
        if (err != nullptr)
            return err;
    }

    job.config().setPass(1);
    job.output().lineBuffer(0).copyFrom(job.output().lineBuffer(1));
    job.output().currentBuffer() = &job.output().lineBuffer(0);
    job.output().setClipSize(Size<OS::Unit>(OS::Unit(lineLength << bufferpix_l2size),
                                              OS::Unit(stripRows << bufferpix_l2size)));
    return nullptr;
}

static MyError pageboxMemorySetup(int32_t copies, JobWS& job, CoreWS& ws)
{
    MyError err = releaseBufferState(job, ws);
    if (err != nullptr)
        return err;

    job.output().setSprHeadSize(sizeof(Sprite::Header) + spritePaletteBytes(job));
    job.output().setSprBlockSize(sizeof(Sprite::Area) + job.output().sprHeadSize());
    job.output().sAreaChange() = 0;

    uint32_t maximumBytes = maximumUsefulAreaBytes(job) + ws.jpeg_maxmemory;
    uint32_t minimumBytes = minimumAreaBytes(job, ws);
    applyConfiguredMaxMemoryLimit(minimumBytes, maximumBytes);

    uint32_t stripGroups = 0;
    if (!tryExpandSystemSpriteArea(minimumBytes, maximumBytes, job, ws, stripGroups)) {
        err = claimPrivateSpriteArea(maximumBytes, job, ws, stripGroups);
        if (err != nullptr)
            return err;
    }

    err = createConfiguredBuffers(stripGroups, job, ws);
    if (err != nullptr) {
        (void)releaseBufferState(job, ws);
        return err;
    }

    int saveAreaBytes = 0;
    {
        ScopedPassthrough scopedPT(ws.m_interceptMgr, Passthrough_Sprite);
        err = Sprite::readSaveAreaSize(job.output().lineBuffer(0).selector(job.output().spriteArea()),
                                       saveAreaBytes);
    }
    if (err != nullptr) {
        (void)releaseBufferState(job, ws);
        return err;
    }

    if (saveAreaBytes <= 0) {
        (void)releaseBufferState(job, ws);
        return ws.messages.lookupError(ErrorBlock_PrintNoFreeMemory);
    }

    void* saveBlock = nullptr;
    err = rma_claim((size_t)saveAreaBytes, saveBlock);
    if (err != nullptr) {
        (void)releaseBufferState(job, ws);
        return err;
    }

    job.output().vduSaveArea() = (Sprite::VDUSaveArea*)saveBlock;
    job.output().vduSaveArea()->initialise();

    initialiseCurrentRectangle(job);

    err = prepareCurrentBuffer(job, ws);
    if (err != nullptr) {
        (void)releaseBufferState(job, ws);
        return err;
    }

    err = startPage(copies, job);
    if (err != nullptr)
        (void)releaseBufferState(job, ws);
    return err;
}

static MyError resolveBufferImage(const JobWS& job,
                                           const DP::BufferSprite& buffer,
                                           uint8_t*& imageOut,
                                           uint32_t& rowBytesOut,
                                           uint32_t& rowsOut,
                                           CoreWS& ws)
{
    Sprite::ResolvedSelector resolved;
    ScopedPassthrough scopedPT(ws.m_interceptMgr, Passthrough_Sprite);
    MyError err = buffer.selector(job.output().spriteArea()).resolve(resolved);
    if (err != nullptr)
        return err;

    Sprite::HeaderView header(*resolved.header());
    imageOut = header.imagePtr();
    rowBytesOut = (uint32_t)header.rowBytes();
    rowsOut = header.height();
    return nullptr;
}

static MyError prepareDumpSource(JobWS& job,
                                          uintptr_t& dumpData,
                                          uint32_t& rowBytes,
                                          uint32_t& bufferRows,
                                          CoreWS& ws)
{
    job.output().setDumpPassImageCount(0);

    if (!usesPassBufferCycling(job)) {
        uint8_t* image = nullptr;
        MyError err = resolveBufferImage(job,
                                                  job.output().lineBuffer(0),
                                                  image,
                                                  rowBytes,
                                                  bufferRows,
                                                  ws);
        if (err != nullptr)
            return err;

        dumpData = (uintptr_t)image;
        return nullptr;
    }

    uint32_t imageIndex = 0;
    for (uint32_t pass = 1; pass <= job.config().numPasses(); ++pass) {
        uint8_t* image = nullptr;
        uint32_t passRowBytes = 0;
        uint32_t passRows = 0;
        MyError err = resolveBufferImage(job,
                                                  job.output().lineBuffer(pass),
                                                  image,
                                                  passRowBytes,
                                                  passRows,
                                                  ws);
        if (err != nullptr)
            return err;

        job.output().dumpPassImages()[imageIndex++] = image;
        rowBytes = passRowBytes;
        bufferRows = passRows;
    }

    if (hasExtra32bppBuffer(job)) {
        uint8_t* image = nullptr;
        uint32_t expandedRowBytes = 0;
        MyError err = resolveBufferImage(job,
                                         job.output().lineBuffer(job.config().numPasses() + 1u),
                                         image,
                                         expandedRowBytes,
                                         bufferRows,
                                         ws);
        if (err != nullptr)
            return err;

        job.output().dumpPassImages()[imageIndex++] = image;
        rowBytes = expandedRowBytes >> 2;
    }

    job.output().setDumpPassImageCount(imageIndex);
    dumpData = (uintptr_t)job.output().dumpPassImages();
    return nullptr;
}

static void advanceDumpSource(JobWS& job,
                              uintptr_t& dumpData,
                              uint32_t rowBytes,
                              uint32_t rowsDumped)
{
    uint32_t step = rowBytes * rowsDumped;

    if (job.output().dumpPassImageCount() == 0u) {
        dumpData += step;
        return;
    }

    for (uint32_t pass = 0; pass < job.config().numPasses(); ++pass)
        job.output().dumpPassImages()[pass] += step;

    if (hasExtra32bppBuffer(job))
        job.output().dumpPassImages()[job.config().numPasses()] += step << 2;
}

static MyError dumpOneChunk(JobWS& job,
                            uintptr_t dumpData,
                            int32_t& rowsRemaining,
                            uint32_t rowBytes,
                            int32_t& rowsDumped)
{
    int32_t chunkRows = (int32_t)dumpDepthRows(job);
    while (chunkRows < 32)
        chunkRows <<= 1;

    int32_t rowsToDump = rowsRemaining;
    if (rowsToDump > chunkRows)
        rowsToDump = chunkRows;

    PDumperCall call = {0};
    call.r0 = (uint32_t)dumpData;
    call.r1 = job.jobhandle;
    call.r2 = job.config().stripType();
    call.r3 = job.output().lineLength();
    call.r4 = (uint32_t)rowsToDump;
    call.r5 = rowBytes;
    call.r6 = job.output().halftoneWord();
    call.r7 = (uint32_t)(uintptr_t)job.config().configBytes();
    call.r8 = (uint32_t)(uintptr_t)&job.config().pdumperWord();

    MyError err = CallPDumperForJob(job, PDumperReason_OutputDump, call);
    if (err != nullptr)
        return err;

    rowsRemaining -= rowsToDump;
    rowsDumped = rowsToDump;
    return nullptr;
}

static MyError dumpCurrentBuffer(JobWS& job, CoreWS& ws)
{
    MyError err = restore_output_state(job);
    if (err != nullptr)
        return err;

    Sprite::ResolvedSelector resolved;
    {
        ScopedPassthrough scopedPT(ws.m_interceptMgr, Passthrough_Sprite);
        err = job.output().lineBuffer(0).selector(job.output().spriteArea()).resolve(resolved);
        if (err != nullptr)
            return err;

        err = Sprite::removeLeftHandWastage(resolved);
        if (err != nullptr)
            return err;
    }

    uint32_t bufferRows = 0;
    int32_t rowsRemaining = (int32_t)job.output().totalHeight() -
                            (int32_t)job.output().currentLine();
    if (rowsRemaining < 0)
        rowsRemaining = 0;

    uintptr_t dumpData = 0;
    uint32_t rowBytes = 0;
    err = prepareDumpSource(job, dumpData, rowBytes, bufferRows, ws);
    if (err != nullptr)
        return err;

    if (rowsRemaining > (int32_t)bufferRows)
        rowsRemaining = (int32_t)bufferRows;

    while (rowsRemaining > 0) {
        int32_t rowsDumped = 0;
        err = dumpOneChunk(job, dumpData, rowsRemaining, rowBytes, rowsDumped);
        if (err != nullptr)
            return err;

        if (rowsRemaining > 0)
            advanceDumpSource(job, dumpData, rowBytes, (uint32_t)rowsDumped);
    }

    return nullptr;
}

static int32_t scaledOriginOffset(int32_t coordMp, int32_t scaledFactor)
{
    int32_t lo = 0;
    int32_t hi = 0;
    full_signed_multiply(coordMp << bufferpix_l2size, scaledFactor, &lo, &hi);

    int32_t quotient = 0;
    uint32_t remainder = 0;
    extended_divide(hi, lo, 72000 * 256, &quotient, &remainder);
    return quotient;
}

static void swapClipDimensions(JobWS& job)
{
    const Size<OS::Unit>& clipSize(job.output().clipSize());
    job.output().setClipSize(Size<OS::Unit>(clipSize.height, clipSize.width));
}

static MyError finalisePreparedBuffer(JobWS& job,
                                               CoreWS& ws)
{
    if (job.output().currentBuffer() != &job.output().lineBuffer(0)) {
        DP::BufferSprite* rotatedBuffer = job.output().currentBuffer();
        bool useFastCopy = copysprite_withrotate_uses_fast_path(job,
                                                                *rotatedBuffer,
                                                                job.output().lineBuffer(0),
                                                                ws);

        MyError err = nullptr;
        if (!useFastCopy) {
            err = flip_buffer(job, ws);
            if (err != nullptr)
                return err;
        }

        job.output().currentBuffer() = &job.output().lineBuffer(0);
        swapClipDimensions(job);

        err = do_copysprite_withrotate(job,
                                       *rotatedBuffer,
                                       job.output().lineBuffer(0),
                                       ws);
        if (err != nullptr)
            return err;
    } else {
        MyError err = flip_buffer(job, ws);
        if (err != nullptr)
            return err;
    }

    job.output().setRotationId(DP::OutputState::RotationId_None);
    return nullptr;
}

static MyError prepareCurrentRectangleBuffer(JobWS& job,
                                                      CoreWS& ws)
{
    if ((job.output().rotationId() & 1) != 0) {
        MyError err = restore_output_state(job);
        if (err != nullptr)
            return err;

        if (job.output().bufferMarked()) {
            err = copysprite_withrotate(job,
                                        job.output().lineBuffer(0),
                                        job.output().rotationBuffer(),
                                        ws);
            if (err != nullptr)
                return err;
        }

        job.output().currentBuffer() = &job.output().rotationBuffer();
        if (job.output().bufferMarked())
            err = redirect_output(job, ws);
        else
            err = prepareCurrentBuffer(job, ws);
        if (err != nullptr)
            return err;

        DP::OutputState::RotationId savedRotationId = job.output().rotationId();
        job.output().setRotationId(DP::OutputState::RotationId_Rotate180);
        err = flip_buffer(job, ws);
        job.output().setRotationId(savedRotationId);
        if (err != nullptr)
            return err;

        swapClipDimensions(job);
    }

    return flip_buffer(job, ws);
}

static MyError prepareRectangle(const UserRectangle& rect,
                                JobWS& job,
                                bool& prepared,
                                Rect<OS::Unit>& boxOut,
                                CoreWS& ws)
{
    prepared = false;

    RotationId rotationId;
    int32_t xScaleFactor, yScaleFactor;
    if (!rectTransformState(rect, rotationId, xScaleFactor, yScaleFactor))
        return nullptr;

    const Rect<OS::Millipoint>& printArea = job.output().printArea();
    int32_t stripTopPixels = (int32_t)job.output().totalHeight() -
                             (int32_t)job.output().currentLine();
    int32_t stripBottomPixels = stripTopPixels - (int32_t)currentStripRows(job);

    int32_t top = Divide(stripTopPixels * 72000,
                         (int32_t)job.info.pixelResY()) + printArea.y0.raw();
    int32_t bottom = Divide(stripBottomPixels * 72000,
                            (int32_t)job.info.pixelResY()) + printArea.y0.raw();
    int32_t left = printArea.x0.raw() - rect.rectbottomleft.x.raw();
    int32_t right = printArea.x1.raw() - rect.rectbottomleft.x.raw();
    bottom -= rect.rectbottomleft.y.raw();
    top -= rect.rectbottomleft.y.raw();

    if (!!(rotationId & RotationId_SwappedAxes)) {
        int32_t rotatedLeft = bottom;
        int32_t rotatedBottom = -left;
        int32_t rotatedRight = top;
        int32_t rotatedTop = -right;
        left = rotatedLeft;
        bottom = rotatedBottom;
        right = rotatedRight;
        top = rotatedTop;
    }

    int32_t transformedLeft = 0;
    int32_t transformedRight = 0;
    int32_t transformedBottom = 0;
    int32_t transformedTop = 0;

    transformedLeft = divide_and_scale(left, xScaleFactor);
    transformedRight = divide_and_scale(right, xScaleFactor);
    transformedBottom = divide_and_scale(bottom, yScaleFactor);
    transformedTop = divide_and_scale(top, yScaleFactor);

    if (!!(rotationId & RotationId_NegativeXScale)) {
        transformedLeft = -transformedLeft;
        transformedRight = -transformedRight;
    }
    if (!!(rotationId & RotationId_NegativeYScale)) {
        transformedBottom = -transformedBottom;
        transformedTop = -transformedTop;
    }

    int32_t originXMp = (!!(rotationId & RotationId_NegativeXScale)) ? transformedRight : transformedLeft;
    const bool swappedAxes = !!(rotationId & DP::OutputState::RotationId_SwappedAxes);
    const bool negativeYScale = !!(rotationId & DP::OutputState::RotationId_NegativeYScale);
    const bool useBottomForOriginY = swappedAxes == negativeYScale;
    int32_t originYMp = useBottomForOriginY ? transformedBottom : transformedTop;

    if (transformedLeft > transformedRight) {
        int32_t temp = transformedLeft;
        transformedLeft = transformedRight;
        transformedRight = temp;
    }
    if (transformedBottom > transformedTop) {
        int32_t temp = transformedBottom;
        transformedBottom = transformedTop;
        transformedTop = temp;
    }

    int32_t rectWidthMp = rect.rectbox.width.raw() * 400;
    int32_t rectHeightMp = rect.rectbox.height.raw() * 400;
    if (transformedRight < 0 || transformedTop < 0)
        return nullptr;
    if (transformedLeft > rectWidthMp || transformedBottom > rectHeightMp)
        return nullptr;

    boxOut = Rect<OS::Unit>(OS::Unit(Divide(transformedLeft, 400) - 2),
                            OS::Unit(Divide(transformedBottom, 400) - 2),
                            OS::Unit(Divide(transformedRight, 400) + 2),
                            OS::Unit(Divide(transformedTop, 400) + 2));

    int32_t xDpi = (rotationId & RotationId_SwappedAxes) ? (int32_t)job.info.pixelResY()
                                     : (int32_t)job.info.pixelResX();
    int32_t yDpi = (rotationId & RotationId_SwappedAxes) ? (int32_t)job.info.pixelResX()
                                     : (int32_t)job.info.pixelResY();
    int32_t currentXScale = xScaleFactor * xDpi;
    int32_t currentYScale = yScaleFactor * yDpi;

    job.output().setRotationId(rotationId);
    job.output().setCurrentScale(currentXScale, currentYScale);
    job.output().setCurrentOffset(scaledOriginOffset(originXMp, currentXScale),
                                    scaledOriginOffset(originYMp, currentYScale));

    MyError err = prepareCurrentRectangleBuffer(job, ws);
    if (err != nullptr)
        return err;

    job.usersbg = rect.rectanglebg;
    job.usersoffset = rect.rectoffset;
    job.usersbox = rect.rectbox;
    job.userstransform = rect.recttransform;
    job.usersbottomleft = rect.rectbottomleft;

    prepared = true;
    return nullptr;
}

MyError pagebox_setup(int32_t copies,
                      uint32_t page_sequence,
                      const char* page_number,
                      CoreJobWS& coreJob)
{
    (void)page_sequence;
    (void)page_number;

    CoreWS& ws = CoreWS::instance();
    JobWS& job(toJobWS(coreJob));

    if (ws.isCountingPass())
        ws.counting_nextrect = coreJob.rectlist.head();

    ws.jpeg_maxmemory = 0; // ready to find max.

    MyError err = setupPageMetrics(job, ws);
    if (err != nullptr)
        return err;

    if (ws.isCountingPass())
        return nullptr;

    // allocate memory now, if no pre-scan
    return pageboxMemorySetup(copies, job, ws);
}

// Device.h entry point
MyError pagebox_nextbox(uint32_t& copies,
                        uint32_t& rectId,
                        Rect<OS::Unit>& box,
                        CoreJobWS& coreJob)
{
    MyError err;
    CoreWS& ws = CoreWS::instance();
    JobWS& job(toJobWS(coreJob));

    if (ws.isCountingPass()) {
        UserRectangle* rect = ws.counting_nextrect;
        if (rect != nullptr) {
            // `pagebox_nextbox_morecounting`
            ws.counting_nextrect = rect->next();

            rectId = rect->rectangleid;
            box = Rect<OS::Unit>(rect->rectoffset, rect->rectbox);

            return nullptr;
        }

        ws.m_countingPass = false; // counting done if nextrect=0

        // allocate memory now
        if ((err = pageboxMemorySetup(copies, job, ws)) != nullptr)
            return err;

        // Continue with `pagebox_real_nextbox`.
    }

    // `pagebox_real_nextbox`
    if ((err = finalisePreparedBuffer(job, ws)) != nullptr)
        return err;

    UserRectangle* rect = job.output().currentRect();
    Rect<OS::Unit> preparedBox;
    for (;;) {
        while (rect != nullptr) {
            job.output().currentRectRef() = rect->next();

            bool prepared = false;
            err = prepareRectangle(*rect,
                                   job,
                                   prepared,
                                   preparedBox,
                                   ws);
            if (err != nullptr)
                return err;

            if (prepared) {
                err = pagebox_setmaxbox(job);
                if (err != nullptr)
                    return err;

                rectId = rect->rectangleid;
                box = preparedBox;

                job.output().setBufferMarked(true);

                return nullptr;
            }

            rect = job.output().currentRect();
        }

        err = nullptr;
        if (usesPassBufferCycling(job)) {
            uint8_t nextPass = job.config().pass() + 1;
            bool wrapped = false;
            if (nextPass > job.config().numPasses()) {
                nextPass = 1;
                wrapped = true;
            }

            job.config().setPass(nextPass);
            job.output().lineBuffer(0).copyFrom(job.output().lineBuffer(nextPass));

            if (!wrapped) {
                err = restore_output_state(job);
                if (err != nullptr)
                    return err;

                err = prepareCurrentBuffer(job, ws);
                if (err != nullptr)
                    return err;

                initialiseCurrentRectangle(job);
                rect = job.output().currentRect();
                continue;
            }
        }

        err = dumpCurrentBuffer(job, ws);
        if (err != nullptr)
            return err;

        err = prepareCurrentBuffer(job, ws);
        if (err != nullptr)
            return err;

        job.output().setCurrentLine((int32_t)job.output().currentLine() +
                                    (int32_t)currentStripRows(job));

        if (job.output().currentLine() < job.output().totalHeight()) {
            initialiseCurrentRectangle(job);
            rect = job.output().currentRect();
            continue;
        }

        uint32_t nextCopies = 0;
        err = endPage(copies, &nextCopies, job);
        if (err != nullptr)
            return err;

        if (nextCopies == 0) {
            copies = 0;
            return nullptr;
        }

        if ((err = pageboxMemorySetup(nextCopies, job, ws)) != nullptr)
            return err;

        initialiseCurrentRectangle(job);
        rect = job.output().currentRect();
    }
}

MyError pagebox_setmaxbox(CoreJobWS& coreJob)
{
    CoreJobWS& job(coreJob);
    Point<OS::Unit> zero(0, 0);
    Rect<OS::Unit> clipBox(zero, job.usersbox);

    return pagebox_setnewbox(clipBox, coreJob);
}

static Rect<OS::Unit> scaleBox(const Rect<OS::Unit>& box, JobWS& job)
{
    Rect<Draw::Unit> t(XScale(Draw::fromOSUnit(box.x0), job),
                       YScale(Draw::fromOSUnit(box.y0), job),
                       XScale(Draw::fromOSUnit(box.x1), job),
                       YScale(Draw::fromOSUnit(box.y1), job));

    t.offsetBy(job.output().currentOffset());

    return Rect<OS::Unit>(toOSUnit(t.x0), toOSUnit(t.y0), toOSUnit(t.x1), toOSUnit(t.y1));
}

static OS::Unit scaleBoxY(JobWS& job, OS::Unit y)
{
    return OS::Unit((YScale(y.raw() << 8, job) -
                     (int32_t)job.output().currentOffset().dy.raw()) >> 8);
}

static void snapToPixels(Rect<OS::Unit>& rect)
{
    // Round to a whole number of pixels. This is incorrect for -ve numbers,
    // but matches asm. (three OS units in a row match zero, -1 == 0 == +1)
    rect.x0 = OS::Unit(rect.x0.raw() & ~(bufferpix_mask));
    rect.y0 = OS::Unit(rect.y0.raw() & ~(bufferpix_mask));
    rect.x1 = OS::Unit(rect.x1.raw() & ~(bufferpix_mask));
    rect.y1 = OS::Unit(rect.y1.raw() & ~(bufferpix_mask));
}

static void orderBox(Rect<OS::Unit>& box)
{
    if (box.x0 > box.x1) {
        OS::Unit t = box.x0;
        box.x0 = box.x1;
        box.x1 = t;
    }
    if (box.y0 > box.y1) {
        OS::Unit t = box.y0;
        box.y0 = box.y1;
        box.y1 = t;
    }
}

// Device.h entry point
MyError pagebox_setnewbox(const Rect<OS::Unit>& box, CoreJobWS& coreJob)
{
    JobWS& job(toJobWS(coreJob));

    Rect<OS::Unit> scaledBox(scaleBox(box, job));

    snapToPixels(scaledBox);
    // ensure correct order (should really be done much earlier)
    orderBox(scaledBox);

    // now clip this against the window we can set
    Point<OS::Unit> zero(0,0);
    Rect<OS::Unit> bounds(zero, job.output().clipSize());

    if (scaledBox.x1 < bounds.x0 || scaledBox.y1 < bounds.y0 ||
        scaledBox.x0 > bounds.x1 || scaledBox.y0 > bounds.y1) {
        job.disabled |= Disabled_NullClip;
        return nullptr;
    }

    job.disabled &= ~Disabled_NullClip;
    scaledBox.clipWith(bounds);

    scaledBox.x1 -= OS::Unit(1);
    scaledBox.y1 -= OS::Unit(1);

    MyError err;
    if ((err = vdu_char(24)) != nullptr)
        return err;
    if ((err = vdu_pair((uint32_t)scaledBox.x0.raw())) != nullptr)
        return err;
    if ((err = vdu_pair((uint32_t)scaledBox.y0.raw())) != nullptr)
        return err;
    if ((err = vdu_pair((uint32_t)scaledBox.x1.raw())) != nullptr)
        return err;
    return vdu_pair((uint32_t)scaledBox.y1.raw());
}

MyError pagebox_cleartobg(CoreJobWS& coreJob)
{
    JobWS& job(toJobWS(coreJob));
    if ((job.disabled & Disabled_NullClip) != 0)
        return nullptr;

    MyError err = setBackgroundColour((uint32_t)job.usersbg, job);
    if (err != nullptr)
        return err;

    return vdu_char(16);
}
