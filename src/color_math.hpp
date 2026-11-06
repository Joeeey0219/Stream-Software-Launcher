#pragma once

#include <cmath>
#include <cstdint>

namespace stream_launcher {

struct Rgb {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

inline Rgb hsv_rgb(double hue, double saturation, double value) {
    const double chroma = value * saturation;
    const double x = chroma * (1.0 - std::abs(std::fmod(hue / 60.0, 2.0) - 1.0));
    const double m = value - chroma;
    double r = 0.0, g = 0.0, b = 0.0;
    if (hue < 60.0) { r = chroma; g = x; }
    else if (hue < 120.0) { r = x; g = chroma; }
    else if (hue < 180.0) { g = chroma; b = x; }
    else if (hue < 240.0) { g = x; b = chroma; }
    else if (hue < 300.0) { r = x; b = chroma; }
    else { r = chroma; b = x; }
    return {static_cast<std::uint8_t>(std::lround((r + m) * 255)),
            static_cast<std::uint8_t>(std::lround((g + m) * 255)),
            static_cast<std::uint8_t>(std::lround((b + m) * 255))};
}

// 32-bit BI_RGB DIB pixels are BGRA bytes in memory (0x00RRGGBB as a DWORD).
inline std::uint32_t dib_pixel(Rgb color) {
    return (static_cast<std::uint32_t>(color.r) << 16) |
           (static_cast<std::uint32_t>(color.g) << 8) |
           static_cast<std::uint32_t>(color.b);
}

}  // namespace stream_launcher
