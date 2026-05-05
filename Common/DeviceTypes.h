#ifndef COMMON_DEVICETYPES_H
#define COMMON_DEVICETYPES_H

#include "Core/PDriver.h"

#include "RLib/Draw.h"

/*
 * Typed helpers for the Core/Device API.
 *
 * These are deliberately small wrappers around existing integer/pointer
 * contracts, so backends can be migrated incrementally without changing
 * Core/Device.h signatures.
 */

class DevicePixelUnit
{
public:
    DevicePixelUnit() : m_value(0) {}
    /*explicit*/ DevicePixelUnit(int32_t value) : m_value(value) {}

    int32_t raw() const { return m_value; }

private:
    int32_t m_value;
};

class DeviceRgb
{
public:
    DeviceRgb() : m_bbggrr00(0) {}
    /*explicit*/ DeviceRgb(uint32_t bbggrr00) : m_bbggrr00(bbggrr00) {}

    static DeviceRgb fromRgb(uint8_t red, uint8_t green, uint8_t blue)
    {
        uint32_t packed = ((uint32_t)blue << 24) |
                          ((uint32_t)green << 16) |
                          ((uint32_t)red << 8);
        return DeviceRgb(packed);
    }

    uint32_t bbggrr00() const { return m_bbggrr00; }

private:
    uint32_t m_bbggrr00;
};

class DeviceGCOL
{
public:
    DeviceGCOL() : m_value(0) {}
    /*explicit*/ DeviceGCOL(uint32_t value) : m_value(value) {}

    uint32_t raw() const { return m_value; }

private:
    uint32_t m_value;
};

struct DevicePixelValue
{
    uint32_t value;
    uint32_t bytes;

    DevicePixelValue() : value(0), bytes(0) {}
};

struct DeviceScale
{
    int32_t values[4];

    DeviceScale()
    {
        values[0] = 1;
        values[1] = 1;
        values[2] = 1;
        values[3] = 1;
    }

    DeviceScale(int32_t magX, int32_t magY, int32_t divX, int32_t divY)
    {
        values[0] = magX;
        values[1] = magY;
        values[2] = divX;
        values[3] = divY;
    }

    const int32_t* raw() const { return &values[0]; }
};

struct DeviceSourceRect
{
    int32_t lowX;
    int32_t lowY;
    int32_t highX;
    int32_t highY;

    DeviceSourceRect()
        : lowX(0), lowY(0), highX(0), highY(0)
    {}

    DeviceSourceRect(int32_t lowXValue,
                     int32_t lowYValue,
                     int32_t highXValue,
                     int32_t highYValue)
        : lowX(lowXValue)
        , lowY(lowYValue)
        , highX(highXValue)
        , highY(highYValue)
    {}

    const int32_t* raw() const { return &lowX; }
};

/*
 * Destination coordinates for transformed sprite/JPEG plotting.
 * Values are 1/256 OS units, in the order:
 *   X0,Y0, X1,Y1, X2,Y2, X3,Y3
 */
struct DeviceDestCoords
{
    int32_t values[8];

    DeviceDestCoords()
    {
        values[0] = 0;
        values[1] = 0;
        values[2] = 0;
        values[3] = 0;
        values[4] = 0;
        values[5] = 0;
        values[6] = 0;
        values[7] = 0;
    }

    DeviceDestCoords(int32_t x0, int32_t y0,
                     int32_t x1, int32_t y1,
                     int32_t x2, int32_t y2,
                     int32_t x3, int32_t y3)
    {
        values[0] = x0; values[1] = y0;
        values[2] = x1; values[3] = y1;
        values[4] = x2; values[5] = y2;
        values[6] = x3; values[7] = y3;
    }

    const int32_t* raw() const { return &values[0]; }
};

inline const int32_t* device_scale_raw(const DeviceScale* scale)
{
    return scale ? scale->raw() : 0;
}

inline const int32_t* device_source_rect_raw(const DeviceSourceRect* sourceRect)
{
    return sourceRect ? sourceRect->raw() : 0;
}

inline const int32_t* device_dest_coords_raw(const DeviceDestCoords* destCoords)
{
    return destCoords ? destCoords->raw() : 0;
}

inline const int32_t* device_draw_matrix_raw(const Draw::Matrix* matrix)
{
    return matrix ? matrix->raw() : 0;
}

#endif
