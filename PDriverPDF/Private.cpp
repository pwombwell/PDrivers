#include "Core/PDriver.h"
#include "Private.h"

#include "JobWS.h"
#include "Output.h"
#include "PDriverPDF.h"
#include "PDF/FontRecord.h"
#include "PDF/PageRef.h"

#include "Core/Job.h"
#include "Core/Workspace.h"

#include "RLib/OS.h"
#include "RLib/OS/Convert.h"

#include "RLibX/Font.h"
#include "RLibX/OS.h"

#include <stddef.h>
#include <string.h>

#if Medusa
extern const uint32_t SpriteType_RISCOS5;
extern const uint32_t SpriteType_New8bpp;
#endif

Module* Module::create(void* pw)
{
    return new PDriverWS(pw);
}

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

#if DevelopmentChecks
static _kernel_oserror saveoverflowdisaster = {
    0,
    "DISASTER: Too many 'save's or 'gsave's"
};

static _kernel_oserror restoreunderflowdisaster = {
    0,
    "DISASTER: Too many 'restore's or 'grestore's"
};
#endif

static MyError output_expansionbuffer(PDriverWS& ws,
                                      int32_t len,
                                      Output& output)
{
    return output_stringN((const char*)ws.expansionbuffer, len, 1, 126, output);
}

/* Output a string of printable characters as a PostScript string, taking care:
 *   (a) not to produce lines of more than 72 characters.
 *   (b) to output characters outside the range ASCII 32-126 as octal escape
 *       sequences.
 *   (c) to output "(", ")" and "\\" as escape sequences.
 */
static MyError output_PSstring_dir(const uint8_t* str,
                                   uint32_t len,
                                   int step,
                                   Output& output)
{
    MyError err;
    const uint8_t* p = str;
    if (step < 0 && len != 0) {
        p = str + len - 1;
    }
    uint32_t linecount = 1;

    err = output.str('(');
    if (err)
        return err;

    while (len-- > 0) {
        if (linecount >= 68) {
            err = output.str('\\');
            if (err)
                return err;

            err = output.str('\n');
            if (err)
                return err;

            linecount = 0;
            int escaped = 0;
#if PSDebugEscapes
            err = readescapestate(&escaped);
#else
            bool escapedBool = false;
            err = OS::xreadEscapeState(escapedBool);
            escaped = escapedBool;
#endif
            if (err)
                return err;

            if (escaped)
                return ErrorBlock_Escape;
        }

        uint8_t ch = *p;
        p += step;

        if (ch < ' ' || ch >= 126) {
            linecount += 3;
            err = output.str('\\');
            if (err)
                return err;

            err = output.str('0' + (ch >> 6));
            if (err)
                return err;

            err = output.str('0' + ((ch >> 3) & 0x7));
            if (err)
                return err;

            ch = (uint8_t)('0' + (ch & 0x7));
            linecount += 1;
            err = output.str(ch);
            if (err)
                return err;

            continue;
        }

        if (ch == '\\' || ch == '(' || ch == ')') {
            linecount += 1;
            err = output.str('\\');
            if (err)
                return err;
        }

        linecount += 1;
        err = output.str(ch);
        if (err)
            return err;
    }

    err = output.str(')');
    if (err)
        return err;

    return output.str('\n');
}

MyError output_PSstringBackwards(const uint8_t* str,
                                 uint32_t len,
                                 Output& output)
{
    return output_PSstring_dir(str, len, -1, output);
}

MyError output_PSstring(const uint8_t* str,
                        uint32_t len,
                        Output& output)
{
    return output_PSstring_dir(str, len, 1, output);
}

/* Ensure that we are using the OS co-ordinate system. */
MyError ensure_OScoords(Output& output, JobWS& job)
{
    if (job.coordsystem != 0) {
        job.coordsystem = 0;
        return output_grestore(job);
    }

    return nullptr;
}

/* Ensure that we are using the text co-ordinate system with the correct
 * current font factors.
 */
MyError ensure_textcoords(Output& output, JobWS& job)
{
    int32_t scale_x, scale_y;
    MyError err = Font::xreadScaleFactor(scale_x, scale_y);
    if (err) {
        return err;
    }

    if (job.coordsystem == 1 &&
        job.coordscaleX == scale_x &&
        job.coordscaleY == scale_y) {
        return nullptr;
    }

    if ((err = ensure_OScoords(output, job)) != nullptr)
        return err;

    job.coordsystem = 1;
    job.coordscaleX = scale_x;
    job.coordscaleY = scale_y;

    if ((err = output_gsave(job)) != nullptr)
        return err;

    if ((err = output_coordpair(scale_x, scale_y, output)) != nullptr)
        return err;

    return output.str("TS\n");
}

static MyError output_savecommon(JobWS& jobWS)
{
#if DevelopmentChecks
    if (!jobWS.m_context.push())
        return &saveoverflowdisaster;
#else
    jobWS.m_context.push();
#endif
    return nullptr;
}

MyError output_save(JobWS& job)
{
    MyError err = job.output().str("q\n");
    if (err)
        return err;

    return output_savecommon(job);
}

MyError output_gsave(JobWS& job)
{
    MyError err = job.output().str("q\n");
    if (err)
        return err;

    return output_savecommon(job);
}

static MyError output_restorecommon(JobWS& job)
{
#if DevelopmentChecks
    if (!job.m_context.pop())
        return &restoreunderflowdisaster;
#else
    job.m_context.pop();
#endif
    return nullptr;
}

MyError output_restore(JobWS& job)
{
    MyError err = job.output().str("Q\n");
    if (err)
        return err;

    return output_restorecommon(job);
}

MyError output_grestore(JobWS& job)
{
    MyError err = job.output().str("Q\n");
    if (err)
        return err;

    return output_restorecommon(job);
}

MyError output_stringN(const char *s, int32_t len, uint8_t min, uint8_t max,
                               Output& output)
{
    MyError err;
    const uint8_t range = (uint8_t)(max - min);

    for (int32_t i = 0; i < len; ++i) {
        uint8_t c = (uint8_t)s[i];

        if (c - min <= range) {
            if ((err = output.str(c)) != nullptr)
                return err;
        }
    }

    return nullptr;
}

MyError output_immgstring(const char* s, Output& output)
{
    return output_gstring(s, output);
}

MyError output_gstring(const char* s, Output& output)
{
    PDriverWS& ws(PDriverWS::instance());

    size_t size = sizeof(ws.expansionbuffer);
    uint32_t written;
    MyError err = OS::gsTrans(s, (char*)ws.expansionbuffer, size, written);
    if (err)
        return err;

    if (written > 0)
        written -= 1;

    return output_expansionbuffer(ws, written, output);
}

MyError output_bytes(const uint8_t* data,
                     uint32_t length,
                     Output& output)
{
    for (uint32_t i = 0; i < length; ++i) {
        MyError err = output.byte(data[i]);
        if (err)
            return err;
    }

    return nullptr;
}

MyError output_coordpair(int32_t x, int32_t y, Output& output)
{
    return output.writeCoordPair(x, y);
}

MyError output_rgbvalue(uint32_t bbGGRR00, Output& output)
{
    MyError err;
    
    if ((err = output.numSpace((int32_t)((bbGGRR00 >> 8) & 0xFF))) != nullptr)
        return err;

    if ((err = output.numSpace((int32_t)((bbGGRR00 >> 16) & 0xFF))) != nullptr)
        return err;

    return output.numSpace((int32_t)((bbGGRR00 >> 24) & 0xFF));
}

/* Read the value of a system variable and print it to the output stream.
 * The value must fit in expansionbuffer and will be truncated if it doesn't.
 */
MyError output_variable(const char* name, Output& output)
{
    PDriverWS& ws(PDriverWS::instance());

    int32_t len = sizeof(ws.expansionbuffer);

    int used, context;
    OS::VarType type;
    MyError err = OS::xreadVarVal(name, ws.expansionbuffer, len,
                                  0, OS::VarType_String,
                                  used, context, type);
    if (err)
        return err;

    return output_stringN((const char*)ws.expansionbuffer, used, 32, 126, output);
}

static MyError pdf_begin_obj(JobWS& job, PDF::ObjectId objectId)
{
    MyError err;
    PDF::ObjectTable& objectTable(job.m_pdfDocument.objectTable());

    Output& output(job.output());

    if (!objectTable.setObjectOffset(objectId, output.written()))
        return MyError::OOM();

    if ((err = output.num(objectId.value())) != nullptr)
        return err;

    return output.str(" 0 obj\n");
}

static MyError pdf_end_obj(Output& output)
{
    return output.str("endobj\n");
}

static MyError pdf_output_offset10(uint32_t value, Output& output)
{
    char buf[11];
    for (int i = 9; i >= 0; --i) {
        buf[i] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    buf[10] = '\0';
    return output.str(buf);
}

MyError pdf_begin_document(JobWS& job, const char *title)
{
    Output& output(job.output());

    return output.str("%PDF-1.4\n");
}

MyError pdf_begin_page(const Rect<PDF::Point>& mediaBox,
                       JobWS& job)
{
    PDF::Document& pdf(job.m_pdfDocument);
    PDF::Page& page(pdf.currentPage());

    Output& output(job.output());

    MyError err;
    if ((err = pdf.createNewPage()) != nullptr)
        return err;

    if ((err = pdf_begin_obj(job, page.pageObject().objectId())) != nullptr)
        return err;
    if ((err = output.str("<< /Type /Page /Parent ")) != nullptr)
        return err;
    if ((err = output.num(pdf.pagesRootObject().value())) != nullptr)
        return err;
    if ((err = output.str(" 0 R /MediaBox [")) != nullptr)
        return err;
    if ((err = output.writeCoordSpace(mediaBox.x0)) != nullptr)
        return err;
    if ((err = output.writeCoordSpace(mediaBox.y0)) != nullptr)
        return err;
    if ((err = output.writeCoordSpace(mediaBox.x1)) != nullptr)
        return err;
    if ((err = output.writeCoord(mediaBox.y1)) != nullptr)
        return err;
    if ((err = output.str("] /Contents ")) != nullptr)
        return err;
    if ((err = output.num(page.contentsObject().value())) != nullptr)
        return err;
    if ((err = output.str(" 0 R /Resources ")) != nullptr)
        return err;
    if ((err = output.num(pdf.resourcesObject().value())) != nullptr)
        return err;
    if ((err = output.str(" 0 R >>\n")) != nullptr)
        return err;
    if ((err = pdf_end_obj(output)) != nullptr)
        return err;

    if ((err = pdf_begin_obj(job, page.contentsObject().objectId())) != nullptr)
        return err;
    if ((err = output.str("<< /Length ")) != nullptr)
        return err;
    if ((err = output.num(page.lengthObject().value())) != nullptr)
        return err;
    if ((err = output.str(" 0 R >>\nstream\n")) != nullptr)
        return err;

    page.beginStream(output.written());

    /* Scale OS units to points: 1/180 inch -> 0.4 pt. */
    if ((err = output.str("q\n0.4 0 0 0.4 0 0 cm\n")) != nullptr)
        return err;
    return nullptr;
}

MyError pdf_end_page(JobWS& job)
{
    PDF::Document& pdf(job.m_pdfDocument);
    PDF::Page& page(pdf.currentPage());

    Output& output(job.output());

    if (!page.inPage())
        return nullptr;

    MyError err;
    if ((err = output.str("Q\n")) != nullptr)
        return err;

    size_t streamEnd = output.written();

    page.closeStream(streamEnd);
    if ((err = output.str("endstream\nendobj\n")) != nullptr)
        return err;

    if ((err = pdf_begin_obj(job, page.lengthObject().objectId())) != nullptr)
        return err;
    if ((err = output.num(uint32_t(page.streamLength()))) != nullptr)
        return err;
    if ((err = output.str("\n")) != nullptr)
        return err;
    if ((err = pdf_end_obj(output)) != nullptr)
        return err;

    page.endStream();
    return nullptr;
}

static MyError pdf_output_images(JobWS& job)
{
    PDF::Document& pdf(job.m_pdfDocument);
    PDF::ImageRecord* rec = pdf.firstImage();
    Output& output(job.output());

    while (rec) {
        MyError err;
        if ((err = pdf_begin_obj(job, rec->objectId())) != nullptr)
            return err;
        if ((err = output.str("<< /Type /XObject /Subtype /Image /Width ")) != nullptr)
            return err;
        if ((err = output.num(rec->width())) != nullptr)
            return err;
        if ((err = output.str(" /Height ")) != nullptr)
            return err;
        if ((err = output.num(rec->height())) != nullptr)
            return err;
        if (rec->isImageMask()) {
            if ((err = output.str(" /ImageMask true")) != nullptr)
                return err;
        } else {
            if ((err = output.str(" /ColorSpace ")) != nullptr)
                return err;
            if (rec->colourSpace() == PDF::ImageColourSpace_Gray) {
                err = output.str("/DeviceGray");
            } else if (rec->colourSpace() == PDF::ImageColourSpace_Indexed) {
                if ((err = output.str("[/Indexed /DeviceRGB ")) != nullptr)
                    return err;
                if ((err = output.num(rec->paletteEntries() - 1)) != nullptr)
                    return err;
                if ((err = output.str(" <")) != nullptr)
                    return err;
                const uint8_t* palette = rec->palette();
                for (uint32_t i = 0; i < rec->paletteLength(); ++i) {
                    if ((err = output.hex(palette[i])) != nullptr)
                        return err;
                }
                err = output.str(">]");
            } else {
                err = output.str("/DeviceRGB");
            }
            if (err)
                return err;
        }

        if ((err = output.str(" /BitsPerComponent ")) != nullptr)
            return err;
        if ((err = output.num(rec->bitsPerComponent())) != nullptr)
            return err;

        if (rec->mask().value() != 0) {
            if ((err = output.str(" /Mask ")) != nullptr)
                return err;
            if ((err = output.num(rec->mask().value())) != nullptr)
                return err;
            if ((err = output.str(" 0 R")) != nullptr)
                return err;
        }
        if (rec->softMask().value() != 0) {
            if ((err = output.str(" /SMask ")) != nullptr)
                return err;
            if ((err = output.num(rec->softMask().value())) != nullptr)
                return err;
            if ((err = output.str(" 0 R")) != nullptr)
                return err;
        }

        if (rec->filter() == PDF::ImageFilter_DCT) {
            if ((err = output.str(" /Filter /DCTDecode")) != nullptr)
                return err;
        }
        if ((err = output.str(" /Length ")) != nullptr)
            return err;
        if ((err = output.num(rec->length())) != nullptr)
            return err;
        if ((err = output.str(" >>\nstream\n")) != nullptr)
            return err;
        if ((err = output_bytes(rec->data(), rec->length(), output)) != nullptr)
            return err;
        if ((err = output.str("\nendstream\n")) != nullptr)
            return err;
        if ((err = pdf_end_obj(output)) != nullptr)
            return err;
        rec = rec->next();
    }
    return nullptr;
}

static MyError pdf_output_fonts(JobWS& job)
{
    PDF::Document& pdf(job.m_pdfDocument);
    PDF::FontRecord* rec = pdf.firstFont();
    Output& output(job.output());

    while (rec) {
        MyError err;
        
        if ((err = pdf_begin_obj(job, rec->objectId())) != nullptr)
            return err;
        if ((err = output.str("<< /Type /Font /Subtype /Type1 /BaseFont /")) != nullptr)
            return err;
        if ((err = output.str(rec->name())) != nullptr)
            return err;
        if (rec->usesWinAnsiEncoding()) {
            if ((err = output.str(" /Encoding /WinAnsiEncoding")) != nullptr)
                return err;
        }
        if ((err = output.str(" >>\n")) != nullptr)
            return err;
        if ((err = pdf_end_obj(output)) != nullptr)
            return err;
        rec = rec->next();
    }
    return nullptr;
}

static MyError pdf_output_resources(JobWS& job)
{
    PDF::Document& pdf(job.m_pdfDocument);
    Output& output(job.output());

    MyError err;
    if ((err = pdf_begin_obj(job, pdf.resourcesObject().objectId())) != nullptr)
        return err;

    if ((err = output.str("<< /ProcSet [/PDF /Text /ImageB /ImageC /ImageI]")) != nullptr)
        return err;

    PDF::FontRecord* font = pdf.firstFont();
    if (font) {
        if ((err = output.str(" /Font <<")) != nullptr)
            return err;
        while (font) {
            if ((err = output.str(" /F")) != nullptr)
                return err;
            if ((err = output.num(font->resourceId())) != nullptr)
                return err;
            if ((err = output.str(" ")) != nullptr)
                return err;
            if ((err = output.num(font->objectId().value())) != nullptr)
                return err;
            if ((err = output.str(" 0 R")) != nullptr)
                return err;
            font = font->next();
        }
        if ((err = output.str(" >>")) != nullptr)
            return err;
    }

    PDF::ImageRecord* rec = pdf.firstImage();
    if (rec) {
        if ((err = output.str(" /XObject <<")) != nullptr)
            return err;
        while (rec) {
            if ((err = output.str(" /Im")) != nullptr)
                return err;
            if ((err = output.num(rec->objectId().value())) != nullptr)
                return err;
            if ((err = output.str(" ")) != nullptr)
                return err;
            if ((err = output.num(rec->objectId().value())) != nullptr)
                return err;
            if ((err = output.str(" 0 R")) != nullptr)
                return err;
            rec = rec->next();
        }
        if ((err = output.str(" >>")) != nullptr)
            return err;
    }

    if ((err = output.str(" >>\n")) != nullptr)
        return err;
    return pdf_end_obj(output);
}

MyError pdf_end_document(JobWS& job)
{
    PDF::Document& pdf(job.m_pdfDocument);
    PDF::ObjectTable& objectTable(pdf.objectTable());
    PDF::Page& page(pdf.currentPage());

    if (page.inPage()) {
        MyError err = pdf_end_page(job);
        if (err)
            return err;
    }

    Output& output(job.output());

    MyError err;
    if ((err = pdf_output_images(job)) != nullptr)
        return err;
    if ((err = pdf_output_fonts(job)) != nullptr)
        return err;
    if ((err = pdf_output_resources(job)) != nullptr)
        return err;

    if ((err = pdf_begin_obj(job, pdf.pagesRootObject().objectId())) != nullptr)
        return err;
    if ((err = output.str("<< /Type /Pages /Kids [")) != nullptr)
        return err;
    for (uint32_t i = 0; i < pdf.pageCount(); ++i) {
        if ((err = output.num(pdf.pageAt(i).objectId().value())) != nullptr)
            return err;
        if ((err = output.str(" 0 R ")) != nullptr)
            return err;
    }
    if ((err = output.str("] /Count ")) != nullptr)
        return err;
    if ((err = output.num(pdf.pageCount())) != nullptr)
        return err;
    if ((err = output.str(" >>\n")) != nullptr)
        return err;
    if ((err = pdf_end_obj(output)) != nullptr)
        return err;

    if ((err = pdf_begin_obj(job, pdf.catalogObject().objectId())) != nullptr)
        return err;
    if ((err = output.str("<< /Type /Catalog /Pages ")) != nullptr)
        return err;
    if ((err = output.num(pdf.pagesRootObject().value())) != nullptr)
        return err;
    if ((err = output.str(" 0 R >>\n")) != nullptr)
        return err;
    if ((err = pdf_end_obj(output)) != nullptr)
        return err;

    size_t xrefStart = output.written();

    if ((err = output.str("xref\n0 ")) != nullptr)
        return err;
    uint32_t objCount = objectTable.objectCount();
    if ((err = output.num((objCount + 1))) != nullptr)
        return err;
    if ((err = output.str("\n0000000000 65535 f \n")) != nullptr)
        return err;
    for (uint32_t i = 1; i <= objCount; ++i) {
        if ((err = pdf_output_offset10(objectTable.objectOffset(i), output)) != nullptr)
            return err;
        if ((err = output.str(" 00000 n \n")) != nullptr)
            return err;
    }

    if ((err = output.str("trailer\n<< /Size ")) != nullptr)
        return err;
    if ((err = output.num(objCount + 1)) != nullptr)
        return err;
    if ((err = output.str(" /Root ")) != nullptr)
        return err;
    if ((err = output.num(pdf.catalogObject().value())) != nullptr)
        return err;
    if ((err = output.str(" 0 R >>\nstartxref\n")) != nullptr)
        return err;
    if ((err = output.num(uint32_t(xrefStart))) != nullptr)
        return err;
    if ((err = output.str("\n%%EOF\n")) != nullptr)
        return err;

    return output.flush();
}

/* Subroutine to multiply two single precision signed numbers together and
 * get a double precision result.
 */
void arith_dpmult(int32_t a, int32_t b, int32_t *lo, int32_t *hi) {
    int64_t result = (int64_t)a * (int64_t)b;
    if (lo) {
        *lo = (int32_t)result;
    }
    if (hi) {
        *hi = (int32_t)(result >> 32);
    }
}

// Subroutine to divide a double precision signed number by a single precision
// unsigned number, yielding a single precision signed result. Quotient and
// remainder are calculated on "round to minus infinity" rules.
void arith_dpdivmod(int32_t dividend_lo, int32_t dividend_hi, uint32_t divisor,
                    int32_t *quotient, uint32_t *remainder) {
    int64_t dividend = ((int64_t)dividend_hi << 32) | (uint32_t)dividend_lo;
    int64_t q = dividend / (int64_t)divisor;
    int64_t r = dividend % (int64_t)divisor;

    if (r < 0) {
        r += divisor;
        q -= 1;
    }

    if (quotient) {
        *quotient = (int32_t)q;
    }
    if (remainder) {
        *remainder = (uint32_t)r;
    }
}
