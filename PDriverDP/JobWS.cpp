#include "Core/PDriver.h"
#include "JobWS.h"

#include "GlobalWS.h"

#include <string.h>

void DP::JobConfig::copyFrom(const PDriverWS& ws)
{
    m_dumpDepth = ws.printer_dump_depth;
    m_interlace = ws.printer_interlace;
    m_xInterlace = ws.printer_x_interlace;
    m_passesPerLine = ws.printer_passes_per_line;
    m_stripType = ws.printer_strip_type;
    m_outputBpp = ws.printer_output_bpp;
    m_numPasses = ws.printer_no_passes;
    m_pass = ws.printer_pass;

    m_pdumperWord = 0;

    memcpy(m_strings, ws.printer_strings, sizeof(m_strings));
    m_configureWord = ws.printer_configureword;
    m_pdumperPointer = ws.printer_pdumper_pointer;
    m_pdumperNumber = ws.printer_pdumper_number;
    m_leftMargin = ws.dummy_leftmargin;
}
