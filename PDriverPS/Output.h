#ifndef PDRIVERPS_OUTPUT_H
#define PDRIVERPS_OUTPUT_H

#include "Common/OutputBuffer.h"
#include "Types.h"

#include "RLib/Draw.h"
#include "RLib/Geometry.h"
#include "RLibX/OS.h"

class CoreWS;
class JobWS;

using namespace riscos::Geometry;

class Output : public OutputBuffer
{
public:
    Output(FileHandle fileHandle) : OutputBuffer(fileHandle) {}

    MyError writeCoord(int32_t value);
    MyError writeCoord(OS::Unit value);
    MyError writeCoord(OS::Millipoint value);
    MyError writeCoord(Draw::Unit value);
    MyError writeCoord(PS::Unit value);

    MyError writeCoordSpace(int32_t value);
    MyError writeCoordSpace(OS::Unit value);
    MyError writeCoordSpace(OS::Millipoint value);
    MyError writeCoordSpace(Draw::Unit value);
    MyError writeCoordSpace(PS::Unit value);

    MyError writeCoordPair(int32_t x, int32_t y);
    MyError writeCoordPair(OS::Millipoint x, OS::Millipoint y);
    MyError writeCoordPair(Offset<OS::Millipoint> value);
    MyError writeCoordPair(Point<OS::Unit> value);
    MyError writeCoordPair(Offset<OS::Unit> value);
    MyError writeCoordPair(Size<OS::Unit> value);
    MyError writeCoordPair(Point<Draw::Unit> value);
    MyError writeCoordPair(Point<PS::Unit> value);

    MyError moveTo(const Draw::Point& point);
    MyError lineTo(const Draw::Point& point);
    MyError bezierTo(const Draw::Point& control1,
                     const Draw::Point& control2,
                     const Draw::Point& endPoint);
    MyError closePath();
};

#endif
