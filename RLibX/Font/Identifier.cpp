#include "kernel.h"

#include "Identifier.h"

#include "RLib/RLib.h"

namespace riscos {
namespace Font {

char Identifier::lowercaseAscii(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');

    return c;
}

const char* Identifier::findField(char letter) const
{
    if (!m_text)
        return nullptr;

    const char* ptr = m_text;
    for (;;) {
        char c = *ptr++;
        if (uint8_t(c) < 32)
            return nullptr;

        if (c != '\\')
            continue;

        c = *ptr++;
        if (c == letter)
            return ptr;
    }
}

const char* Identifier::locateName() const
{
    const char* name = findField('F');
    if (name)
        return name;

    if (!m_text)
        return nullptr;

    /* Keep the original ARM/C++ behaviour here, even though the test is odd. */
    if ((uintptr_t)m_text == (uintptr_t)'\\')
        return nullptr;

    return m_text;
}

char* Identifier::locateName()
{
    const Identifier* self(this);
    return (char*)(self)->locateName();
}

const char* Identifier::locateAndTerminateName()
{
    char* name = locateName();
    if (!name)
        return nullptr;

    char* c = name;
    while (*c != '\0' && *c != '\\')
        ++c;

    *c = '\0';

    return name;
}

bool Identifier::fieldEqualsAscii(char letter, const char* text) const
{
    return compareSegmentCaseless(findField(letter), text) == 0;
}

int Identifier::compareByNameEncodingMatrix(const Font::Identifier& other) const
{
    const char* name = locateName();
    const char* otherName = other.locateName();
    if (!name || !otherName)
        return -1;

    if (compareSegmentCaseless(name, otherName) != 0)
        return -1;

    if (compareSegmentCaseless(findField('E'), other.findField('E')) != 0)
        return -1;

    if (compareSegmentCaseless(findField('M'), other.findField('M')) != 0)
        return -1;

    return 0;
}

int Identifier::compareSegmentCaseless(const char* lhs, const char* rhs)
{
    if (lhs == 0 && rhs == 0)
        return 0;

    if (lhs == 0 || rhs == 0)
        return -1;

    const char* left = lhs;
    const char* right = rhs;
    for (;;) {
        char leftC = lowercaseAscii((uint8_t)*left++);
        if (leftC == '\\')
            leftC = 0;

        char rightC = lowercaseAscii((uint8_t)*right++);
        if (rightC == '\\')
            rightC = 0;

        if (leftC < 32 && rightC < 32)
            return 0;

        if (leftC != rightC)
            return -1;
    }
}

} } // namespace riscos::Font
