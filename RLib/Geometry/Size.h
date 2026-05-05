#pragma once

#include "RLib/RLib.h"

namespace riscos {
namespace Geometry {

// A Size container.
template<typename T>
struct Size {
    Size() { (void)this; }
    Size(const T width, const T height) : width(width), height(height) { }
    Size(const Size& size) : width(size.width), height(size.height) { }

    T width, height;
};

} } // namespace riscos::Geometry
