#pragma once

#include "RLibX/Utils/List.h"

#include <stdint.h>

struct DeclaredFont {
    DeclaredFont* m_next;
    uint32_t flags;

    char* chars() const { return (char*)this + sizeof(*this); }
};

typedef riscos::Utils::List<DeclaredFont> DeclaredFontList;
