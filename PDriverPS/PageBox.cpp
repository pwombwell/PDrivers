#include "Core/PDriver.h"
#include "PageBox.h"

#include "Colour.h"
#include "GlobalWS.h"
#include "JobWS.h"
#include "Output.h"
#include "PDriverPS.h"
#include "Private.h"

#include "Core/Job.h"
#include "Core/Workspace.h"

#include "RLib/OS/CharInput.h"
#include "RLib/OS/Convert.h"
#include "RLib/OS/File.h"

#include "RLibX/Font.h"
#include "RLibX/MakePSFont.h"
#include "RLibX/OS.h"

#include <stddef.h>

static MyError pagebox_generateprologue(JobWS& job);

/* Page control and box generation routines for the PostScript printer driver. */

#if PSDebugEscapes
static MyError readescapestate(int *escaped) {
    bool escapedBool = false;
    MyError err = OS::xreadEscapeState(escapedBool);
    *escaped = escapedBool;
    if (!err) {
        volatile uint32_t *debug_ptr = (volatile uint32_t *)(uintptr_t)0x100000;
        *debug_ptr = (*escaped) ? 1u : 0u;
    }
    return err;
}
#endif

static void floorCeilDiv(const Base::Fixed64<12>& numerator,
                         const Base::Fixed<12>& divisor,
                         PS::Unit& floorValue,
                         PS::Unit& ceilValue)
{
    int64_t quotient = numerator.raw() / divisor.raw();
    int64_t remainder = numerator.raw() % divisor.raw();

    if (remainder < 0) {
        remainder += divisor.raw();
        quotient -= 1;
    }

    floorValue = PS::Unit((int32_t)quotient);
    ceilValue = PS::Unit((int32_t)(remainder == 0 ? quotient : quotient + 1));
}

inline Base::Fixed64<12> millipointToFixed12(const OS::Millipoint& mp) {
    int64_t raw = mp.raw();
    return Base::Fixed64<12>::fromRaw(raw << 12);
}

// 400 millipoints in an OS unit (1000 * 2/5).
// Our OS unit is in [:16], and we want the result in [:12], so
// multiply by (400/16) = 25 and return in the new unit.
inline Base::Fixed64<12> osunit16ToMillipoint12(const Base::Fixed64<16>& os) {
    int64_t raw = os.raw();
    return Base::Fixed64<12>::fromRaw(raw * 25);
}

// Subroutine to merge a point specified in the rectangle co-ordinate system
// (OS units before transformation, relative to the rectangle's bottom left
// corner) into the page bounding box so far (in points, relative to the
// bottom left corner of the paper).
//   Entry: R0,R1 = point
//          R4-R7 = page bounding box so far
//          R9 points to rectangle
//   Exit:  R4-R7 updated
//          All other registers preserved
//
// This requires some rather messy arithmetic. We start with the specified
// point, which is given in OS units before transformation - i.e. units of 2/5
// point. Then the rectangle's transformation is applied, resulting in a
// transformed point given in units of 1/(5*2^15) point. Then we need to add
// in the bottom left point of the rectangle, given in units of 1/1000 point.
// This is done by adding 25 times the transformed point and 2^12 times the
// bottom left point, giving us the desired position in units of 1/(1000*2^12)
// point.
//   Finally, we divide by 1000*2^12 to get the required position in points,
// and merge the result into the bounding box so far. During this, we take
// care to use a rounded up quotient for upper bounds and a rounded down
// position for lower bounds.
//   These calculations definitely require double precision work. By strict
// mathematical standards, they require triple precision, but this has not
// been done because paper that big just isn't used!
static void pagebox_mergepointintopagebbox(OS::Unit x,
                                           OS::Unit y,
                                           const UserRectangle& rect,
                                           Rect<PS::Unit>& out)
{
#if 1
    // Norcroft refuses to pass structs with methods in regs.

    // OS::Units are 1/180th inch. PS::Units are 1/72th inch.
    // 400 Millipoints in an OS::Unit. 

    // OS::Unit * [16:16] scale factor = OS::Unit at [:16]
    Base::Fixed64<16> ax = rect.recttransform.a.mul64(x.raw());
    Base::Fixed64<16> bx = rect.recttransform.b.mul64(x.raw());
    Base::Fixed64<16> cy = rect.recttransform.c.mul64(y.raw());
    Base::Fixed64<16> dy = rect.recttransform.d.mul64(y.raw());

    // (Add and) convert OS::Unit [:16] to millipoints ([:12]).
    Base::Fixed64<12> scaledX = osunit16ToMillipoint12(ax + cy);
    Base::Fixed64<12> scaledY = osunit16ToMillipoint12(bx + dy);

    // Convert location from millipoint, to millipoint [:12], and add.
    scaledX += millipointToFixed12(rect.rectbottomleft.x);
    scaledY += millipointToFixed12(rect.rectbottomleft.y);

    // Scale millipoints [:12] to PS::Unit [:0]
    // ie. divide by 1000, then >> 12, or divide by (1000 << 12).
    // The latter gives us a more accurate remainder.

    Base::Fixed<12> divisor = Base::Fixed<12>::fromInt(1000);

    PS::Unit lowerX, lowerY, upperX, upperY;
    floorCeilDiv(scaledX, divisor, lowerX, upperX);
    floorCeilDiv(scaledY, divisor, lowerY, upperY);

    if (out.x0 > lowerX)
        out.x0 = lowerX;

    if (out.y0 > lowerY)
        out.y0 = lowerY;

    if (out.x1 < upperX)
        out.x1 = upperX;

    if (out.y1 < upperY)
        out.y1 = upperY;
#else
    int64_t ax, bx, cy, dy;

    ax = rect.recttransform.a.raw() * x.raw();
    bx = rect.recttransform.b.raw() * x.raw();
    cy = rect.recttransform.c.raw() * y.raw();
    dy = rect.recttransform.d.raw() * y.raw();

    int64_t scaledX = (ax + cy) * 25;
    int64_t scaledY = (bx + dy) * 25;

    scaledX += ((int64_t)rect.rectbottomleft.x.raw() << 12);
    scaledY += ((int64_t)rect.rectbottomleft.y.raw() << 12);

    const int32_t divisor = 1000 << 12;

    int64_t qX = scaledX / divisor;
    int64_t qY = scaledY / divisor;
    int64_t remX = scaledX % divisor;
    int64_t remY = scaledY % divisor;

    // Match arith_dpdivmod(): quotient rounded down, remainder always >= 0.
    if (remX < 0) {
        remX += divisor;
        qX -= 1;
    }
    if (remY < 0) {
        remY += divisor;
        qY -= 1;
    }

    debugLog("bbox merge point:%d,%d bl:%d,%d box:%d,%d matrix:%d,%d,%d,%d scaled:%lld,%lld q:%lld,%lld rem:%lld,%lld before:%d,%d,%d,%d",
             x.raw(),
             y.raw(),
             rect.rectbottomleft.x.raw(),
             rect.rectbottomleft.y.raw(),
             rect.rectbox.width.raw(),
             rect.rectbox.height.raw(),
             rect.recttransform.a.raw(),
             rect.recttransform.b.raw(),
             rect.recttransform.c.raw(),
             rect.recttransform.d.raw(),
             scaledX,
             scaledY,
             qX,
             qY,
             remX,
             remY,
             out.x0.raw(),
             out.y0.raw(),
             out.x1.raw(),
             out.y1.raw());

    if (out.x0 > qX)
        out.x0 = PS::Unit((int32_t)qX);

    if (out.y0 > qY)
        out.y0 = PS::Unit((int32_t)qY);

    if (remX != 0)
        qX += 1;

    if (remY != 0)
        qY += 1;

    if (out.x1 < qX)
        out.x1 = PS::Unit((int32_t)qX);

    if (out.y1 < qY)
        out.y1 = PS::Unit((int32_t)qY);

    debugLog("bbox merge after:%d,%d,%d,%d",
             out.x0.raw(),
             out.y0.raw(),
             out.x1.raw(),
             out.y1.raw());
#endif
}

static MyError pagebox_generateprologue_newstyle(JobWS& job)
{
    MyError err;
    Output& output(job.output());
    PS::Document& document(job.document());
    PS::FontCatalogue& fonts(document.fonts());

#if PSDebugPageBox
    MyError err = output.str("XXXX Entered pagebox_generateprologue_newstyle\n");
    if (err) {
        return err;
    }
#endif

    document.setVerbose(false);

    if ((err = pagebox_insertfile(prologue2_filename, job)) != nullptr)
        return err;

    DeclaredFont* node = fonts.firstDeclaredFont();
debugLog("declaring fonts list %p", node);
    if (node != nullptr) {
        if ((err = output.str('\n')) != nullptr)
            return err;
        if ((err = output.str("PDdict begin userdict begin\n")) != nullptr)
            return err;
    }

    while (node != nullptr) {
        uint32_t flags_word = node->flags;
        const char *name = (const char *)node->chars();

        uint32_t flags = 0;
        if (document.accents())
            flags = 16u;

        flags |= 1u | 4u;

        if ((flags_word & 1u) != 0)
            flags |= 2u;

        if ((flags_word & 2u) != 0)
            flags |= 8u;

        debugLog("pagebox makefont node:%p name:%s flags:0x%x makeFlags:0x%x",
                 node,
                 name,
                 node->flags,
                 flags);

        
        (void)MakePSFont::xmakeFont(job.jobhandle, name, nullptr, nullptr, flags);

        node = node->m_next;
    }

    if (fonts.hasDeclaredFontEntries()) {
        if ((err = output.str("end end\n")) != nullptr)
            return err;
    }

    return output.str('\n');
}

MyError pagebox_setup(int32_t copies,
                      uint32_t page_sequence,
                      const char *page_number,
                      CoreJobWS& coreJob)
{
    MyError err;
    CoreWS& ws = CoreWS::instance();
    JobWS& job = *(JobWS*)&coreJob;
    Output& output(job.output());
    PS::Document& document(job.document());
    (void)copies;
    (void)page_sequence;

    ws.m_countingPass = false;

#if PSDebugPageBox
    MyError err = output.str("% pagebox_setup\n");
    if (err) {
        return err;
    }
#endif

    if (document.shouldSendPrologue()) {
        document.markPrologueSent();

        if ((err = pagebox_generateprologue(job)) != nullptr)
            return err;
    }

    debugLog("[pagebox_setup] generated prologue");
    UserRectangle *rect = coreJob.rectlist.head();
    document.setCurrentRect(rect);

    Rect<PS::Unit> bounds(0x7FFFFFFF, 0x7FFFFFFF,
                          (int32_t)0x80000000u, (int32_t)0x80000000u);

    debugLog("bbox setup initial:%d,%d,%d,%d",
             bounds.x0.raw(),
             bounds.y0.raw(),
             bounds.x1.raw(),
             bounds.y1.raw());

    while (rect != nullptr) {
        debugLog("bbox rect:%p next:%p bl:%d,%d offset:%d,%d box:%d,%d matrix:%d,%d,%d,%d",
                 rect,
                 rect->next(),
                 rect->rectbottomleft.x.raw(),
                 rect->rectbottomleft.y.raw(),
                 rect->rectoffset.dx.raw(),
                 rect->rectoffset.dy.raw(),
                 rect->rectbox.width.raw(),
                 rect->rectbox.height.raw(),
                 rect->recttransform.a.raw(),
                 rect->recttransform.b.raw(),
                 rect->recttransform.c.raw(),
                 rect->recttransform.d.raw());
        pagebox_mergepointintopagebbox(0, 0, *rect, bounds);
        pagebox_mergepointintopagebbox(rect->rectbox.width, 0, *rect, bounds);
        pagebox_mergepointintopagebbox(rect->rectbox.width, rect->rectbox.height, *rect, bounds);
        pagebox_mergepointintopagebbox(0, rect->rectbox.height, *rect, bounds);
        rect = rect->next();
    }

    debugLog("bbox page final before empty:%d,%d,%d,%d empty:%u",
             bounds.x0.raw(),
             bounds.y0.raw(),
             bounds.x1.raw(),
             bounds.y1.raw(),
             bounds.isEmpty() ? 1u : 0u);

    document.includeBoundingBox(bounds);
    if ((err = output.str("%%Page: ")) != nullptr)
        return err;
    if (page_number != nullptr) {
        if ((err = output_string(page_number, 33, 126, output)) != nullptr)
            return err;
    } else {
        if ((err = output.str('?')) != nullptr)
            return err;
    }
    if ((err = output.str(' ')) != nullptr)
        return err;
    if ((err = output.num((int32_t)job.numberofpages)) != nullptr)
        return err;
    if ((err = output.str('\n')) != nullptr)
        return err;

    if (bounds.isEmpty())
        bounds.set(0, 0, 0, 0);

    debugLog("bbox page emit:%d,%d,%d,%d",
             bounds.x0.raw(),
             bounds.y0.raw(),
             bounds.x1.raw(),
             bounds.y1.raw());

    if ((err = output.str("%%PageBoundingBox: ")) != nullptr)
        return err;
    if ((err = output.writeCoordPair(bounds.bottomLeft())) != nullptr)
        return err;
    if ((err = output.writeCoordSpace(bounds.x1)) != nullptr)
        return err;
    if ((err = output.writeCoord(bounds.y1)) != nullptr)
        return err;
    if ((err = output.str('\n')) != nullptr)
        return err;

    if ((err = output.str("PDdict begin\n")) != nullptr)
        return err;

    if ((err = output_save(job)) != nullptr)
        return err;

    if ((err = output.num(job.pageSize.rect.x0)) != nullptr)
        return err;
    if ((err = output.str(" MP ")) != nullptr)
        return err;
    if ((err = output.num(job.pageSize.rect.y0)) != nullptr)
        return err;
    if ((err = output.str(" MP ")) != nullptr)
        return err;
    if ((err = output.num(job.pageSize.rect.x1)) != nullptr)
        return err;
    if ((err = output.str(" MP ")) != nullptr)
        return err;
    if ((err = output.num(job.pageSize.rect.y1)) != nullptr)
        return err;
    if ((err = output.str(" MP \n")) != nullptr)
        return err;

    return output.str("PS\n");
}

MyError pagebox_nextbox(uint32_t& copies,
                        uint32_t& rectId,
                        Geometry::Rect<OS::Unit>& box,
                        CoreJobWS& coreJob)
{
    MyError err;
    CoreWS& ws = CoreWS::instance();
    JobWS& job = toJobWS(coreJob);

    Output& output(job.output());
    PS::Document& document(job.document());
#if PSDebugPageBox
    if ((err = output.str("% pagebox_nextbox\n")) != nullptr)
        return err;
#endif

    const UserRectangle *rect = document.takeCurrentRect();

    UserRectangle *rectlist = coreJob.rectlist.head();

    if (rect != rectlist) {
        if ((err = pagebox_setmaxbox(job)) != nullptr)
            return err;
        if ((err = output_restore(job)) != nullptr)
            return err;
    }

    if (rect == nullptr) {
        if ((err = output_restore(job)) != nullptr)
            return err;
        if ((err = output.num(copies)) != nullptr)
            return err;
        if ((err = output.str(" Copies\n")) != nullptr)
            return err;
        if ((err = output.str("end\n")) != nullptr)
            return err;

        copies = 0;

        return nullptr;
    }

    vdu5_changed(coreJob, (uint32_t)-1);

    if ((err = output_save(job)) != nullptr)
        return err;

    coreJob.usersbg = rect->rectanglebg;

    coreJob.usersbottomleft.x = rect->rectbottomleft.x;
    coreJob.usersbottomleft.y = rect->rectbottomleft.y;

    debugLog("bbox nextbox rect:%p bl:%d,%d box:%d,%d matrix:%d,%d,%d,%d",
             rect,
             rect->rectbottomleft.x.raw(),
             rect->rectbottomleft.y.raw(),
             rect->rectbox.width.raw(),
             rect->rectbox.height.raw(),
             rect->recttransform.a.raw(),
             rect->recttransform.b.raw(),
             rect->recttransform.c.raw(),
             rect->recttransform.d.raw());
    if ((err = output.writeCoord(rect->rectbottomleft.x)) != nullptr)
        return err;
    if ((err = output.str(" MP ")) != nullptr)
        return err;
    if ((err = output.writeCoord(rect->rectbottomleft.y)) != nullptr)
        return err;
    if ((err = output.str(" MP T\n")) != nullptr)
        return err;

    coreJob.userstransform = rect->recttransform;

    if ((err = output_coordpair(rect->recttransform.a.raw(), rect->recttransform.b.raw(), output)) != nullptr)
        return err;
    if ((err = output_coordpair(rect->recttransform.c.raw(), rect->recttransform.d.raw(), output)) != nullptr)
        return err;
    if ((err = output.str("UM\n")) != nullptr)
        return err;

#if PSCoordSpeedUps
    job.coordsystem = 0;
#endif

    coreJob.usersoffset.dx = rect->rectoffset.dx;
    coreJob.usersoffset.dy = rect->rectoffset.dy;
    coreJob.usersbox.width = rect->rectbox.width;
    coreJob.usersbox.height = rect->rectbox.height;

    if ((err = output_coordpair(rect->rectbox, output)) != nullptr)
        return err;
    if ((err = output.str("MC\n")) != nullptr)
        return err;

    document.setClipped(false);

    rectId = rect->rectangleid;

    box.set(0, 0, rect->rectbox.width, rect->rectbox.height);

    return nullptr;
}

MyError pagebox_setmaxbox(CoreJobWS& coreJob)
{
    MyError err;
    JobWS& job = (JobWS&)coreJob;
    Output& output(job.output());
    PS::Document& document(job.document());
#if PSDebugPageBox
    if ((err = output.str("% pagebox_setmaxbox\n")) != nullptr)
        return err;
#endif

#if PSCoordSpeedUps
    if ((err = ensure_OScoords(output, job)) != nullptr)
        return err;
#endif

    if (document.clipped()) {
        if ((err = output_grestore(job)) != nullptr)
            return err;
        document.setClipped(false);
    }

    return nullptr;
}

MyError pagebox_cleartobg(CoreJobWS& coreJob)
{
    MyError err;
    JobWS& job(toJobWS(coreJob));

    Output& output(job.output());

#if PSDebugPageBox
    MyError err = output.str("% pagebox_cleartobg\n");
    if (err) {
        return err;
    }
#endif

    if ((err = colour_setrealrgb(job.usersbg, job)) != nullptr)
        return err;
#if PSCoordSpeedUps
    if ((err = colour_ensure(job)) != nullptr)
        return err;
#endif

    return output.str("CP fill\n");
}

MyError pagebox_setnewbox(const Geometry::Rect<OS::Unit>& box, CoreJobWS& coreJob)
{
    CoreWS& ws = CoreWS::instance();
    JobWS& job = *(JobWS *)&coreJob;
    Output& output(job.output());
    PS::Document& document(job.document());
#if PSDebugPageBox
    MyError err = output.str("% pagebox_setnewbox\n");
    if (err) {
        return err;
    }
#endif

    MyError err;
    if ((err = pagebox_setmaxbox(job)) != nullptr)
        return err;
    if ((err = output_gsave(job)) != nullptr)
        return err;
    document.setClipped(true);

    if ((err = output_coordpair(box.bottomLeft(), output)) != nullptr)
        return err;
    if ((err = output_coordpair(box.topRight(), output)) != nullptr)
        return err;
    return output.str("Cp\n");
}

MyError pagebox_generateprologue(JobWS& job)
{
    PS::Document& document(job.document());
    PS::FontCatalogue& fonts(document.fonts());
    Output& output(job.output());

    MyError err = output.str("%%BeginProlog\n");
    if (err)
        return err;

    if (!job.illustrationjob) {
        if ((err = pagebox_insertfile(psfeed_filename, job)) != nullptr)
            return err;
    }

    if (fonts.hasDeclaredFonts()) {
        /* New style. */
        if ((err = pagebox_generateprologue_newstyle(job)) != nullptr)
            return err;
    } else {
        /* Old style application. */
        if (document.verbose()) {
            if ((err = pagebox_insertfile(prologue_filename, job)) != nullptr)
                return err;
            if ((err = pagebox_insertfile(prologue2_filename, job)) != nullptr)
                return err;
        } else {
#if PSDebugPageBox
            if ((err = output.str("XXXX Entered pagebox_generateprologue_oldstyle\n")) != nullptr)
                return err;
#endif

            char buffer[48];
            int32_t list_context = 0;
            fonts.resetDeclaredFonts();

            for (;;) {
                int32_t context = list_context;
                if ((err = Font::xlistFonts(buffer, &context, -1)) != nullptr)
                    return err;
                if (context == -1) {
                    break;
                }
                list_context = context;

                const char* listfont = buffer;
                const char* namePtr = Font::Identifier(listfont).locateName();
                if (namePtr == nullptr)
                    continue;

                const FontBlock* block = job.jobfontlist.head();
                while (block != nullptr) {
                    if (block->word() != PDriverMiscOp_PS_Font) {
                        block = block->next();
                        continue;
                    }

                    const char* blockNamePtr = Font::Identifier(block->riscos()).locateName();
                    if (blockNamePtr == nullptr) {
                        block = block->next();
                        continue;
                    }

                    if (Font::Identifier::compareSegmentCaseless(namePtr, blockNamePtr) == 0) {
                        (void)font_declare(listfont, 0, job);
                        break;
                    }
                    block = block->next();
                }
            }

            if ((err = pagebox_generateprologue_newstyle(job)) != nullptr)
                return err;
        }
    }

    if ((err = output.str("%%EndProlog\n")) != nullptr)
        return err;
    if ((err = output.str('\n')) != nullptr)
        return err;

    if (job.illustrationjob)
        return nullptr;

    if ((err = output.str("%%BeginSetup\n")) != nullptr)
        return err;

    if ((err = pagebox_insertfile(pspaper_filename, job)) != nullptr)
        return err;

    if ((err = output_immgstring("<PDriver$PSextra>", output)) != nullptr)
        return err;
    if ((err = output.str('\n')) != nullptr)
        return err;

    if ((err = output.str("%%EndSetup\n")) != nullptr)
        return err;
    return output.str('\n');
}

// Scoped file lifetype handler, for reading files. Close ignores errors, but
//  they're unlikely for reads (the original asm ignored close errors too).
class ScopedReadFile {
public:
    /*explicit*/ ScopedReadFile(FileHandle file) : m_file(file) {}

    ~ScopedReadFile() {
        if (m_file != 0) {
            (void)OS::closeFile(m_file);
        }
    }

    FileHandle get() const { return m_file; }

private:
    ScopedReadFile(const ScopedReadFile&);
    ScopedReadFile& operator=(const ScopedReadFile&);

    FileHandle m_file;
};

const char prologue_filename[] = "<PDriver$PSprologue>";
const char prologue2_filename[] = "<PDriver$PSprologue2>";
const char epilogue_filename[] = "<PDriver$PSepilogue>";
const char psfeed_filename[] = "<PDriver$PSFeed>";
const char pspaper_filename[] = "<PDriver$PSPaper>";

// Subroutine to insert a file into the PostScript output. Strips comments
// and compresses multiple new lines. Rules:
//   (a) Lines starting "%%" or "%!" are copied verbatim.
//   (b) Lines starting "%" but not "%%" or "%!" are removed completely.
//   (c) If a "%" appears in a line that doesn't start "%%" or "%!", it and
//       all characters up to but not including the next new line are removed.
// This is done using a finite state machine driven by the characters read.
// Entry: R0 -> name of variable holding file name (relative to <PDriver$Dir>
//              or PDriver:).
// Exit:  All registers preserved
// `pagebox_insertfile`
MyError pagebox_insertfile(const char* filename, JobWS& job)
{
    MyError err;
    PDriverWS& ws = (PDriverWS&)CoreWS::instance();
    Output& output(job.output());

    debugLog("[pagebox_insertfile] filename:%s", filename);

    char* buffer = ws.expansionbuffer;
    size_t bufferSize = sizeof(ws.expansionbuffer);

    uint32_t written;
    if ((err = OS::gsTrans(filename, buffer, bufferSize, written)) != nullptr)
        return err;

    if (written == 0)
        return nullptr;

    // Make certain result is zero-terminated
    buffer[written] = '\0';

    FileHandle file;
    if ((err = OS::openFile(buffer, file)) != nullptr) {
        debugLog("    errored:%s", err.message());
        return err;
    }

    ScopedReadFile scopedFile(file);

    for (;;) {
        // `pagebox_insertfile_linestart`
        if (OS::readEscapeState())
            return MyError(ErrorBlock_Escape);

        char c;
        bool eof;
        if ((err = OS::bget(file, c, eof)) != nullptr)
            return err;
        if (eof)
            return nullptr;

        if (c == '\n')
            continue;

        if (c == '%')
            goto linestartpercent;

        if ((err = output.byte(c)) != nullptr)
            return err;

copy:
        if ((err = OS::bget(file, c, eof)) != nullptr)
            return err;
        if (eof)
            return nullptr;

        if (c == '%')
            goto ignoreuptonewline;

        if ((err = output.byte(c)) != nullptr)
            return err;
        if (c != '\n')
            goto copy;

        continue;

linestartpercent:
        if ((err = OS::bget(file, c, eof)) != nullptr)
            return err;
        if (eof)
            return nullptr;

        if (c == '\n')
            continue;

        if (c != '%' && c != '!')
            goto ignoreline;

        if ((err = output.str('%')) != nullptr)
            return err;
        if ((err = output.byte(c)) != nullptr)
            return err;

copyverbatim:
        if ((err = OS::bget(file, c, eof)) != nullptr)
            return err;
        if (eof)
            return nullptr;

        if ((err = output.byte(c)) != nullptr)
            return err;
        if (c != '\n')
            goto copyverbatim;
        continue;

ignoreuptonewline:
        if ((err = OS::bget(file, c, eof)) != nullptr)
            return err;
        if (eof)
            return nullptr;

        if (c != '\n')
            goto ignoreuptonewline;

        if ((err = output.byte(c)) != nullptr)
            return err;
        continue;

ignoreline:
        if ((err = OS::bget(file, c, eof)) != nullptr)
            return err;
        if (eof)
            return nullptr;

        if (c != '\n')
            goto ignoreline;

        continue;
    }

    return err;
}
