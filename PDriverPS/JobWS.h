#ifndef PDRIVERPS_JOBWS_H
#define PDRIVERPS_JOBWS_H

#include <stdint.h>

#include "PDriverPS.h"
#include "Document.h"
#include "FontChunkPainter.h"
#include "Output.h"

#include "Common/Ascii85.h"
#include "Common/ColTrans.h"
#include "Common/ColourPixelValues.h"
#include "Common/GraphicsContext.h"
#include "Common/Font.h"
#include "Common/FontColours.h"
#include "Core/Job.h"

/* PDriverPS.JobWS -> C */

class PDriverWS;
class ColourJob;

struct JobWS : public CoreJobWS
{
public:
    JobWS(FileHandle file, bool illustration, CoreWS& ws)
        : CoreJobWS(file, illustration, ws)
        , m_output(file)
        , m_document(PS::Document::Flag_None, PS::Document::Level_L1)
        , m_chunkPainter(*this)
    {
    }

    PS::Document& document() { return m_document; }
    const PS::Document& document() const { return m_document; }

    Output& output() { return m_output; }

    PSFontChunkPainter& chunkPainter() { return m_chunkPainter; }
    const PSFontChunkPainter& chunkPainter() const { return m_chunkPainter; }



public:
#if PSCoordSpeedUps
    uint32_t realcolour;
#endif

    GraphicsContextStack m_context;

    /* Start position of current VDU 5 string. */
    Point<OS::Unit> startofvdu5str;

    // Which VDU 5 character definitions have been sent to the PostScript
    // interpreter and are still up to date. Character C is in this state if and
    // only if bit (C MOD 32) is set in word (C DIV 32)-1 in the following array
    // of seven words, or equivalently if bit (C MOD 8) is set in byte
    // (C DIV 8)-4.
    uint8_t vducharsdefined[7*4];

#if PSCoordSpeedUps
    uint32_t coordsystem;
    int32_t coordscaleX;
    int32_t coordscaleY;
#endif

private:
    Output              m_output;
    PS::Document        m_document;
    PSFontChunkPainter  m_chunkPainter;
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
};





inline JobWS& toJobWS(CoreJobWS& coreJobWS)
{
    return (JobWS&)coreJobWS;
}

inline const JobWS& toJobWS(const CoreJobWS& coreJobWS)
{
    return (const JobWS&)coreJobWS;
}

inline ColourJob& toColourJob(JobWS& jobWS)
{
    return (ColourJob&)jobWS;
}

inline const ColourJob& toColourJob(const JobWS& jobWS)
{
    return (const ColourJob&)jobWS;
}

#endif
