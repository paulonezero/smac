// The arithmetic of the modern unit animations (ui/modern.h): pure functions, unit tested
// (tests/unit/test_modern.cpp).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace modern {

// The way along one tile's move at `t` (0..1 of its time): a cubic through 0 and 1 whose speed at
// each end is 0 (the unit starts from rest / comes to rest) or the cruising speed (1: the move
// carries on from the tile before / into the next one). Both at rest: an ease in and out; neither:
// a constant speed, so the tiles of a longer move join without a pause.
inline double step_pos(double t, bool from_rest, bool to_rest) {
    t = std::clamp(t, 0.0, 1.0);
    const double v0 = from_rest ? 0.0 : 1.0, v1 = to_rest ? 0.0 : 1.0;
    const double t2 = t * t, t3 = t2 * t;
    return (t3 - 2 * t2 + t) * v0 + (3 * t2 - 2 * t3) + (t3 - t2) * v1;
}

// An ease in and out of 0..1.
inline double smooth(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3 - 2 * t);
}

// The eighths of a turn from the facing `from` to `to` (0..7), the shorter way round: -3..4 (a half
// turn goes the positive way, as the original's).
inline int32_t turn_eighths(int32_t from, int32_t to) {
    const int32_t d = (((to - from) % 8) + 8) % 8;
    return d > 4 ? d - 8 : d;
}

// The time of one tile's move in ms. anim_mode: the game's unit animation preference (below 0
// smooth, 0 normal, above 0 fast); triad: 0 land, 1 sea, 2 air (quicker, as the original's);
// own_multiplayer: the player's own unit in a network game (the original's quickest).
inline int32_t step_ms(int32_t anim_mode, int32_t triad, bool own_multiplayer) {
    int32_t ms = anim_mode < 0 ? 280 : (anim_mode ? 110 : 190);
    if (triad == 1) ms = ms * 5 / 6;
    if (triad == 2) ms = ms * 2 / 3;
    if (own_multiplayer) ms = std::min(ms, 90);
    return ms;
}

// The time of a turn of `eighths`: three eighths of a tile's move for each.
inline int32_t turn_ms(int32_t eighths, int32_t step) { return std::abs(eighths) * step * 3 / 8; }

// How long the move waits for the turn: a turn of up to a quarter is made on the way, the part
// beyond it on the spot first (a unit does not set off backwards).
inline int32_t slide_delay_ms(int32_t eighths, int32_t step) {
    return std::max(0, std::abs(eighths) - 2) * step * 3 / 8;
}

// The selection ring's size at `ms`: it breathes between 0.9 and 1 of its full size, once in 1.4 s.
inline double ring_scale(uint32_t ms) {
    return 0.95 + 0.05 * std::sin((double)(ms % 1400) * (2.0 * 3.14159265358979323846 / 1400.0));
}

// The ring's pixel at (dx, dy) from its centre, for an ellipse of radii (rx, ry) and a band
// `thick` pixels wide inside it: 0 outside the ring, 1 its dark outline (a pixel either side of
// the band), 2 the band's outer half, 3 its inner half.
inline int32_t ring_pixel(double dx, double dy, double rx, double ry, double thick) {
    auto inside = [&](double grow) {
        const double a = rx + grow, b = ry + grow;
        if (a <= 0 || b <= 0) return false;
        return dx * dx / (a * a) + dy * dy / (b * b) <= 1.0;
    };
    if (!inside(1.0) || inside(-thick - 1.0)) return 0;
    if (!inside(0.0) || inside(-thick)) return 1;
    return inside(-thick / 2) ? 3 : 2;
}

}  // namespace modern
