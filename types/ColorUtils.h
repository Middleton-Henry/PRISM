#pragma once
#include <cmath>
#include <cstdint>
#include "../types/DataTypes.h"

namespace ColorUtils {

inline double SRGBToLinear(uint8_t c) {
    double v = c / 255.0;
    return (v <= 0.04045) ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
}

inline double LabF(double t) {
    constexpr double delta  = 6.0 / 29.0;
    constexpr double delta3 = delta * delta * delta;
    return (t > delta3) ? std::cbrt(t) : t / (3.0 * delta * delta) + 4.0 / 29.0;
}

inline DataTypes::Lab RGBToLab(const DataTypes::RGB& rgb) {
    double r = SRGBToLinear(rgb.r);
    double g = SRGBToLinear(rgb.g);
    double b = SRGBToLinear(rgb.b);

    double X = r * 0.4124564 + g * 0.3575761 + b * 0.1804375;
    double Y = r * 0.2126729 + g * 0.7151522 + b * 0.0721750;
    double Z = r * 0.0193339 + g * 0.1191920 + b * 0.9503041;

    constexpr double Xn = 0.95047, Yn = 1.00000, Zn = 1.08883;

    double fx = LabF(X / Xn);
    double fy = LabF(Y / Yn);
    double fz = LabF(Z / Zn);

    return { 116.0*fy - 16.0, 500.0*(fx - fy), 200.0*(fy - fz) };
}

inline DataTypes::LabA RGBAToLabA(const DataTypes::RGBA& rgba) {
    DataTypes::Lab lab = RGBToLab({rgba.r, rgba.g, rgba.b});
    return { lab.L, lab.a, lab.b, rgba.a / 255.0 };
}

}