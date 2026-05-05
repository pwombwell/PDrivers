#ifndef CORE_CTRLSTRING_H
#define CORE_CTRLSTRING_H

#include "OS.h"

#include <stdlib.h>

// Wrapper for a control-terminated string.
class CtrlString
{
public:
    CtrlString() : m_str(nullptr), m_len(0) {}
    CtrlString(const char* str) : m_str(str) { m_len = calcLength(); }
    CtrlString(const char* str, size_t len) : m_str(str), m_len(len) {}

    const char* text() const { return m_str; }
    bool isNull() const { return m_str == nullptr; }

    size_t length() const { return m_len; }

    // Size includes terminator.
    size_t size() const { return m_str == nullptr ? 0 : length() + 1; }

    void copyAsNul(char* dst) const
    {
        if (m_str == nullptr)
            return;

        const char* s = m_str;
        for (;;) {
            char c = *s++;
            if (uint8_t(c) < 32) {
                *dst = '\0';
                return;
            }

            *dst++ = c;
        }
    }

    char* strdup() const
    {
        if (m_str == nullptr)
            return nullptr;

        char* s = (char*)malloc(size());
        if (s == nullptr)
            return nullptr;

        copyAsNul(s);
        return s;
    }

    bool equalsIgnoreCase(const CtrlString& other) const
    {
        const char* lhs = m_str;
        const char* rhs = other.m_str;

        for (;;) {
            char lhsCh = normalise(lhs);
            char rhsCh = normalise(rhs);
            if (lhsCh != rhsCh)
                return false;

            if (lhsCh == 0)
                return true;
        }
    }

private:
    static char lowercaseAscii(char c) {
        if (c >= 'A' && c <= 'Z')
            return c + ('a' - 'A');

        return c;
    }

    static char normalise(const char*& s) {
        if (s == nullptr)
            return '\0';

        char c = lowercaseAscii(*s++);
        if (uint8_t(c) < 32)
            return '\0';

        return c;
    }

    uint32_t calcLength() const {
        if (m_str == nullptr)
            return 0;

        uint32_t length = 0;
        while (uint8_t(m_str[length]) >= 32)
            length++;

        return length;
    }

private:
    const char* m_str;
    size_t      m_len;
};

#endif
