#ifndef CORE_FONTBLOCK_H
#define CORE_FONTBLOCK_H

#include <stddef.h>

#include "CtrlString.h"

#include "RLibX/Font/Identifier.h"
#include "RLibX/Utils/List.h"

class CoreWS;

enum FontBlockWord /* uint32_t */ {
    // Opaque "flags" word passed in/out by MiscOp 0 (add) and 2 (enumerate).
    // The Printer Driver core code doesn't interpret the value, it just
    // stores and returns the word it is given.
    //
    // Acorn documented it in the PRM as a flags word, but their only pdriver
    // that uses the font block list is PDriverPS, and both that and !Printers
    // (SupportPS) interpret the word as a Postscript resource type.
    //
    // Since that clashes with the documented flags, this enum is empty
    // and considered opaque. PDrivers are free to interpret it how they wish.
    ForceToWord = 0x7fffffff
};

enum FontBlockAddFlag {
    FontBlockAddFlag_None              = 0,
    FontBlockAddFlag_OverwriteExisting = 1u<<0
};
DEFINE_ENUM_BITWISE_OPERATORS(FontBlockAddFlag);

/* MiscOp add-font block header.
 * The FontBlock is followed immediately by two variable-length strings:
 *  - RISC OS name (control-terminated on input, stored NUL-terminated)
 *  - alien name (stored NUL-terminated)
 */
class FontBlock {
public:
    static FontBlock* create(const CtrlString& riscos,
                             const CtrlString& alien,
                             FontBlockWord word);

    FontBlock* clone() const;
    static void operator delete(void* p);

    const char* riscos() const { return (const char *)this + sizeof(*this); }
    const char* alien() const { return m_alienName; }

    FontBlockWord word() const { return m_word; }
    size_t size() const { return m_size; }

    bool matchesRiscosName(const CtrlString& riscosName) const;

    FontBlock* next() const { return m_next; }
    FontBlock* m_next;

private:
    static void* operator new(size_t objectSize, size_t totalSize);
    FontBlock(size_t size, FontBlockWord flags);

private:
    size_t          m_size;
    FontBlockWord   m_word;
    char*           m_alienName;
};

// Memory layout used by PDriver_MiscOp (2).
struct FontBlockEnumerationRecord {
    const char*     riscosName;
    const char*     alienName;
    FontBlockWord   word;
};

class FontBlockList : private Utils::List<FontBlock>
{
public:
    ~FontBlockList();

    FontBlock* head() const;

    FontBlock* findByRiscosName(const CtrlString& riscosName) const;
    const char* locateName(Font::Identifier identifier,
                                           FontBlockWord resourceWord) const;

    MyError addFont(const CtrlString& riscosName,
                    const CtrlString& alienName,
                    FontBlockWord word,
                    FontBlockAddFlag addFlags);

    void removeFonts();

    void enumerate(FontBlockEnumerationRecord*& buffer,
                   uint32_t& size, uint32_t& handle) const;

    MyError duplicate(FontBlockList& globalList);
};

#endif
