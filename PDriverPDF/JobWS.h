#ifndef PDRIVERPDF_JOBWS_H
#define PDRIVERPDF_JOBWS_H

#include <stdint.h>

#include "PDriverPDF.h"
#include "FontChunkPainter.h"
#include "Output.h"
#include "PDF/Document.h"

#include "Common/Ascii85.h"
#include "Common/ColTrans.h"
#include "Common/ColourPixelValues.h"
#include "Common/GraphicsContext.h"
#include "Common/Font.h"
#include "Common/PrinterFontName.h"
#include "Core/Job.h"

class ColourJob;

struct JobWS : public CoreJobWS
{
public:
    JobWS(FileHandle file, bool illustration, CoreWS& ws)
        : CoreJobWS(file, illustration, ws)
        , m_output(file)
        , m_chunkPainter(*this)
    { }
    virtual ~JobWS(); // Norcroft warns if not explicitly virtual.

    PDFFontChunkPainter& chunkPainter() { return m_chunkPainter; }
    const PDFFontChunkPainter& chunkPainter() const { return m_chunkPainter; }

    PrinterFontNameEntry& FontNameEntry() { return m_FontNameEntry; }
    const PrinterFontNameEntry& FontNameEntry() const { return m_FontNameEntry; }

    const char* mappedFont(FontHandle handle) const { return m_FontNameEntry.lookup(handle); }
    void setMappedFont(FontHandle handle, const char* psFontName) { m_FontNameEntry.remember(handle, psFontName); }

    void clearMappedFont(FontHandle handle) { m_FontNameEntry.forget(handle); }
    void clearMappedFonts() { m_FontNameEntry.clear(); }

    Output& output() { return m_output; }

public:
    UserRectangle *currentrect;

#if PSCoordSpeedUps
    uint32_t realcolour;
#endif

    GraphicsContextStack m_context;

    /* Start position of current VDU 5 string. */
    Point<OS::Unit> startofvdu5str;

    /* VDU 5 chars defined table. */
    uint32_t vducharsdefined[7];
    uint8_t vduchardata[256][8];

#if PSCoordSpeedUps
    uint32_t coordsystem;
    int32_t coordscaleX;
    int32_t coordscaleY;
#endif

    /* Declared fonts list head. */
    Utils::List<DeclaredFont> declaredfonts;

    /* Job flags. */
    bool jobverbose;
    bool jobaccents;
    bool    jobclipped;
    uint8_t jobbaseclip;
    uint8_t sendprologue;

    /* Source clipping offsets. */
    int32_t sourceclip_x;
    int32_t sourceclip_y;

    PDF::Document m_pdfDocument;

private:
    // Attempt to block copy, Norcroft style.
    JobWS(const JobWS&);
    JobWS& operator=(const JobWS&);

private:
    Output              m_output;
    PrinterFontNameEntry  m_FontNameEntry;
    PDFFontChunkPainter m_chunkPainter;
};

class ColourJob : public JobWS
{
public:
    ColourJob(FileHandle file, bool illustration, CoreWS& ws)
        : JobWS(file, illustration, ws)
    { }

    uint32_t lookupPixelValue(uint8_t value) const { return m_pixelValues.get(value); }
    uint8_t  rgbToPixelValue(uint32_t rgb) { return m_pixelValues.rgbToPixelValue(rgb); }

private:
    ColourPixelValues m_pixelValues;

private:
    ColourJob(const ColourJob&);
    ColourJob& operator=(const ColourJob&);
};



inline JobWS& toJobWS(CoreJobWS& coreJobWS)
{
    return (JobWS&)coreJobWS;
}

inline const JobWS& toJobWS(const CoreJobWS& coreJobWS)
{
    return (const JobWS&)coreJobWS;
}

// Norcroft says it's ambiguous if these take CoreJobWS...
inline ColourJob& toColourJob(JobWS& jobWS)
{
    return (ColourJob&)jobWS;
}

inline const ColourJob& toColourJob(const JobWS& jobWS)
{
    return (const ColourJob&)jobWS;
}

#endif
