#ifndef COMMON_PRINTERFONTNAME_H
#define COMMON_PRINTERFONTNAME_H

#include "Core/PDriver.h"
#include "Core/FontBlock.h"

// Norcroft won't let me do 'namespace Font { class Identifier; }'
#include "RLibX/Font/Identifier.h"

struct FontBlockEnumerationRecord;

class PrinterFontResolution
{
public:
    enum Source {
        Source_Cache,
        Source_Verbose,
        Source_Declared
    };

    PrinterFontResolution(const char* name, Source source)
        : m_name(name), m_source(source)
    {}

    PrinterFontResolution()
        : m_name(nullptr),
          m_source(Source_Declared)
    {}

    const char* name() const { return m_name; }
    bool fromCache() const { return m_source == Source_Cache; }
    bool wasDeclared() const { return m_source == Source_Declared; }

private:
    friend class PrinterFontName;

    const char* m_name;
    Source      m_source;
};

class PrinterFontMapping
{
public:
    PrinterFontMapping();

    const char* lookup(FontHandle handle) const { return m_names[handle]; }
    void remember(FontHandle handle, const char* printerFontName) { m_names[handle] = printerFontName; }
    void forget(FontHandle handle) { m_names[handle] = nullptr; }
    void clear();

    PrinterFontResolution resolveForHandle(FontHandle handle,
                                           Font::Identifier identifier,
                                           bool verbose,
                                           FontBlockList& fontList,
                                           FontBlockWord resourceWord,
                                           FontBlockWord declaredWord);

private:
    // Array to map RISCOS font handles to PostScript font names.  The pointers
    // are to entries in the MiscOp list, so we don't free them from here.
    // XXXX This thing consumes a lot of space!  Replace it with a linked list or something.
    const char* m_names[256];
};

class PrinterFontName
{
public:
    static const char* defaultName();
    static const char* nullName();

    static bool equalsIgnoreCase(const char* lhs, const char* rhs);
    static bool containsIgnoreCase(const char* text, const char* needle);
    static bool startsIgnoreCase(const char* text, const char* prefix);
    static bool isPdfSymbolic(const char* name);
    static const char* trimName(const char* name, char* buffer, size_t size);
    static const char* mapToPdfBase14(const char* name, bool* symbolicOut);

private:
    static uint8_t lowercaseAscii(char c);
};

#endif
