#include "Core/PDriver.h"
#include "Output.h"

#include "JobWS.h"

#include "Core/Job.h"

#include "RLib/OS/CharInput.h"

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

MyError Output::writeCoordPair(Draw::Unit x, Draw::Unit y)
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

MyError Output::writeCoordPair(Point<OS::Millipoint> offset)
{
    MyError err = writeCoordSpace(offset.x);
    if (err)
        return err;

    return writeCoordSpace(offset.y);
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

/* Output a string of printable characters as a PostScript string, taking care:
 *   (a) not to produce lines of more than 72 characters.
 *   (b) to output characters outside the range ASCII 32-126 as octal escape
 *       sequences.
 *   (c) to output "(", ")" and "\\" as escape sequences.
 */
MyError Output::psString_dir(const uint8_t* s, size_t len, int step)
{
    MyError err;
    uint32_t linecount = 1;

    const uint8_t* p = s;
    if (step < 0 && len != 0)
        p = s + len - 1;

    if ((err = str('(')) != nullptr)
        return err;

    while (len-- > 0) {
        if (linecount >= 68) {
            if ((err = str('\\')) != nullptr)
                return err;

            if ((err = str('\n')) != nullptr)
                return err;

            linecount = 0;
#if PSDebugEscapes
            int escaped = 0;
            if ((err = readescapestate(&escaped)) != nullptr)
                return err;
#else
            bool escaped = OS::readEscapeState();
#endif

            if (escaped)
                return ErrorBlock_Escape;
        }

        uint8_t c = *p;
        p += step;

        if (c < ' ' || c >= 126) {
            linecount += 3;

            if ((err = str('\\')) != nullptr)
                return err;

            if ((err = num((uint8_t)('0' + (c >> 6)))) != nullptr)
                return err;

            if ((err = num((uint8_t)('0' + ((c >> 3) & 0x7)))) != nullptr)
                return err;

            c = (uint8_t)('0' + (c & 0x7));
            linecount += 1;

            if ((err = byte(c)) != nullptr)
                return err;

            continue;
        }

        if (c == '\\' || c == '(' || c == ')') {
            linecount += 1;

            if ((err = str('\\')) != nullptr)
                return err;
        }

        linecount += 1;

        if ((err = str(c)) != nullptr)
            return err;
    }

    if ((err = str(')')) != nullptr)
        return err;

    return str('\n');
}

MyError Output::psString(const uint8_t* str, size_t len)
{
    return psString_dir(str, len, 1);
}

MyError Output::psStringBackwards(const uint8_t* str, size_t len)
{
    return psString_dir(str, len, -1);
}
