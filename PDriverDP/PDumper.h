#ifndef PDUMPER_H
#define PDUMPER_H

#pragma once

#include "RLib/RLib.h"
#include "RLibX/Utils/List.h"

enum StripType /* : uint8_t */ {
    StripType_Monochrome    = 0, // monochrome
    StripType_Greyscale     = 1, // grey scale
    StripType_256           = 2, // 256 colour
    StripType_24bppMultiple = 3, // Multiple pass 24 bit colour (RISC OS 3 only)
    StripType_16bppSingle   = 4, // Single pass 16 bit colour (RISC OS 3.5 or later)
    StripType_24bppSingle   = 5  // Single pass 24 bit colour (RISC OS 3.5 or later)
};

enum StripTypeMask {
    StripTypeMask_Monochrome    = (1u << StripType_Monochrome),    // Monochrome         
    StripTypeMask_Greyscale     = (1u << StripType_Greyscale),     // grey scale
    StripTypeMask_256           = (1u << StripType_256),           // 256 colour
    StripTypeMask_24bppMultiple = (1u << StripType_24bppMultiple), // Multiple pass 24 bit colour (RISC OS 3 only)
    StripTypeMask_16bppSingle   = (1u << StripType_16bppSingle),   // Single pass 16 bit colour (RISC OS 3.5 or later)
    StripTypeMask_24bppSingle   = (1u << StripType_24bppSingle)    // Single pass 24 bit colour (RISC OS 3.5 or later)
};

/* PDumper record layout (mirrors PDriverDP/Constants.s). */
class PDumper {
public:
    PDumper* next() const { return m_next; }

    template<size_t N>
    static MyError lookupError(const OS::ErrorBlock<N>& error,
                        const char* param0 = nullptr,
                        const char* param1 = nullptr,
                        const char* param2 = nullptr,
                        const char* param3 = nullptr)
    { // FIXME: Unnecessary fn if ErrorView constructs from ErrorBlock.
        return lookupError((const OS::ErrorView&)error,
                           param0, param1, param2, param3);
    }

private:
    static MyError lookupError(const OS::ErrorView& error,
                               const char* param0, const char* param1,
                               const char* param2, const char* param3);

public:
    uint32_t    number;
    uint32_t    version;
    void*       workspace;
    void*       branch_table;
    uint32_t    branches;
    StripTypeMask striptypemask;

public: // FIXME: Norcroft - can't friend List due to namespaces?
    PDumper* m_next;
};

typedef riscos::Utils::List<PDumper> PDumperList;

#endif
