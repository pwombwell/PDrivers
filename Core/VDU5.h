#ifndef CORE_VDU5_H
#define CORE_VDU5_H

#include <stddef.h>
#include <stdint.h>

#include "RLib/Geometry.h"
#include "RLibX/OS.h"

class CoreWS;
class CoreJobWS;

class ScreenVars;
using namespace riscos::Geometry;

class VDU5
{
public:
    // Routine to initialise the VDU 5 information.
    MyError init(CoreJobWS& job);

    // Routine to calculate the character and line advances from the information
    // in 'vdu5charspace' and 'm_cursorControl'.
    void adjustAdvances(uint8_t cursorControl);

    const Size<OS::Unit>& charSize() const { return m_charSize; }
    Size<OS::Unit>& charSize() { return m_charSize; }

    const Offset<OS::Unit>& charSpacing() const { return m_charSpacing; }
    Offset<OS::Unit>& charSpacing() { return m_charSpacing; }

    const Offset<OS::Unit>& charAdvance() const { return m_charAdvance; }
    Offset<OS::Unit>& charAdvance() { return m_charAdvance; }

    const Offset<OS::Unit>& lineAdvance() const { return m_lineAdvance; }
    Offset<OS::Unit>& lineAdvance() { return m_lineAdvance; }

    const Offset<OS::Unit>& autoAdvance() const { return m_autoAdvance; }
    Offset<OS::Unit>& autoAdvance() { return m_autoAdvance; }

private:
    // The current VDU 5 character size and spacing, in OS units
    Size<OS::Unit> m_charSize;
    Offset<OS::Unit> m_charSpacing;

    // The amounts one is currently supposed to move by to advance one VDU 5
    // character position and one VDU 5 character line. Each of these differs from
    // 'vdu5charspace' in that one component is zero and the other may be negated,
    // depending on the setting of 'm_cursorControl'.
    Offset<OS::Unit> m_charAdvance;

    // The amount one is currently supposed to move by to advance one VDU 5 line.
    Offset<OS::Unit> m_lineAdvance;

    // The amount one is currently automatically supposed to advance by after
    // printing a VDU 5 character. Differs from 'vdu5charadvance' in that both
    // components are zero if bit 5 of 'm_cursorControl' is set.
    Offset<OS::Unit> m_autoAdvance;
};

#endif
