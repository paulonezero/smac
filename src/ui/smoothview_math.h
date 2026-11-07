// The arithmetic of the smooth map view (ui/smoothview.h), apart so that the unit tests reach it.
//
// A view position is in pixels: origin_x * half_w, origin_y * half_h. The tiles sit on a diamond
// grid: a view's origin tile has x and y of the same parity (pfncZoomProcessing), so one step of
// the view is half a tile.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

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

// Edge scrolling as PRACX's: the pointer within `band` pixels of an edge on an axis scrolls that
// way, the nearer the edge the faster: `slow` at the band's inner pixel, `fast` at the screen's
// edge pixel (both in tiles per second). Negative towards the low edge; 0 away from the edges.
inline double edge_rate(int32_t pos, int32_t size, int32_t band, double slow, double fast) {
    if (band <= 0 || pos < 0 || pos >= size) return 0;
    int32_t depth = -1;  // 0 at the band's inner pixel, band - 1 at the screen's edge
    double sign = 1;
    if (pos < band) depth = band - 1 - pos, sign = -1;
    else if (pos >= size - band) depth = pos - (size - band);
    if (depth < 0) return 0;
    return sign * (slow + (fast - slow) * (depth + 1) / band);
}

// A tile at `zoom` is 8 * q pixels wide and 4 * q high (pfncZoomProcessing's q).
inline int32_t tile_q(int32_t zoom) {
    const int32_t q = (50 * (zoom + 16) / 16 + 1) / 4;
    return q < 1 ? 1 : q;
}

// Zoom levels as PRACX's: `count` zooms spaced evenly from the one that shows the whole map (no
// further out than the engine's -14) to the one that shows about six tiles across (no further
// in than its 16), the original's zoom 0 among them, all even as the original's are. `width`
// and `height`: the map's part of the screen; `map_w`, `map_h`: MapAreaX and MapAreaY (a tile
// is two of either). Fewer than `count` where the levels would repeat; none for count < 2.
inline std::vector<int32_t> zoom_levels(int32_t count, int32_t width, int32_t height, int32_t map_w, int32_t map_h) {
    std::vector<int32_t> levels;
    if (count < 2) return levels;
    int32_t lo = 0;
    while (lo > -14 && (4 * tile_q(lo) * map_w > width || 2 * tile_q(lo) * map_h > height)) lo -= 2;
    int32_t hi = 0;
    while (hi < 16 && width / (8 * tile_q(hi)) > 6 && height / (4 * tile_q(hi)) > 3) hi += 2;
    const double step = (double)(hi - lo) / (count - 1);
    for (int32_t i = 0; i < count; i++) {
        const int32_t z = (int32_t)std::lround((lo + step * i) / 2) * 2;
        if (levels.empty() || levels.back() != z) levels.push_back(z);
    }
    if (std::find(levels.begin(), levels.end(), 0) == levels.end()) {
        if (levels.size() < 3) {
            levels.insert(std::upper_bound(levels.begin(), levels.end(), 0), 0);
        } else {  // the inner level nearest to zero becomes it
            auto nearest = std::min_element(levels.begin() + 1, levels.end() - 1,
                                            [](int32_t a, int32_t b) { return std::abs(a) < std::abs(b); });
            *nearest = 0;
        }
    }
    return levels;
}

// Details when zoomed out: the map's drawing code tests its detail thresholds (-8, -10) against
// this instead of the zoom. PRACX writes -14 over those thresholds, so they pass at every zoom
// but -14 itself; here a zoom above -14 counts as no less than -7 for them.
inline int32_t detail_zoom(int32_t zoom, bool details) {
    if (!details || zoom <= -14) return zoom;
    return std::max(zoom, -7);
}

// A zoom key with the levels (not empty): the zoom it leads to from `zoom`, whose nearest level
// is where the view is now. `command` as pfncProcZoomKey's: 0x203 in, 0x204 out, 0x205 the
// original's zoom 0, 0x206 one level out from it (the original's -6), 0x207 the innermost
// level, 0x208 the outermost.
inline int32_t zoom_to(const std::vector<int32_t>& levels, int32_t zoom, int32_t command) {
    const int32_t last = (int32_t)levels.size() - 1;
    int32_t at = 0, zero = 0;
    for (int32_t i = 1; i <= last; i++) {
        if (std::abs(levels[(size_t)i] - zoom) <= std::abs(levels[(size_t)at] - zoom)) at = i;
    }
    for (int32_t i = 0; i <= last; i++) {
        if (levels[(size_t)i] == 0) zero = i;
    }
    switch (command) {
    case 0x203: return levels[(size_t)std::min(at + 1, last)];
    case 0x204: return levels[(size_t)std::max(at - 1, 0)];
    case 0x205: return levels[(size_t)zero];
    case 0x206: return levels[(size_t)std::max(zero - 1, 0)];
    case 0x207: return levels[(size_t)last];
    case 0x208: return levels[0];
    default: return zoom;
    }
}

}  // namespace smoothview
