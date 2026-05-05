#include "PDriver.h"
#include "VDU5.h"

#include "Device.h"
#include "Job.h"
#include "OS.h"
#include "Workspace.h"

MyError VDU5::init(CoreJobWS& job)
{
    MyError err = vdu5_flush(job);
    if (err)
        return err;

    debugLog("init 4");
    m_charSize.width = 16;
    m_charSize.height = 32;
    m_charSpacing.dx = 16;
    m_charSpacing.dy = 32;

    job.m_cursorControl = 0x40;
    adjustAdvances(job.m_cursorControl);
    debugLog("init 6");

    return nullptr;
}

// Routine to calculate the character and line advances from the information
// in 'vdu5charspace' and 'm_cursorControl'.
void VDU5::adjustAdvances(uint8_t cursorControl)
{
    // Get defined character spacing.
    Offset<OS::Unit> charAdvance = m_charSpacing;

    // Change signs of spacing to get the sum of the two advances.
    if ((cursorControl & 0x02u) != 0u)
        charAdvance.dx = -charAdvance.dx;

    if ((cursorControl & 0x04u) == 0u)
        charAdvance.dy = -charAdvance.dy;

    Offset<OS::Unit> lineAdvance = charAdvance;

    // Make appropriate movement horizontal and vertical.
    if ((cursorControl & 8) == 0) {
        charAdvance.dy = 0;
        lineAdvance.dx = 0;
    } else {
        charAdvance.dx = 0;
        lineAdvance.dy = 0;
    }

    // Set automatic advance to character advance.
    Offset<OS::Unit> autoAdvance(charAdvance);

    // Then disable automatic advance if required.
    if ((cursorControl & 0x20) != 0u)
        autoAdvance = Offset<OS::Unit>(0, 0);

    m_charAdvance = charAdvance;
    m_lineAdvance = lineAdvance;
    m_autoAdvance = autoAdvance;
}
