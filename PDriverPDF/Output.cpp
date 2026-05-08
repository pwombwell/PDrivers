#include "Core/PDriver.h"
#include "PDriverPDF.h"
#include "Output.h"

#include "GlobalWS.h"
#include "JobWS.h"

MyError Output::writeCoord(int32_t value)
{
    return num(value);
}

MyError Output::writeCoord(Draw::Unit value)
{
    return num(value);
}

MyError Output::writeCoord(OS::Unit value)
{
    return num(value.raw());
}

MyError Output::writeCoord(PDF::Point value)
{
    return writeReal(value.raw(), 1000);
}

MyError Output::writeCoordSpace(int32_t value)
{
    MyError err = writeCoord(value);
    if (err)
        return err;

    return str(' ');
}

MyError Output::writeCoordSpace(Draw::Unit value)
{
    MyError err = writeCoord(value);
    if (err)
        return err;

    return str(' ');
}

MyError Output::writeCoordSpace(OS::Unit value)
{
    MyError err = writeCoord(value);
    if (err)
        return err;

    return str(' ');
}

MyError Output::writeCoordSpace(PDF::Point value)
{
    MyError err = writeCoord(value);
    if (err)
        return err;

    return str(' ');
}

MyError Output::writeCoordPair(int32_t x, int32_t y)
{
    MyError err = writeCoordSpace(x);
    if (err)
        return err;

    return writeCoordSpace(y);
}

MyError Output::writeCoordPair(const Point<Draw::Unit>& value)
{
    MyError err = writeCoordSpace(value.x);
    if (err)
        return err;

    return writeCoordSpace(value.y);
}

MyError Output::writeCoordPair(const Point<OS::Unit>& value)
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

    return str("m\n");
}

MyError Output::lineTo(const Draw::Point& point)
{
    MyError err = writeCoordPair(point);
    if (err != nullptr)
        return err;

    return str("l\n");
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

    return str("c\n");
}

MyError Output::closePath()
{
    return str("h\n");
}

MyError Output::writeReal(int32_t numerator, int32_t denominator)
{
    if (denominator == 0)
        return num(numerator);

    int32_t sign = 0;
    int32_t n = numerator;
    int32_t d = denominator;
    if (n < 0) {
        n = -n;
        sign ^= 1;
    }
    if (d < 0) {
        d = -d;
        sign ^= 1;
    }

    int32_t integer = n / d;
    int32_t rem = n - integer * d;

    MyError err = nullptr;
    if (sign) {
        if ((err = str('-')) != nullptr)
            return err;
    }

    if ((err = num(integer)) != nullptr)
        return err;

    if (rem == 0) {
        return nullptr;
    }

    int32_t frac = (int32_t)((((int64_t)rem * 10000) + (d / 2)) / d);
    if (frac == 0) {
        return nullptr;
    }

    char buf[6];
    buf[0] = '.';
    buf[1] = (char)('0' + (frac / 1000) % 10);
    buf[2] = (char)('0' + (frac / 100) % 10);
    buf[3] = (char)('0' + (frac / 10) % 10);
    buf[4] = (char)('0' + (frac % 10));
    buf[5] = '\0';

    int end = 4;
    while (end > 1 && buf[end] == '0') {
        buf[end] = '\0';
        --end;
    }

    return str(buf);
}

MyError Output::writeRGBcomponent(uint8_t v)
{
    if (v == 255)
        return str('1');
    else if (v == 0)
        return str('0');

    uint32_t scaled = (uint32_t)v * 1000u + 127u;
    scaled /= 255u;

    char buf[6];
    buf[0] = '0';
    buf[1] = '.';
    buf[2] = '0' + (scaled / 100u) % 10u;
    buf[3] = '0' + (scaled / 10u) % 10u;
    buf[4] = '0' + (scaled % 10u);
    buf[5] = '\0';

    return str(buf);
}

MyError Output::writeGrey(uint8_t grey)
{
    MyError err;
    if ((err = writeRGBcomponent(grey)) != nullptr)
        return err;
    if ((err = str(" G\n")) != nullptr)
        return err;
    if ((err = writeRGBcomponent(grey)) != nullptr)
        return err;
    return str(" g\n");
}

MyError Output::writeRGB(uint32_t bbggrr00)
{
    uint8_t red   = (uint8_t)((bbggrr00 >> 8) & 0xFF);
    uint8_t green = (uint8_t)((bbggrr00 >> 16) & 0xFF);
    uint8_t blue  = (uint8_t)((bbggrr00 >> 24) & 0xFF);

    MyError err;
    if ((err = writeRGBcomponent(red)) != nullptr)
        return err;
    if ((err = str(' ')) != nullptr)
        return err;
    if ((err = writeRGBcomponent(green)) != nullptr)
        return err;
    if ((err = str(' ')) != nullptr)
        return err;
    if ((err = writeRGBcomponent(blue)) != nullptr)
        return err;
    if ((err = str(" RG\n")) != nullptr)
        return err;

    if ((err = writeRGBcomponent(red)) != nullptr)
        return err;
    if ((err = str(' ')) != nullptr)
        return err;
    if ((err = writeRGBcomponent(green)) != nullptr)
        return err;
    if ((err = str(' ')) != nullptr)
        return err;
    if ((err = writeRGBcomponent(blue)) != nullptr)
        return err;
    return str(" rg\n");
}

MyError Output::writeRect(const Rect<OS::Unit>& rect)
{
    MyError err;
    if ((err = writeCoordSpace(rect.x0)) != nullptr)
        return err;
    if ((err = writeCoordSpace(rect.y0)) != nullptr)
        return err;
    if ((err = writeCoordSpace(rect.width())) != nullptr)
        return err;
    if ((err = writeCoordSpace(rect.height())) != nullptr)
        return err;

    return str("re\n");
}
