#ifndef RLIBX_FONT_IDENTIFIER_H
#define RLIBX_FONT_IDENTIFIER_H

namespace riscos {
namespace Font {

// Trivial parsing of escaped font identifier strings such as \F...\E...\M....
class Identifier
{
public:
    Identifier(const char* text) : m_text(text) {}

    const char* text() const { return m_text; }

    const char* findField(char letter) const;
    const char* locateName() const;

    // Accept the buffer may be modified by caller (to terminate).
    char* locateName();
    const char* locateAndTerminateName();

    bool fieldEqualsAscii(char letter, const char* text) const;

    int compareByNameEncodingMatrix(const Identifier& other) const;

    static int compareSegmentCaseless(const char* lhs, const char* rhs);

private:
    static char lowercaseAscii(char c);

    const char* m_text;
};

} } // namespace riscos::Font

#endif
