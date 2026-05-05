#include "PDriverInfo.h"

#include "CtrlString.h"
#include "MsgCode.h"

#include "RLibX/Utils/String.h"

#include <stdlib.h>

PDriverInfo::PDriverInfo(const PDriverInfo& rhs)
    : m_pixelResX(rhs.m_pixelResX)
    , m_pixelResY(rhs.m_pixelResY)
    , m_features(rhs.m_features)
    , m_printerName(nullptr)
    , m_htoneResX(rhs.m_htoneResX)
    , m_htoneResY(rhs.m_htoneResY)
    , m_printer(rhs.m_printer)
{
}

PDriverInfo::~PDriverInfo()
{
    free(m_printerName);
}

PDriverInfo& PDriverInfo::operator=(const SetPDriverInfo& rhs)
{
    m_pixelResX = rhs.pixelResX;
    m_pixelResY = rhs.pixelResY;
    m_features = rhs.features;
    m_htoneResX = rhs.htoneResX;
    m_htoneResY = rhs.htoneResY;
    m_printer = rhs.printer;

    setName(rhs.printerName);

    return *this;
}

const char*
PDriverInfo::setName(const char* name)
{
    char* newName = CtrlString(name).strdup(); // returns null if given null.

    free(m_printerName);
    m_printerName = newName;

    return m_printerName;
}

uint32_t
PDriverInfo::setNameToNone(MsgCode& msgs)
{
    if (m_printerName != nullptr) {
        free(m_printerName);
        m_printerName = nullptr;
    }

    const char* result;
    uint32_t len;
    if (!msgs.lookupToken("none", result, len))
        return 0;

    m_printerName = CtrlString(result, len).strdup();
    if (m_printerName == nullptr)
        return 0;

    return len;
}
