#pragma once

#include "RLib/RLib.h"

struct MakePSFont
{
public:
    enum OutputHandle {
        OutputHandle_NoOutput = -1,
        OutputHandle_NameOnly = -2
    };

    enum Flags {
        Flags_RegistryEntries = 1u << 0,
        Flags_NoDownload      = 1u << 1,
        Flags_Transient       = 1u << 2,
        Flags_Kerning         = 1u << 3,
        Flags_UseRFRemap      = 1u << 4
    };

    static MyError xmakeFont(FileHandle outputHandle,
                            const char* fontName,
                            char* returnNameBuffer,
                            uint32_t* bufferSizeInOut,
                            uint32_t flags);
};
