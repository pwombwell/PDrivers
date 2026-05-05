#include "kernel.h"

#include "ColourTrans.h"

#include "swis.h"

namespace riscos {
namespace ColourTrans {

MyError generateTable(OS::Mode sourceMode,
                      const OS::PaletteBase* sourcePalette,
                      OS::Mode destMode,
                      const OS::PaletteBase* destPalette,
                      ColourTrans::Table* transTab,
                      TableFlags flags)
{
    return _swix(ColourTrans_GenerateTable, _INR(0, 5),
                 sourceMode.toNumber(), sourcePalette, destMode.toNumber(), destPalette, transTab,
                 flags);
}

MyError generateTableGetSize(OS::Mode sourceMode,
                             const OS::PaletteBase* sourcePalette,
                             OS::Mode destMode,
                             const OS::PaletteBase* destPalette,
                             TableFlags flags,
                             uint32_t& size)
{
    return _swix(ColourTrans_GenerateTable, _INR(0, 5) | _OUT(4),
                 sourceMode.toNumber(), sourcePalette, destMode.toNumber(), destPalette, 0, flags,
                 &size);
}

MyError returnGCOLForMode(uint32_t paletteEntry,
                          uint32_t mode,
                          uint32_t* palette,
                          uint32_t& gcol)
{
    return _swix(ColourTrans_ReturnGCOLForMode, _INR(0, 3) | _OUT(0),
                 paletteEntry, mode, palette, &gcol);
}

} } // namespace riscos::ColourTrans
