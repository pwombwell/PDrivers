#ifndef PDRIVERDP_JOBWS_H
#define PDRIVERDP_JOBWS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "Common/ColTrans.h"
#include "Core/Job.h"

#include "PDumper.h"

/* PDriverDP.JobWS -> C */

#ifndef max_passes
/* Must match the assembler build configuration. */
#define max_passes 4u
#endif

class PDriverWS;
class PDumper;

namespace DP {

class BufferSprite
{
public:
    BufferSprite() {
        reset();
    }

    void reset() {
        m_name[0] = '\0';
    }

    bool hasName() const { return m_name[0] != '\0'; }

    const char* name() const { return m_name; }
    char* nameBuffer() { return m_name; }

    void copyFrom(const DP::BufferSprite& other) {
        memcpy(m_name, other.m_name, sizeof(m_name));
    }

    Sprite::Selector selector(Sprite::Area* area) const {
        if (area == nullptr)
            return Sprite::Selector(m_name);

        return Sprite::Selector(area, m_name);
    }

private:
    char m_name[13];
};

enum {
    JobStringBytes = 244,
    JobFontCoordBlockBytes = (4 * 4) + (4 * 4),
    JobPathWords = (3 * 3 + 5 * 7 + 1 * 1),
    DashEntries = 64 + 2,
    JobSpriteScaleWords = 8,
    JobSlavedFontBytes = 256,
    JobFontTranslationBytes = 8
};

class JobConfig
{
public:
    JobConfig() : m_pdumperWord(0) { }

    void copyFrom(const PDriverWS& ws);

    StripType stripType() const { return StripType(m_stripType); }
    uint8_t outputBpp() const { return m_outputBpp; }
    uint8_t numPasses() const { return m_numPasses; }   // `job_no_passes`
    uint8_t dumpDepth() const { return m_dumpDepth; }
    uint8_t pass() const { return m_pass; }

    const uint8_t* configBytes() const { return &m_dumpDepth; }
    uint8_t* configBytes() { return &m_dumpDepth; }

    uint32_t& pdumperWord() { return m_pdumperWord; }
    PDumper* pdumperPointer() const { return m_pdumperPointer; }
    uint32_t pdumperNumber() const { return m_pdumperNumber; }
    uint32_t leftMargin() const { return m_leftMargin; }
    void setLeftMargin(uint32_t value) { m_leftMargin = value; }
    void setPass(uint8_t value) { m_pass = value; }

private:
    uint8_t  m_dumpDepth;
    uint8_t  m_interlace;
    uint8_t  m_xInterlace;
    uint8_t  m_passesPerLine;
    uint8_t  m_stripType;  // StripType enum, but 8 bits (norcroft limit)
    uint8_t  m_outputBpp;
    uint8_t  m_numPasses;    // `job_no_passes`
    uint8_t  m_pass;

    uint32_t m_pdumperWord;

    uint8_t  m_strings[DP::JobStringBytes];
    uint32_t m_configureWord;
    PDumper* m_pdumperPointer;
    uint32_t m_pdumperNumber;

    uint32_t m_leftMargin;
};

class OutputState
{
public:
    OutputState()
        : m_sAreaChange(0)
        , m_spriteArea((Sprite::Area*)-1)
        , m_currentBuffer(nullptr)
        , m_vduSaveArea(nullptr)
        , m_currentRect(nullptr)
        , m_hToneAlign(0)
        , m_savedPIgnore(0)
        , m_bufferMarked(false)
    {
        m_savedVduState.unset();
        m_dumpPassImageCount = 0;
        for (uint32_t i = 0; i < max_passes + 1; ++i)
            m_dumpPassImages[i] = nullptr;
    }

    uint8_t halftoneX() const { return m_halftoneX; }
    uint8_t halftoneY() const { return m_halftoneY; }
    void setHalftoneX(uint8_t value) { m_halftoneX = value; }
    void setHalftoneY(uint8_t value) { m_halftoneY = value; }

    uint32_t halftoneWord() const
    {
        return (uint32_t)m_halftoneX |
               ((uint32_t)m_halftoneY << 8) |
               ((uint32_t)m_halftoneCells << 16) |
               ((uint32_t)m_hToneAlign << 24);
    }

    uint8_t savedPrinterIgnore() const { return m_savedPIgnore; }
    void setSavedPrinterIgnore(uint8_t value) { m_savedPIgnore = value; }

    uint32_t& sAreaChange() { return m_sAreaChange; }

    Sprite::Area* spriteArea() const { return m_spriteArea; }
    Sprite::Area*& spriteAreaRef() { return m_spriteArea; }
    void markSpriteAreaUnavailable() { m_spriteArea = (Sprite::Area*)-1; }
    bool hasSpriteAreaSelection() const { return (intptr_t)m_spriteArea != -1; }
    bool ownsSpriteAreaAllocation() const { return (intptr_t)m_spriteArea > 0; }

    DP::BufferSprite& lineBuffer(uint32_t index) { return m_lineBuffer[index]; }
    const DP::BufferSprite& lineBuffer(uint32_t index) const { return m_lineBuffer[index]; }
    DP::BufferSprite*& currentBuffer() { return m_currentBuffer; }
    DP::BufferSprite& rotationBuffer() { return m_rotationBuffer; }
    const DP::BufferSprite& rotationBuffer() const { return m_rotationBuffer; }
    uint8_t** dumpPassImages() { return m_dumpPassImages; }
    uint32_t dumpPassImageCount() const { return m_dumpPassImageCount; }
    void setDumpPassImageCount(uint32_t value) { m_dumpPassImageCount = value; }

    Sprite::VDUContext& savedVduState() { return m_savedVduState; }
    Sprite::VDUSaveArea*& vduSaveArea() { return m_vduSaveArea; }

    UserRectangle* currentRect() const { return m_currentRect; }
    UserRectangle*& currentRectRef() { return m_currentRect; }

    uint32_t currentXScale() const { return m_currentXScale.raw(); }
    uint32_t currentYScale() const { return m_currentYScale.raw(); }
    Base::Fixed<16> currentXScaleNEW() const { return m_currentXScale; }
    Base::Fixed<16> currentYScaleNEW() const { return m_currentYScale; }

    Offset<Draw::Unit> currentOffset() const { return m_currentOffset; }

    Size<OS::Unit> clipSize() const { return m_clipSize; }

    enum RotationId {
        RotationId_None           = 0,      // No rotation, +ve scaling.
        RotationId_SwappedAxes    = 1u<<0,  // Rotate 90 degrees, +ve scaling.
        RotationId_NegativeXScale = 1u<<1,  // X scale is -ve.
        RotationId_NegativeYScale = 1u<<2,  // Y scale is -ve.

        RotationId_Rotate90       = RotationId_SwappedAxes,     // = 1
        RotationId_FlipX          = RotationId_NegativeXScale,  // aka. reflect
        RotationId_FlipY          = RotationId_NegativeYScale,  // aka. reflect
        RotationId_Rotate180      = RotationId_NegativeXScale |
                                    RotationId_NegativeYScale,  // = 6
        RotationId_Rotate270      = RotationId_SwappedAxes |
                                    RotationId_NegativeXScale |
                                    RotationId_NegativeYScale   // = 7
    };

    RotationId rotationId() const { return m_rotationId; }     // `job_rotation_id`
    uint32_t totalHeight() const { return m_totalHeight; }
    uint32_t currentLine() const { return m_currentLine; }
    uint32_t topMargin() const { return m_topMargin; }
    uint32_t lineLength() const { return m_lineLength; }
    uint32_t sprHeadSize() const { return m_sprHeadSize; }
    uint32_t sprBlockSize() const { return m_sprBlockSize; }
    bool bufferMarked() const { return m_bufferMarked; }

    Rect<OS::Millipoint>& printArea() { return m_printArea; }
    const Rect<OS::Millipoint>& printArea() const { return m_printArea; }

    void setRotationId(RotationId value) { m_rotationId = value; }
    void setClipSize(Size<OS::Unit> size) {
        m_clipSize = size;
    }
    void setCurrentScale(int32_t xScale, int32_t yScale)
    {
        m_currentXScale.fromInt(xScale);
        m_currentYScale.fromInt(yScale);
    }
    void setCurrentOffset(int32_t xOffset, int32_t yOffset) {
        m_currentOffset.dx.fromRaw(xOffset);
        m_currentOffset.dy.fromRaw(yOffset);
    }
    void setTotalHeight(uint32_t value) { m_totalHeight = value; }
    void setCurrentLine(int32_t value) { m_currentLine = (uint32_t)value; }
    void setTopMargin(uint32_t value) { m_topMargin = value; }
    void setLineLength(uint32_t value) { m_lineLength = value; }
    void setSprHeadSize(uint32_t value) { m_sprHeadSize = value; }
    void setSprBlockSize(uint32_t value) { m_sprBlockSize = value; }
    void setBufferMarked(bool value) { m_bufferMarked = value; }

private:
    RotationId              m_rotationId;   // `job_rotation_id`

    uint32_t                m_sprHeadSize;
    uint32_t                m_sprBlockSize;

    Rect<OS::Millipoint>    m_printArea;
    uint32_t                m_lineLength; // pixels

    uint8_t                 m_halftoneX;
    uint8_t                 m_halftoneY;
    uint8_t                 m_halftoneCells;
    uint8_t                 m_hToneAlign;

    uint32_t                m_sAreaChange;
    Sprite::Area*           m_spriteArea;

    DP::BufferSprite        m_lineBuffer[max_passes + 2];
    DP::BufferSprite*       m_currentBuffer;
    DP::BufferSprite        m_rotationBuffer;
    uint8_t*                m_dumpPassImages[max_passes + 1];
    uint32_t                m_dumpPassImageCount;

    Size<OS::Unit>          m_clipSize;

    Sprite::VDUContext      m_savedVduState;
    Sprite::VDUSaveArea*    m_vduSaveArea;
    UserRectangle*          m_currentRect;
    uint32_t                m_totalHeight;
    uint32_t                m_currentLine;
    uint32_t                m_topMargin;

    Base::Fixed<16>         m_currentXScale;
    Base::Fixed<16>         m_currentYScale;

    Offset<Draw::Unit>      m_currentOffset;

    uint32_t                m_stepAlong;
    uint8_t                 m_hThorPos;
    uint8_t                 m_hThorPosc;
    uint8_t                 m_stashPass;
    uint8_t                 m_savedPIgnore;
    bool                    m_bufferMarked;
};
DEFINE_ENUM_BITWISE_OPERATORS(OutputState::RotationId);

class FontWorkspace
{
public:
    FontWorkspace() = default;

    uint8_t* chunkBuffer() { return fontChunkData; }
    uint32_t chunkBufferBytes() const { return sizeof(fontChunkData); }

private:
    uint8_t fontChunkData[DP::JobPathWords*4];
};

class PlotWorkspace
{
public:
    PlotWorkspace()
        : m_dashStyle(nullptr)
        , m_pathPtr(m_path)
    {}

    Draw::Path path() { return Draw::Path(m_path); }

    const Draw::DashPattern& dashPattern() { return m_dashPattern; }
    Draw::Unit* dashPatternElements() { return &m_dashPattern.m_element[0]; }
    const Draw::DashPattern* dashStyle() const { return m_dashStyle; }
    Draw::PathComponent*& pathCursor() { return m_pathPtr; }

    void resetPathCursor() { m_pathPtr = m_path; }
    void clearDashStyle() { m_dashStyle = nullptr; }

    void setDashStyle(const Draw::DashPattern* dashStyle) {
        m_dashStyle = dashStyle;
    }

private:
    Draw::PathComponent                 m_path[DP::JobPathWords];
    Draw::SizedDashPattern<DashEntries> m_dashPattern;
    const Draw::DashPattern*            m_dashStyle;

    Draw::PathComponent* m_pathPtr;
};

} // namespace DP

struct JobWS : public CoreJobWS
{
    JobWS(FileHandle file, bool illustration, CoreWS& ws)
        : CoreJobWS(file, illustration, ws)
    {
    }

    DP::JobConfig      m_config;
    DP::OutputState    m_output;

    /* Font coordinate block used at paint time. */
    uint8_t            m_fontCoordBlock[DP::JobFontCoordBlockBytes];
    DP::FontWorkspace  m_fontWS;

    /* Font handling workspace. */
    uint8_t          m_slavedFonts[DP::JobSlavedFontBytes]; // `job_slaved_fonts`
    uint32_t         m_prevFontScaleX;
    uint32_t         m_prevFontScaleY;
    uint8_t          m_fontTranslation[DP::JobFontTranslationBytes];

    uint32_t         m_fontFG;
    uint32_t         m_fontBG;
    uint8_t          m_fontFgGcol;
    uint8_t          m_fontColourOffset;
    uint8_t          m_stashedPassthrough;

    // FontWorkspace, PlotWorkspace and SpriteWorkspare were a union in the
    // original module to save memory. Constructors make this unworkable, but
    // we could remove our constructors, potentially - though memory's cheap
    // (until AI came along).
    DP::PlotWorkspace  m_plotWS;

    // ColourTrans table generated by jpeg_ctrans_handle. `job_spritetrans`
    ColourTrans::Table32K spriteTrans;

    // ColourTrans 32K table. `colourtrans32k`
    GreyTable32K     colourTrans32K;
    // Table type to control allocation and use of the 32K fake colour table.
    StripType           colourTransTableType; // generated table type. 0,1,2 only.


    DP::JobConfig& config() { return m_config; }
    const DP::JobConfig& config() const { return m_config; }

    DP::OutputState& output() { return m_output; }
    const DP::OutputState& output() const { return m_output; }

    DP::FontWorkspace& fontWS() { return m_fontWS; }
    const DP::FontWorkspace& fontWS() const { return m_fontWS; }

    DP::PlotWorkspace& plotWS() { return m_plotWS; }
    const DP::PlotWorkspace& plotWS() const { return m_plotWS; }

    int32_t* fontCoordBlockWords() { return (int32_t *)(void *)m_fontCoordBlock; }
    uint8_t* slavedFonts() { return m_slavedFonts; }
};

inline JobWS& toJobWS(CoreJobWS& coreJobWS)
{
    return (JobWS&)coreJobWS;
}

inline const JobWS& toJobWS(const CoreJobWS& coreJobWS)
{
    return (const JobWS&)coreJobWS;
}

#endif
