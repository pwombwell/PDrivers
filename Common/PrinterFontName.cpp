#include "Core/PDriver.h"
#include "PrinterFontName.h"

#include "Core/FontBlock.h"
#include "Core/OS.h"

#include "RLibX/Font/Identifier.h"

#include <string.h>

static const char defaultFont[] = "Courier";
static const char nullString[] = "XXXX";

PrinterFontMapping::PrinterFontMapping()
{
    clear();
}

void PrinterFontMapping::clear()
{
    memset(m_names, 0, sizeof(m_names));
}

// `font_paintchunk_scanlist`
PrinterFontResolution PrinterFontMapping::resolveForHandle(FontHandle handle,
                                                           Font::Identifier identifier,
                                                           bool verbose,
                                                           FontBlockList& fontList,
                                                           FontBlockWord resourceWord,
                                                           FontBlockWord declaredWord)
{
    debugLog("resolve handle:%u identifier:%s resourceWord:0x%x declaredWord:0x%x",
             handle,
             identifier.text(),
             resourceWord,
             declaredWord);

    // is it known?
    const char* name = lookup(handle);
    if (name) { // yes, so just output it.
        debugLog("resolve cache handle:%u name:%s", handle, name);
        return PrinterFontResolution(name, PrinterFontResolution::Source_Cache);
    }

    // Font handle not known; we'll have to do a little more work.

    // If we're not using verbose prologue then go ahead and scan the list.
    if (verbose) {
        // Verbose prologue.  Skip past the \F (if present) and put a NUL over the next \ (if present).
        // We are working in the expansionbuffer (still pointed to by r1).
        // The font mapping scheme is NOT USED under verbose prologue!
        name = identifier.locateAndTerminateName();

        if (name == nullptr)
            name = PrinterFontName::defaultName();

        return PrinterFontResolution(name, PrinterFontResolution::Source_Verbose);
    }

    // Scan the fontlist for an exact match of the
    // font name found in expansionbuffer (identifier - actually on stack).
    // Output the corresponding foreign font name.

    // `font_paintchunk_scanlist`

    // equiv to SWI XPDriver_MiscOp.
    name = fontList.locateName(identifier, resourceWord);

    if (name == nullptr)
        name = PrinterFontName::defaultName(); // failure!

    debugLog("resolve declared handle:%u identifier:%s name:%s",
             handle,
             identifier.text(),
             name);

    // preserve next handle
    remember(handle, name);

    (void)fontList.addFont(name,
                           PrinterFontName::nullName(),
                           declaredWord,
                           FontBlockAddFlag_None);

    return PrinterFontResolution(name, PrinterFontResolution::Source_Declared);
}

uint8_t PrinterFontName::lowercaseAscii(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');

    return c;
}

const char* PrinterFontName::defaultName()
{
    return defaultFont;
}

const char* PrinterFontName::nullName()
{
    return nullString;
}

bool PrinterFontName::equalsIgnoreCase(const char* lhs, const char* rhs)
{
    if (lhs == nullptr || rhs == nullptr)
        return false;

    while (*lhs != '\0' && *rhs != '\0') {
        uint8_t left = lowercaseAscii(*lhs++);
        uint8_t right = lowercaseAscii(*rhs++);
        if (left != right)
            return false;
    }

    return *lhs == '\0' && *rhs == '\0';
}

bool PrinterFontName::containsIgnoreCase(const char* text, const char* needle)
{
    if (text == nullptr || needle == nullptr || *needle == '\0')
        return false;

    for (const char* haystack = text; *haystack != '\0'; ++haystack) {
        const char* lhs = haystack;
        const char* rhs = needle;
        while (*lhs != '\0' && *rhs != '\0') {
            uint8_t left = lowercaseAscii(*lhs++);
            uint8_t right = lowercaseAscii(*rhs++);
            if (left != right)
                break;
        }

        if (*rhs == '\0')
            return true;
    }

    return false;
}

bool PrinterFontName::startsIgnoreCase(const char* text, const char* prefix)
{
    if (text == nullptr || prefix == nullptr)
        return false;

    while (*prefix != '\0') {
        uint8_t lhs = lowercaseAscii(*text++);
        uint8_t rhs = lowercaseAscii(*prefix++);
        if (lhs != rhs)
            return false;
    }

    return true;
}

bool PrinterFontName::isPdfSymbolic(const char* name)
{
    return equalsIgnoreCase(name, "Symbol") ||
           equalsIgnoreCase(name, "ZapfDingbats");
}

const char* PrinterFontName::trimName(const char* name, char* buffer, size_t size)
{
    if (name == nullptr || size == 0)
        return defaultName();

    size_t out = 0;
    if (*name == '/')
        ++name;

    while ((unsigned char)*name >= 32 && *name != '\\' && out + 1 < size) {
        char c = *name++;
        if (c == '*')
            continue;

        buffer[out++] = c;
    }

    buffer[out] = '\0';
    if (out == 0)
        return defaultName();

    return buffer;
}

const char* PrinterFontName::mapToPdfBase14(const char* name, bool* symbolicOut)
{
    bool bold = containsIgnoreCase(name, "bold") ||
                containsIgnoreCase(name, "demi");
    bool italic = containsIgnoreCase(name, "italic") ||
                  containsIgnoreCase(name, "oblique") ||
                  containsIgnoreCase(name, "slanted");

    if (startsIgnoreCase(name, "Trinity") ||
        startsIgnoreCase(name, "Times")) {
        if (bold && italic)
            return "Times-BoldItalic";
        if (bold)
            return "Times-Bold";
        if (italic)
            return "Times-Italic";
        return "Times-Roman";
    }

    if (startsIgnoreCase(name, "Homerton") ||
        startsIgnoreCase(name, "Helvetica")) {
        if (bold && italic)
            return "Helvetica-BoldOblique";
        if (bold)
            return "Helvetica-Bold";
        if (italic)
            return "Helvetica-Oblique";
        return "Helvetica";
    }

    if (startsIgnoreCase(name, "Corpus") ||
        startsIgnoreCase(name, "Courier")) {
        if (bold && italic)
            return "Courier-BoldOblique";
        if (bold)
            return "Courier-Bold";
        if (italic)
            return "Courier-Oblique";
        return "Courier";
    }

    if (startsIgnoreCase(name, "Selwyn") ||
        startsIgnoreCase(name, "ZapfDingbats")) {
        if (symbolicOut != nullptr)
            *symbolicOut = true;

        return "ZapfDingbats";
    }

    if (startsIgnoreCase(name, "Sidney") ||
        startsIgnoreCase(name, "Symbol")) {
        if (symbolicOut != nullptr)
            *symbolicOut = true;

        return "Symbol";
    }

    return name;
}
