#include "PDriver.h"
#include "FontBlock.h"

#include "MsgCode.h"
#include "Workspace.h"

#include <stdlib.h>
#include <string.h>

FontBlock::FontBlock(size_t size, FontBlockWord word)
    : m_size(size)
    , m_word(word)
    , m_alienName(nullptr)
    , m_next(nullptr)
{ }

void* FontBlock::operator new(size_t, size_t totalSize)
{
    return malloc(totalSize);
}

void FontBlock::operator delete(void* p)
{
    free(p);
}

FontBlock* FontBlock::create(const CtrlString& riscos,
                             const CtrlString& alien,
                             FontBlockWord word)
{
    size_t riscosLen = riscos.size();
    size_t alienLen = alien.size();
    size_t total = sizeof(FontBlock) + riscosLen + alienLen;
    total = (total + 3) & ~size_t(3);

    debugLog("fontblock create riscos:%.*s alien:%.*s word:0x%x riscosLen:%u alienLen:%u total:%u",
             riscos.length(), riscos.text(),
             alien.length(), alien.text(),
             word,
             (uint32_t)riscosLen,
             (uint32_t)alienLen,
             (uint32_t)total);

    FontBlock* block = new (total) FontBlock(total, word);
    if (!block)
        return nullptr;

    char* riscosDst = (char*)(block+1);
    if (riscosLen != 0)
        riscos.copyAsNul(riscosDst);

    if (alienLen == 0) {
        block->m_alienName = nullptr;
    } else {
        char* alienDst = riscosDst + riscosLen;
        block->m_alienName = alienDst;
        alien.copyAsNul(alienDst);
    }

    return block;
}

// `selectjob_copyfontsloop`
FontBlock* FontBlock::clone() const
{
    // FontBlock has two strings appended past it.
    FontBlock* block = new (m_size) FontBlock(m_size, m_word);
    if (!block)
        return nullptr;

    size_t dataLen = m_size - sizeof(FontBlock);
    memcpy(block + 1, this + 1, dataLen);

    if (m_alienName == nullptr) {
        block->m_alienName = nullptr;
    } else {
        ptrdiff_t alienOffset = m_alienName - (const char*)this;
        block->m_alienName = ((char*)block) + alienOffset;
    }

    return block;
}

bool FontBlock::matchesRiscosName(const CtrlString& riscosName) const
{
    return CtrlString(riscos()).equalsIgnoreCase(riscosName);
}

static char lowerAscii(char ch)
{
    if (ch >= 'A' && ch <= 'Z')
        return char(ch + ('a' - 'A'));

    return ch;
}

static const char* fontNameField(const char* s)
{
    if (s == nullptr)
        return "";

    if (s[0] == '\\' && s[1] == 'F')
        return s + 2;

    return s;
}

static bool fontIdentifierIsBareName(const char* s)
{
    if (s == nullptr)
        return true;

    while ((uint8_t)*s >= 32) {
        if (*s == '\\')
            return false;
        ++s;
    }

    return true;
}

static bool fontNameFieldEquals(const char* lhs, const char* rhs)
{
    lhs = fontNameField(lhs);
    rhs = fontNameField(rhs);

    for (;;) {
        char lhsCh = *lhs;
        char rhsCh = *rhs;

        if ((uint8_t)lhsCh < 32)
            lhsCh = 0;
        if ((uint8_t)rhsCh < 32)
            rhsCh = 0;

        if (lhsCh == '\\' && lhs[1] == 'E')
            lhsCh = 0;
        if (rhsCh == '\\' && rhs[1] == 'E')
            rhsCh = 0;

        lhsCh = lowerAscii(lhsCh);
        rhsCh = lowerAscii(rhsCh);

        if (lhsCh != rhsCh)
            return false;

        if (lhsCh == 0)
            return true;

        ++lhs;
        ++rhs;
    }
}

// FontBlockList ---------------------------------------------------------------
FontBlockList::~FontBlockList()
{
    removeFonts();
}

FontBlock* FontBlockList::findByRiscosName(const CtrlString& riscosName) const
{
    debugLog("fontlist find riscos:%.*s", riscosName.length(), riscosName.text());
    FontBlock* block = head();
    while (block != nullptr) {
        debugLog("fontlist find compare block:%p riscos:%s alien:%s word:0x%x",
                 block,
                 block->riscos(),
                 block->alien() != nullptr ? block->alien() : "<null>",
                 block->word());
        if (block->matchesRiscosName(riscosName)) {
            debugLog("fontlist find matched block:%p", block);
            return block;
        }

        block = block->next();
    }

    debugLog("fontlist find no match");
    return nullptr;
}

// `font_locatename`
const char* FontBlockList::locateName(Font::Identifier identifier,
                                                      FontBlockWord resourceWord) const
{
    CtrlString identifierString(identifier.text());
    debugLog("fontlist find alien identifier:%.*s resourceWord:0x%x",
             (int)identifierString.length(),
             identifierString.text(),
             resourceWord);
    for (const FontBlock* block = head(); block != nullptr; block = block->next()) {
        debugLog("fontlist alien compare block:%p riscos:%s alien:%s word:0x%x",
                 block,
                 block->riscos(),
                 block->alien() != nullptr ? block->alien() : "<null>",
                 block->word());
        if (block->word() != resourceWord)
            continue;

        const char* local = block->riscos();
        int cmp = Font::Identifier(local).compareByNameEncodingMatrix(identifier);
        CtrlString localString(local);
        debugLog("fontlist alien namecmp local:%.*s cmp:%d",
                 (int)localString.length(),
                 localString.text(),
                 cmp);

        if (cmp != 0) {
            if (!fontIdentifierIsBareName(identifier.text()))
                continue;

            if (!fontNameFieldEquals(local, identifier.text()))
                continue;

            debugLog("fontlist alien bare-name matched local:%.*s identifier:%.*s",
                     (int)localString.length(),
                     localString.text(),
                     (int)identifierString.length(),
                     identifierString.text());
        }

        debugLog("fontlist alien matched alien:%s",
                 block->alien() != nullptr ? block->alien() : "<null>");
        return block->alien();
    }

    debugLog("fontlist alien no match");
    return nullptr;
}

MyError FontBlockList::addFont(const CtrlString& riscosName,
                               const CtrlString& alienName,
                               FontBlockWord word,
                               FontBlockAddFlag addFlags)
{
    debugLog("fontlist add riscos:%.*s alien:%.*s word:0x%x flags:0x%x list:%p",
             riscosName.length(), riscosName.text(),
             alienName.length(), alienName.text(),
             word,
             addFlags,
             this);
    FontBlock* existing = findByRiscosName(riscosName);

    if (existing != nullptr) {
        debugLog("fontlist add duplicate existing:%p riscos:%s alien:%s word:0x%x",
                 existing,
                 existing->riscos(),
                 existing->alien() != nullptr ? existing->alien() : "<null>",
                 existing->word());
        if (!!(addFlags & FontBlockAddFlag_OverwriteExisting)) {
            debugLog("fontlist add overwriting existing:%p", existing);
            // Remove existing entry from from list, and free it.
            remove(existing);
            delete existing;

            // Attempt to add again and hope that it.
            return addFont(riscosName, alienName, word, addFlags);
        }

        MsgCode& msgs(CoreWS::instance().messages);
        return msgs.lookupError(ErrorBlock_PrintNoDuplicates);
    }

    FontBlock* newBlock = FontBlock::create(riscosName, alienName, word);
    if (newBlock == nullptr)
        return MyError::OOM();

    addTail(newBlock);

    debugLog("fontlist add stored block:%p head:%p riscos:%s alien:%s word:0x%x",
             newBlock,
             head(),
             newBlock->riscos(),
             newBlock->alien() != nullptr ? newBlock->alien() : "<null>",
             newBlock->word());

    return nullptr;
}

void FontBlockList::removeFonts()
{
    FontBlock* block;

    while ((block = detachHead()) != nullptr) {
        delete block;
    }
}

// `miscop_enumfonts` (but caller is responsible for determining which list.
void FontBlockList::enumerate(FontBlockEnumerationRecord*& buffer,
                              uint32_t& size, uint32_t& handle) const
{
    debugLog("fontlist enumerate buffer:%p size:%u handle:%u list:%p",
             buffer,
             size,
             handle,
             this);
//    FontBlock* block = List::head(); <- Norcroft doesn't like the shorthand.

    FontBlock* block = Utils::List<FontBlock>::head();
    uint32_t skip = handle;

    // `miscop_enumfontsskip`
    while (skip != 0 && block != nullptr) {
        block = block->next();
        skip--;
    }

    // The assembler returns immediately if asked to skip past the end,
    // leaving handle unchanged.
    if (skip != 0)
        return;

    if (buffer == nullptr) {
        while (block != nullptr) {
            size += sizeof(FontBlockEnumerationRecord);
            handle++;
            block = block->next();
        }

        if (block == nullptr)
            handle = 0;

        return;
    }

    while (block != nullptr) {
        if (size < sizeof(FontBlockEnumerationRecord))
            return;

        debugLog("fontlist enumerate emit block:%p riscos:%s alien:%s word:0x%x sizeBefore:%u handleBefore:%u",
                 block,
                 block->riscos(),
                 block->alien() != nullptr ? block->alien() : "<null>",
                 block->word(),
                 size,
                 handle);
        buffer->riscosName = block->riscos();
        buffer->alienName = block->alien();
        buffer->word = block->word();

        buffer++;
        size -= sizeof(FontBlockEnumerationRecord);
        handle++;
        block = block->next();
    }

    handle = 0;
}

FontBlock*
FontBlockList::head() const
{
    // Norcroft doesn't like the shorthand (see enumerate)
    return Utils::List<FontBlock>::head();
}

// `selectjob_copyfontlist`
// Copy the current global font list and allocate it to the current job,
// this is required so that the global list can be removed and not disturb
// the local one to this job.  Copying the blocks results in them being linked
// in the wrong order.
MyError FontBlockList::duplicate(FontBlockList& other)
{
    debugLog("fontlist duplicate from:%p to:%p", &other, this);
    for (const FontBlock* block = other.head();
         block != nullptr;
         block = block->next())
    {
        debugLog("fontlist duplicate clone source:%p riscos:%s alien:%s word:0x%x",
                 block,
                 block->riscos(),
                 block->alien() != nullptr ? block->alien() : "<null>",
                 block->word());
        FontBlock* clone = block->clone();
        if (clone == nullptr)
            return MyError::OOM();

        addHead(clone);
        debugLog("fontlist duplicate stored clone:%p", clone);
    }

    return nullptr;
}
