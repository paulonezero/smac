// The arithmetic of the smooth map view (ui/smoothview.h), apart so that the unit tests reach it.
//
// A view position is in pixels: origin_x * half_w, origin_y * half_h. The tiles sit on a diamond
// grid: a view's origin tile has x and y of the same parity (pfncZoomProcessing), so one step of
// the view is half a tile.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace smoothview {

struct Pos {
    double x = 0, y = 0;
};

// A position as the view's origin tile and what is left over in pixels: 0 <= px < 2 * half_w,
// 0 <= py < half_h. The picture is drawn at (-px, -py) from that origin.
struct Split {
    int32_t ox = 0, oy = 0, px = 0, py = 0;
};

inline int32_t floor_div(int32_t a, int32_t b) {
    int32_t q = a / b;
    return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q;
}

inline Split split(Pos p, int32_t half_w, int32_t half_h) {
    const int32_t x = (int32_t)std::lround(p.x), y = (int32_t)std::lround(p.y);
    Split s;
    s.oy = floor_div(y, half_h);
    s.ox = floor_div(x, half_w);
    if ((s.ox ^ s.oy) & 1) s.ox--;
    s.px = x - s.ox * half_w;
    s.py = y - s.oy * half_h;
    return s;
}

// The way from a to b on an axis that wraps every `span` pixels (0: it does not).
inline double shortest(double a, double b, double span) {
    double d = b - a;
    if (span <= 0) return d;
    d = std::fmod(d, span);
    if (d > span / 2) d -= span;
    else if (d < -span / 2) d += span;
    return d;
}

// Fast at first, settling gently on the target: f is the time done, 0..1.
inline double ease(double f) {
    f = std::clamp(f, 0.0, 1.0);
    const double r = 1.0 - f;
    return 1.0 - r * r * r;
}

// How long a glide lasts, by its length against the screen's: short moves are quick.
inline int32_t glide_ms(double distance, double screen) {
    if (screen <= 0) return 120;
    return (int32_t)std::clamp(120.0 + 160.0 * distance / screen, 120.0, 300.0);
}

// Whether a glide is worth it: further than this the view just appears there.
inline bool glide_too_far(double distance, double screen) { return distance > screen * 3.0; }

// A change of zoom: the view position that keeps the map point at screen position `anchor` there.
// A map coordinate X is at screen (X - 1) * half - position (pfncTileToPoint), on each axis.
inline double zoom_axis(double pos, int32_t half0, int32_t half1, double anchor) {
    const double map = (anchor + pos) / half0 + 1;
    return (map - 1) * half1 - anchor;
}

// One frame of a scaling picture: the source pixel shown at destination `dst` when the picture is
// scaled by `scale` about `fixed`, clamped to the source.
inline int32_t scaled_source(int32_t dst, double fixed, double scale, int32_t size) {
    const int32_t i = (int32_t)std::floor(fixed + (dst + 0.5 - fixed) / scale);
    return std::clamp(i, 0, size - 1);
}

// Two-finger scrolls add up to zoom steps: *acc collects the wheel's y (fractions on a trackpad)
// and each whole `step` of it is one zoom step, returned as +1 (in) or -1 (out) per call, the
// rest kept. direction: 1 (a scroll up zooms in), -1 (the reverse), 0 (no zoom by the wheel).
inline int32_t wheel_zoom(double* acc, double dy, double step, int32_t direction) {
    if (direction == 0 || step <= 0) {
        *acc = 0;
        return 0;
    }
    *acc += dy * direction;
    if (*acc >= step) {
        *acc -= step;
        return 1;
    }
    if (*acc <= -step) {
        *acc += step;
        return -1;
    }
    return 0;
}

// The mouse against a screen edge on one axis: -1 at the low edge, 1 at the high one, else 0.
inline int32_t edge_axis(int32_t pos, int32_t size, int32_t band) {
    if (pos < band) return -1;
    if (pos >= size - band) return 1;
    return 0;
}

}  // namespace smoothview
