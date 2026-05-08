#ifndef PDRIVERPS_PSDOCUMENT_H
#define PDRIVERPS_PSDOCUMENT_H

#include "Core/PDriver.h"

#include "Types.h"

#include "Common/Font.h"
#include "Common/PrinterFontName.h"
#include "RLib/Geometry.h"
#include "RLibX/Utils/EnumBitmaskOps.h"

class UserRectangle;

using namespace riscos::Geometry;

namespace PS {

class FontCatalogue
{
public:
    FontCatalogue() : m_hasDeclaredFonts(false) {}
    ~FontCatalogue();

    DeclaredFontList& declaredFonts() { return m_declaredFonts; }
    const DeclaredFontList& declaredFonts() const { return m_declaredFonts; }

    DeclaredFont* firstDeclaredFont() const { return m_declaredFonts.head(); }
    bool hasDeclaredFonts() const { return m_hasDeclaredFonts; }
    bool hasDeclaredFontEntries() const { return m_declaredFonts.head() != nullptr; }

    void resetDeclaredFonts()
    {
        m_hasDeclaredFonts = true;
        DeclaredFont* font;
        while ((font = m_declaredFonts.detachHead()) != nullptr)
            delete font;
    }

    void addDeclaredFont(DeclaredFont* node)
    {
        m_hasDeclaredFonts = true;
        m_declaredFonts.addHead(node);
    }

    PrinterFontNameEntry& FontNameEntry() { return m_FontNameEntry; }
    const PrinterFontNameEntry& FontNameEntry() const { return m_FontNameEntry; }

    const char* mappedFont(FontHandle handle) const { return m_FontNameEntry.lookup(handle); }
    void setMappedFont(FontHandle handle, const char* psFontName) { m_FontNameEntry.remember(handle, psFontName); }
    void clearMappedFont(FontHandle handle) { m_FontNameEntry.forget(handle); }

private:
    DeclaredFontList m_declaredFonts;
    PrinterFontNameEntry m_FontNameEntry;
    bool             m_hasDeclaredFonts;
};

class Document
{
public:
    // bit 0 set if CTRL-D is to be added to the end of the output.
    // bit 1 set if a verbose prologue is required.
    // bit 2 set if auto-accent generation is required
    // bit 3-7 define target 'level'
    enum Flag {
        Flag_None           = 0,
        // External flags, excluding level.
        Flag_UseCtrlD       = 1u<<0,
        Flag_Verbose        = 1u<<1,
        Flag_Accents        = 1u<<2,

        // These are internal flags.
        Flag_Clipped        = 1u<<3,
        Flag_SendPrologue   = 1u<<4
    };

    enum Level {
        Level_L1, Level_L2, Level_L3
    };

    Document(Flag flags, Level level);

    const UserRectangle* currentRect() const { return m_currentRect; }
    void setCurrentRect(const UserRectangle* rect) { m_currentRect = rect; }
    const UserRectangle* takeCurrentRect();

    void resetBoundingBox();
    void includeBoundingBox(const Rect<Unit>& bounds);
    Rect<Unit> boundingBoxOrEmpty() const;

    bool useCtrlD() const { return !!(m_flags & Flag_UseCtrlD); }
    bool verbose() const { return !!(m_flags & Flag_Verbose); }
    void setVerbose(bool verbose) { setFlag(Flag_Verbose, verbose); }
    bool accents() const { return !!(m_flags & Flag_Accents); }

    bool level1() const { return m_level == Level_L1; }
    Level level() const { return m_level; }

    bool shouldSendPrologue() const { return !!(m_flags & Flag_SendPrologue); }
    void markPrologueSent() { setFlag(Flag_SendPrologue, false); }

    bool clipped() const { return !!(m_flags & Flag_Clipped); }
    void setClipped(bool clipped) { setFlag(Flag_Clipped, clipped); }

    FontCatalogue& fonts() { return m_fonts; }
    const FontCatalogue& fonts() const { return m_fonts; }

private:
    void setFlag(Flag mask, bool set) {
        if (set)
            m_flags = Flag(unsigned(m_flags) | unsigned(mask));
        else
            m_flags = Flag(unsigned(m_flags) & ~unsigned(mask));
    }

private:
    const UserRectangle* m_currentRect;

    // DSC bbox in integer PostScript points.
    // It's a whole-document bbox; the union of every page's bbox.
    // Note: x1/y1 are inclusive.
    Rect<Unit>      m_boundingBox;

    FontCatalogue   m_fonts;

    Flag            m_flags;
    Level           m_level;
};

} // namespace PS
DEFINE_ENUM_BITWISE_OPERATORS(PS::Document::Flag);

#endif
