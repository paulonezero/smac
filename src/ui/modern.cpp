// Modern unit animations: the settings and the selection ring (modern.h). The moves are in
// scoot.cpp.
#include "ui/modern.h"

#include <algorithm>

#include "engine/globals.h"
#include "engine/ui_bridge.h"
#include "platform/ui_platform.h"
#include "ui/gfx.h"
#include "ui/mapwin.h"
#include "ui/modern_math.h"
#include "ui/smoothview.h"
#include "ui/sprites.h"
#include "ui/vehdraw.h"
#include "ui/win.h"

namespace modern {

namespace {

bool g_ring = false, g_moves = false;

// The ring's unit as the main view last drew its tile.
struct Placed {
    bool on = false;
    int32_t veh = -1, x = 0, y = 0, px = 0, py = 0, lift = 0;
} g_placed;

// The unit that waited for orders at the last presentation, and where.
struct Waiting {
    int32_t veh = -1, x = 0, y = 0;
    bool operator==(const Waiting& o) const { return veh == o.veh && x == o.x && y == o.y; }
} g_waiting;

uint32_t g_frame_at = 0;  // ring_tick

}  // namespace

void set_enabled(bool ring, bool moves) {
    g_ring = ring;
    g_moves = moves;
}
bool ring() { return g_ring; }
bool moves() { return g_moves; }

void ring_place(int32_t veh, int32_t x, int32_t y, int32_t px, int32_t py, int32_t lift) {
    g_placed = Placed{true, veh, x, y, px, py, lift};
}

void ring_reset() { g_placed.on = false; }

void ring_shift(int32_t dx, int32_t dy) {
    g_placed.px -= dx;
    g_placed.py -= dy;
}

void ring_check() {
    if (!g_ring || smoothview::scaling()) return;
    const int32_t veh = vehdraw_waiting_unit();
    // A unit lifted off the map is being moved: its picture is the move's until it is set down.
    if (veh >= 0 && Vehs[veh].x < 0) return;
    Waiting now;
    if (veh >= 0) now = Waiting{veh, Vehs[veh].x, Vehs[veh].y};
    if (now == g_waiting) return;
    g_waiting = now;
    ui_invalidate_map();
}

void ring_draw(Buffer* dest, int32_t width, int32_t height) {
    if (!g_ring || !g_placed.on || !dest || !dest->data || smoothview::scaling()) return;
    const int32_t veh = vehdraw_waiting_unit();
    if (veh < 0 || veh != g_placed.veh || Vehs[veh].x != g_placed.x || Vehs[veh].y != g_placed.y) return;
    uint8_t* win = MapWins[0];
    if (!win) return;
    width = std::min(width, dest->width);
    height = std::min(height, dest->height);
    const int32_t half_w = map_win(win).half_w, half_h = map_win(win).half_h;
    // The tile's centre: the unit is drawn from the tile's point less half a tile's height.
    const int32_t cx = g_placed.px + half_w, cy = g_placed.py + 2 * half_h - g_placed.lift;
    // Scripted runs: the ring at rest, so that their pictures compare.
    const double scale = ring_scale(g_ui_unattended ? 0 : platform::ticks_ms());
    const double rx = half_w * 0.74 * scale, ry = half_h * 0.74 * scale;
    const double thick = std::max(2.0, half_h / 6.0);
    const uint8_t colors[4] = {0, 0, (uint8_t)FactionColorFill[Vehs[veh].faction_id & 7], 255};
    const int32_t x0 = std::max(0, (int32_t)(cx - rx) - 2), x1 = std::min(width - 1, (int32_t)(cx + rx) + 2);
    const int32_t y0 = std::max(0, (int32_t)(cy - ry) - 2), y1 = std::min(height - 1, (int32_t)(cy + ry) + 2);
    for (int32_t y = y0; y <= y1; y++) {
        uint8_t* row = dest->data + (size_t)y * dest->stride;
        for (int32_t x = x0; x <= x1; x++) {
            const int32_t p = ring_pixel(x - cx, y - cy, rx, ry, thick);
            if (p) row[x] = colors[p];
        }
    }
    // The unit over its ring, as MapWin_draw_vehicles draws it.
    UiRect clip{0, 0, width, height};
    Buffer_set_clip(dest, &clip);
    {
        smoothview::ArtScale art(win);
        veh_draw(dest, veh, g_placed.px, g_placed.py, map_win(win).zoom, 3, 1);
    }
    Buffer_set_clip(dest, nullptr);
}

bool ring_active() { return g_ring && g_placed.on && !g_ui_unattended && vehdraw_waiting_unit() == g_placed.veh; }

bool ring_tick(uint32_t now_ms, int32_t every_ms) {
    if (!g_ring || !g_placed.on || g_ui_unattended) return false;
    if ((int32_t)(now_ms - g_frame_at) < every_ms) return false;
    g_frame_at = now_ms;
    return vehdraw_waiting_unit() == g_placed.veh;
}

}  // namespace modern
