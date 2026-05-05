#pragma once

#include "RLib/RLib.h"
#include "RLib/Draw.h"

namespace riscos {
namespace Draw {

using riscos::MyError;
using namespace riscos::Draw;

MyError xfill(Draw::Path path,
              uint32_t flags,
              const Matrix* matrix,
              int32_t flatness);

MyError xstroke(Draw::Path path,
                uint32_t flags,
                const Matrix* matrix,
                int32_t flatness,
                int32_t thickness,
                const Draw::CapJoin* capJoin,
                const Draw::DashPattern* dash);

MyError processPath(Draw::Path path,
                    uint32_t flags,
                    const Matrix* matrix,
                    uint32_t flatness,
                    uint32_t thickness,
                    const Draw::CapJoin* capJoin,
                    const Draw::DashPattern* dashPattern,
                    Draw::Path buffer);

// Golly, this needs to be improved.
MyError processPathSpecial(Draw::Path path,
                           uint32_t flags,
                           const Matrix* matrix,
                           uint32_t flatness,
                           uint32_t thickness,
                           const Draw::CapJoin* capJoin,
                           const Draw::DashPattern* dashPattern,
                           uint32_t special,
                           uint32_t& sizeOut);

} } // namespace riscos::Draw
