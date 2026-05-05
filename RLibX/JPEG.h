#pragma once

#include "RLib/RLib.h"

namespace riscos {
namespace JPEG {

enum InfoFlags {
    InfoFlags_InfoMonochrome = 1u << 0,
    InfoFlags_InfoNoTrfm     = 1u << 1,
    InfoFlags_InfoDPIUnknown = 1u << 2
};

enum ScaleFlags {
    ScaleFlags_ScaleDithered           = 1u << 0,
    ScaleFlags_ScaleErrorDiffused      = 1u << 1,
    ScaleFlags_ScaleGivenColourMapping = 1u << 2,
    ScaleFlags_ScaleTranslucency       = 0xFF0u
};

enum PrintFlags {
    PrintFlags_Plotting      = 1u << 0,
    PrintFlags_UsingTransTab = 1u << 1
};

// Minimal container for info from the header, parsed by JPEG_Info.
class Info {
public:
    MyError parse(const void* jpeg, uint32_t len);

    uint32_t  width() const  { return m_width; }
    uint32_t  height() const { return m_height; }
    InfoFlags flags() const  { return m_flags; }
    uint32_t  dpiX() const   { return m_dpiX; }
    uint32_t  dpiY() const   { return m_dpiY; }

    // Amount of extra memory SpriteExtend would need to render this JPEG.
    uint32_t  workspace() const { return m_workspace; }

    bool monochrome() const { return !!(m_flags & InfoFlags_InfoMonochrome); }

private:
    uint32_t    m_width;
    uint32_t    m_height;
    InfoFlags   m_flags;
    uint32_t    m_dpiX;
    uint32_t    m_dpiY;

    uint32_t    m_workspace;
};

MyError infoDimensions(const void* image,
                       int size,
                       InfoFlags& infoFlags,
                       int& width,
                       int& height,
                       int& xdpi,
                       int& ydpi,
                       int& workspaceSize);

MyError plotScaled(const void* image,
                   int x,
                   int y,
                   const int32_t* scale,
                   uint32_t length,
                   ScaleFlags flags);

MyError pdriverIntercept(PrintFlags flags, PrintFlags* flagsOut);

} } // namespace riscos::JPEG
