#include "kernel.h"

#include "Draw.h"

#include "swis.h"

namespace riscos {
namespace Draw {

MyError xfill(Draw::Path path,
              uint32_t flags,
              const Matrix* matrix,
              int32_t flatness)
{
    return _swix(Draw_Fill, _INR(0,3),
                 path.components(), flags, matrix, flatness);
}

MyError xstroke(Draw::Path path,
                uint32_t flags,
                const Matrix* matrix,
                int32_t flatness,
                int32_t thickness,
                const Draw::CapJoin* capJoin,
                const Draw::DashPattern* dashPattern)
{
    return _swix(Draw_Stroke, _INR(0,6),
                 path.components(), flags, matrix, flatness, thickness, capJoin, dashPattern);
}

MyError xprocessPath(Draw::Path path,
                     uint32_t flags,
                     const Matrix* matrix,
                     uint32_t flatness,
                     uint32_t reasonOrBuffer,
                     uint32_t& sizeOut)
{
    return _swix(Draw_ProcessPath, _INR(0,5) | _OUT(5),
                 path.components(), flags, matrix, flatness, reasonOrBuffer, sizeOut,
                 &sizeOut);
}

MyError processPath(Draw::Path path,
                    uint32_t flags,
                    const Matrix* matrix,
                    uint32_t flatness,
                    uint32_t thickness,
                    const Draw::CapJoin* capJoin,
                    const Draw::DashPattern* dashPattern,
                    Draw::Path buffer)
{
    return _swix(Draw_ProcessPath, _INR(0,7),
                 path.components(), flags, matrix, flatness, thickness,
                 capJoin, dashPattern, buffer.components());
}

MyError processPathSpecial(Draw::Path path,
                           uint32_t flags,
                           const Matrix* matrix,
                           uint32_t flatness,
                           uint32_t thickness,
                           const Draw::CapJoin* capJoin,
                           const Draw::DashPattern* dashPattern,
                           uint32_t special,
                           uint32_t& sizeOut)
{
    return _swix(Draw_ProcessPath, _INR(0,7) | _OUT(5),
                 path.components(), flags, matrix, flatness, thickness,
                 capJoin, dashPattern, special,
                 &sizeOut);
}

} } // namespace riscos::Draw
