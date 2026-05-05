#include "Core/PDriver.h"
#include "Output.h"

#include "JobWS.h"

#include "Core/Job.h"

MyError Output::writeCoord(int32_t value)
{
    return num(value);
}

MyError Output::writeCoord(OS::Unit value)
{
    return num(value.raw());
}

MyError Output::writeCoord(OS::Millipoint value)
{
    return num(value);
}

MyError Output::writeCoord(Draw::Unit value)
{
    return num(value.raw());
}

MyError Output::writeCoord(PS::Unit value)
{
    return num(value.raw());
}

MyError Output::writeCoordSpace(int32_t value)
{
    MyError err = writeCoord(value);
    if (err)
        return err;

    return byte(' ');
}

MyError Output::writeCoordSpace(OS::Unit value)
{
    MyError err = writeCoord(value);
    if (err)
        return err;

    return byte(' ');
}

MyError Output::writeCoordSpace(OS::Millipoint value)
{
    MyError err = writeCoord(value);
    if (err)
        return err;

    return byte(' ');
}

MyError Output::writeCoordSpace(Draw::Unit value)
{
    MyError err = writeCoord(value);
    if (err)
        return err;

    return byte(' ');
}

MyError Output::writeCoordSpace(PS::Unit value)
{
    MyError err = writeCoord(value);
    if (err)
        return err;

    return byte(' ');
}

MyError Output::writeCoordPair(int32_t x, int32_t y)
{
    MyError err = writeCoordSpace(x);
    if (err)
        return err;

    return writeCoordSpace(y);
}

MyError Output::writeCoordPair(OS::Millipoint x, OS::Millipoint y)
{
    MyError err = writeCoordSpace(x);
    if (err)
        return err;

    return writeCoordSpace(y);
}

MyError Output::writeCoordPair(Offset<OS::Millipoint> offset)
{
    MyError err = writeCoordSpace(offset.dx);
    if (err)
        return err;

    return writeCoordSpace(offset.dy);
}

MyError Output::writeCoordPair(Point<OS::Unit> value)
{
    MyError err = writeCoordSpace(value.x);
    if (err)
        return err;

    return writeCoordSpace(value.y);
}

MyError Output::writeCoordPair(Offset<OS::Unit> value)
{
    MyError err = writeCoordSpace(value.dx);
    if (err)
        return err;

    return writeCoordSpace(value.dy);
}

MyError Output::writeCoordPair(Size<OS::Unit> value)
{
    MyError err = writeCoordSpace(value.width);
    if (err)
        return err;

    return writeCoordSpace(value.height);
}

MyError Output::writeCoordPair(Point<Draw::Unit> value)
{
    MyError err = writeCoordSpace(value.x);
    if (err)
        return err;

    return writeCoordSpace(value.y);
}

MyError Output::writeCoordPair(Point<PS::Unit> value)
{
    MyError err = writeCoordSpace(value.x);
    if (err)
        return err;

    return writeCoordSpace(value.y);
}

MyError Output::moveTo(const Draw::Point& point)
{
    MyError err = writeCoordPair(point);
    if (err != nullptr)
        return err;

    return str("M\n");
}

MyError Output::lineTo(const Draw::Point& point)
{
    MyError err = writeCoordPair(point);
    if (err != nullptr)
        return err;

    return str("L\n");
}

MyError Output::bezierTo(const Draw::Point& control1,
                         const Draw::Point& control2,
                         const Draw::Point& endPoint)
{
    MyError err = writeCoordPair(control1);
    if (err != nullptr)
        return err;
    if ((err = writeCoordPair(control2)) != nullptr)
        return err;
    if ((err = writeCoordPair(endPoint)) != nullptr)
        return err;

    return str("B\n");
}

MyError Output::closePath()
{
    return str("Cl\n");
}
