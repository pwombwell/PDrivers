#ifndef PDRIVERPDF_OUTPUT_H
#define PDRIVERPDF_OUTPUT_H

#include "Common/OutputBuffer.h"
#include "Types.h"

#include "RLib/Geometry.h"

class JobWS;
class PDriverWS;

using namespace riscos::Geometry;

class Output : public OutputBuffer
{
public:
    Output(FileHandle fileHandle) : OutputBuffer(fileHandle) {}

    MyError filePosition(uint32_t& position);

    MyError writeCoord(int32_t value);
    MyError writeCoord(Draw::Unit value);
    MyError writeCoord(OS::Unit value);
    MyError writeCoord(PDF::Point value);

    MyError writeCoordSpace(int32_t value);
    MyError writeCoordSpace(Draw::Unit value);
    MyError writeCoordSpace(OS::Unit value);
    MyError writeCoordSpace(PDF::Point value);

    MyError writeCoordPair(int32_t x, int32_t y);
    MyError writeCoordPair(const Point<Draw::Unit>& value);
    MyError writeCoordPair(const Point<OS::Unit>& value);

    MyError writeReal(int32_t numerator, int32_t denominator);
    MyError writeRGBcomponent(uint8_t v);
    MyError writeGrey(uint8_t grey);
    MyError writeRGB(uint32_t bbggrr00);

    MyError writeRect(const Rect<OS::Unit>& rect);

    MyError moveTo(const Draw::Point& point);
    MyError lineTo(const Draw::Point& point);
    MyError bezierTo(const Draw::Point& control1,
                     const Draw::Point& control2,
                     const Draw::Point& endPoint);
    MyError closePath();
};

#endif
