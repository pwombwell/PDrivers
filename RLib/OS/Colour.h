#pragma once
#ifndef RLIB_OS_COLOUR_H
#define RLIB_OS_COLOUR_H

namespace riscos {
namespace OS {

typedef unsigned char GCOL;

enum Colour {
    Colour_Black        =  0
};

struct ColourPair {
    Colour on;
    Colour off;
};

typedef OS::ColourPair PaletteBase;

} } // namespace riscos::OS

#endif
