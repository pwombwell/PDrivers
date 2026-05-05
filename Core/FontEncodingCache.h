#ifndef FONTENCODINGCACHE_H
#define FONTENCODINGCACHE_H

#include "RLibX/Font/ReadDefn.h"

// Called every char(!). Determines if the font handle's changed,
// if not returns the previous is-utf8 flag.
class FontEncodingCache
{
public:
    FontEncodingCache() : m_font(0), m_utf8(false) {}

    MyError lookup(FontHandle font);
    bool isUtf8() const { return m_utf8; }

private:
    FontHandle m_font;
    bool       m_utf8;
};

inline MyError FontEncodingCache::lookup(FontHandle font)
{
    if (m_font == font)
        return nullptr;

    Font::ReadDefn defn;
    MyError err = defn.init(font);
    if (err)
        return err;

    Font::Identifier id = defn.getIdentifier();

    m_utf8 = id.fieldEqualsAscii('E', "utf8");
    m_font = font;

    return nullptr;
}

#endif
