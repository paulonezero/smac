// Smooth movement of the main map view (see smoothview.h).
#include "ui/smoothview.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "engine/game.h"
#include "engine/map.h"
#include "engine/ui_bridge.h"
#include "platform/ui_platform.h"
#include "ui/basewin.h"
#include "ui/console.h"
#include "ui/mapwin.h"
#include "ui/reports.h"
#include "ui/win.h"

GLOBAL(int32_t, MapMouseModal, 0x915620);  // (console_mouse.cpp)

namespace smoothview {
namespace {

constexpr int32_t kFrameMs = 16;
constexpr int32_t kSettleMs = 140;  // no pan input for this long: back onto the grid
constexpr int32_t kEdgeBand = 2;  // screen pixels
constexpr int32_t kEdgeDelayMs = 120;  // the mouse waits this long at an edge before it scrolls
constexpr double kEdgeSpeed = 1100;  // pixels per second
constexpr double kWheelScale = 14;  // pixels per unit of the wheel

bool g_enabled = false, g_unattended = false;

// The view off the grid. `pan`: it stays there between frames, and the view follows it (a glide's
// picture is only drawn there, the view being at the target already).
struct Override {
    bool on = false, pan = false;
    bool hold = false;  // kept at rest, after a zoom: it does not settle
    bool synced = false;  // cx, cy are the centre it set
    Pos at;
    int32_t zoom = 0, cx = 0, cy = 0;
} g_ov;
std::vector<uint8_t> g_picture;  // the picture a zoom scales

bool g_running = false;  // a glide is in progress
int32_t g_mouse_x = -1, g_mouse_y = -1;
double g_wheel_x = 0, g_wheel_y = 0;
uint32_t g_last_tick = 0, g_last_input = 0, g_edge_since = 0;
double g_min_y = 0, g_max_y = 0, g_min_x = 0, g_max_x = 0;

uint8_t* view() { return MapWins[0]; }
bool flat() { return (MapToggleFlat & 1) != 0; }
// A pan can go round the map any number of times.
int32_t wrap_x(int32_t x) { return ((x % MapAreaX) + MapAreaX) % MapAreaX; }

// The map narrower than the window has pixel offsets of its own; with a tile or two to spare it
// could show a tile twice.
bool roomy(uint8_t* win) {
    return !map_win(win).pixel_x && !map_win(win).pixel_y && map_win(win).cols_odd + 2 < MapAreaX / 2 &&
           map_win(win).rows_above + 2 < MapAreaY / 2;
}

// The main view is what the player is looking at, on the player's own turn.
bool view_shown() {
    return !GameNotStarted && !ui_desktop() && !MapMouseModal && !ControlTurnA && !Win_is_visible(&BaseWin) &&
           !Win_is_visible(&ReportWindow) && CurrentFaction == CurrentPlayerFaction;
}

bool allowed(uint8_t* win) {
    if (!g_enabled || !win || win != view() || g_running || ui_presenting()) return false;
    if (g_ui_unattended && !g_unattended) return false;
    return view_shown();
}

Pos position(uint8_t* win) {
    return Pos{(double)(map_win(win).origin_x * map_win(win).half_w - map_win(win).pixel_x),
               (double)(map_win(win).origin_y * map_win(win).half_h - map_win(win).pixel_y)};
}

void draw(uint8_t* win) {
    map_win(win).scrolling = 0;
    pfncRedrawMap(win, 1);
    ui_map_clean();
}

// The picture at `at`, the view staying as it is.
void frame(uint8_t* win, Pos at) {
    g_ov = Override{};
    g_ov.on = true;
    g_ov.at = at;
    g_ov.zoom = map_win(win).zoom;
    draw(win);
    g_ov.on = false;
    pfncZoomProcessing(win);
    ui_present();
}

}  // namespace

void set_enabled(bool on, bool unattended) {
    g_enabled = on;
    g_unattended = unattended;
}

void apply(uint8_t* win) {
    if (!g_ov.on || win != view()) return;
    MapWin& m = map_win(win);
    // Another zoom has its own grid, and a view moved by other means (a loaded game) is not ours.
    const bool moved = g_ov.pan && g_ov.synced && (m.center_x != g_ov.cx || m.center_y != g_ov.cy);
    if (m.zoom != g_ov.zoom || moved || !roomy(win)) {
        g_ov.on = false;
        return;
    }
    const Split s = split(g_ov.at, m.half_w, m.half_h);
    if (g_ov.pan) {
        int32_t cx = s.ox + m.cols_odd, cy = s.oy + m.rows_above;
        m.center_x = flat() ? cx : wrap_x(cx);
        m.center_y = cy;
        g_ov.cx = m.center_x;
        g_ov.cy = m.center_y;
        g_ov.synced = true;
    }
    m.origin_x = flat() ? s.ox : wrap_x(s.ox);
    m.origin_y = s.oy;
    m.pixel_x = -s.px;
    m.pixel_y = -s.py;
    // One more tile each way: the picture is drawn up to a tile short of the far edges.
    m.cols_odd++;
    m.cols_even++;
    m.rows_above++;
    m.rows_below++;
}

bool capture(uint8_t* win, Pos* from) {
    if (!allowed(win) || (!g_ov.on && !roomy(win))) {
        if (win == view() && g_ov.on) {
            g_ov.on = false;
            pfncZoomProcessing(win);
        }
        return false;
    }
    *from = g_ov.on ? g_ov.at : position(win);
    g_ov.on = false;
    return true;
}

void glide(uint8_t* win, Pos from, int32_t redraw) {
    pfncZoomProcessing(win);
    const Pos to = position(win);
    const double span = flat() ? 0 : (double)MapAreaX * map_win(win).half_w;
    const double dx = shortest(from.x, to.x, span), dy = to.y - from.y;
    const double dist = std::hypot(dx, dy), screen = std::hypot((double)ScreenWidth, (double)ScreenHeight);
    if (dist >= 1 && !glide_too_far(dist, screen)) {
        g_running = true;
        const int32_t ms = glide_ms(dist, screen);
        const uint32_t t0 = platform::ticks_ms();
        int32_t frames = 0;
        for (;;) {
            const uint32_t begun = platform::ticks_ms();
            const int32_t done = (int32_t)(begun - t0);
            if (done >= ms) break;
            const double k = ease((double)done / ms);
            frame(win, Pos{to.x - dx * (1 - k), to.y - dy * (1 - k)});
            frames++;
            const int32_t spent = (int32_t)(platform::ticks_ms() - begun);
            if (spent < kFrameMs) platform::delay_ms((uint32_t)(kFrameMs - spent));
        }
        g_running = false;
        if (g_unattended) std::fprintf(stderr, "view glide: %d frames in %d ms\n", frames, ms);
    }
    map_win(win).scrolling = 0;
    pfncRedrawMap(win, redraw);
    ui_invalidate();
}

bool zoom_capture(uint8_t* win, ZoomFrom* from) {
    if (!allowed(win)) return false;
    MapWinNative* n = ui_mapwin_native(win);
    if (!n || !n->canvas || !n->canvas->data) return false;
    from->at = g_ov.on ? g_ov.at : position(win);
    from->half_w = map_win(win).half_w;
    from->half_h = map_win(win).half_h;
    from->zoom = map_win(win).zoom;
    g_picture.assign(n->canvas->data, n->canvas->data + (size_t)n->canvas->stride * n->canvas->height);
    return true;
}

void zoom_glide(uint8_t* win, const ZoomFrom& from) {
    MapWin& m = map_win(win);
    if (m.zoom == from.zoom) {  // at the limit: nothing moves
        draw(win);
        ui_invalidate();
        return;
    }
    Buffer* canvas = ui_mapwin_native(win)->canvas;
    g_ov.on = false;
    pfncZoomProcessing(win);  // the new zoom's sizes, on the grid
    const int32_t half_w = m.half_w, half_h = m.half_h;
    const double sx = ScreenWidth / 2.0, sy = ScreenHeight / 2.0;
    Pos to = position(win);
    if (roomy(win)) {
        to.x = zoom_axis(from.at.x, from.half_w, half_w, sx);
        to.y = zoom_axis(from.at.y, from.half_h, half_h, sy);
        // Inside the map at the new zoom, as the view on the grid is.
        const double min_y = -2.0 * half_h;
        const double max_y = (double)std::max(-2, MapAreaY + (2 - m.rows_above) * 2) * half_h;
        to.y = std::clamp(to.y, min_y, max_y);
        if (flat()) {
            to.x = std::clamp(to.x, -4.0 * half_w, (double)std::max(-4, MapAreaX + (2 - m.cols_odd) * 2) * half_w);
        }
        g_ov = Override{};
        g_ov.on = g_ov.pan = g_ov.hold = true;
        g_ov.at = to;
        g_ov.zoom = m.zoom;
    }
    // Screen positions: new = old * scale + (scale * from - to) on each axis.
    const double scale = (double)half_w / from.half_w;
    const bool growing = scale >= 1;
    double bx = scale * from.at.x - to.x, by = scale * from.at.y - to.y;
    if (!growing) {  // the new picture, shrinking from the old one's size
        draw(win);
        g_picture.assign(canvas->data, canvas->data + (size_t)canvas->stride * canvas->height);
    }
    const int32_t w = canvas->width, h = canvas->height;
    const bool same_size = (size_t)canvas->stride * h == g_picture.size();
    const bool still = std::abs(scale - 1) < 1e-6;
    // The point each frame's scaling leaves where it is.
    const double fx = still ? 0 : bx / (1 - scale), fy = still ? 0 : by / (1 - scale);
    const int32_t ms = 130;
    if (same_size) {
        g_running = true;
        const uint32_t t0 = platform::ticks_ms();
        std::vector<int32_t> cols((size_t)w);
        int32_t frames = 0;
        for (;;) {
            const uint32_t begun = platform::ticks_ms();
            const int32_t done = (int32_t)(begun - t0);
            if (done >= ms) break;
            const double k = ease((double)done / ms);
            const double s = growing ? std::pow(scale, k) : std::pow(1 / scale, 1 - k);
            for (int32_t x = 0; x < w; x++) cols[(size_t)x] = still ? std::clamp((int32_t)(x - bx * (k - (growing ? 0 : 1))), 0, w - 1) : scaled_source(x, fx, s, w);
            for (int32_t y = 0; y < h; y++) {
                const int32_t sr = still ? std::clamp((int32_t)(y - by * (k - (growing ? 0 : 1))), 0, h - 1) : scaled_source(y, fy, s, h);
                const uint8_t* src = g_picture.data() + (size_t)sr * canvas->stride;
                uint8_t* dst = canvas->data + (size_t)y * canvas->stride;
                for (int32_t x = 0; x < w; x++) dst[x] = src[cols[(size_t)x]];
            }
            ui_map_clean();
            ui_present();
            frames++;
            const int32_t spent = (int32_t)(platform::ticks_ms() - begun);
            if (spent < kFrameMs) platform::delay_ms((uint32_t)(kFrameMs - spent));
        }
        g_running = false;
        if (g_unattended) std::fprintf(stderr, "view zoom: %d frames, scale %.3f\n", frames, scale);
    }
    draw(win);
    ui_invalidate();
}

bool panning() { return g_ov.on && g_ov.pan; }

bool busy() { return (panning() && !g_ov.hold) || g_edge_since != 0 || g_wheel_x != 0 || g_wheel_y != 0; }

void mouse(int32_t x, int32_t y) {
    g_mouse_x = x;
    g_mouse_y = y;
}

void wheel(float dx, float dy) {
    if (!g_enabled) return;
    // The wheel's x and y point the way the view is to go (macOS's natural scrolling included).
    g_wheel_x += dx * kWheelScale;
    g_wheel_y -= dy * kWheelScale;
}

void tick() {
    uint8_t* win = view();
    const uint32_t now = platform::ticks_ms();
    const double dt = std::min<uint32_t>(now - g_last_tick, 50) / 1000.0;
    g_last_tick = now;
    if (!allowed(win) || (!g_ov.on && !roomy(win))) {
        g_wheel_x = g_wheel_y = 0;
        g_edge_since = 0;
        if (panning() && !g_ov.hold && g_enabled && !g_running && !ui_presenting()) {  // a window opened: back on the grid
            g_ov.on = false;
            draw(win);
            ui_invalidate();
        }
        return;
    }
    int32_t ex = 0, ey = 0;
    if ((GamePreferences & PREF_BSC_MOUSE_EDGE_SCROLL_VIEW) && g_mouse_x >= 0 &&
        (g_unattended || platform::window_focused())) {
        ex = edge_axis(g_mouse_x, ScreenWidth, kEdgeBand);
        ey = edge_axis(g_mouse_y, ScreenHeight, kEdgeBand);
    }
    if (ex || ey) {
        if (!g_edge_since) g_edge_since = now;
        if (now - g_edge_since < kEdgeDelayMs) ex = ey = 0;
    } else {
        g_edge_since = 0;
    }
    const double dx = ex * kEdgeSpeed * dt + g_wheel_x, dy = ey * kEdgeSpeed * dt + g_wheel_y;
    g_wheel_x = g_wheel_y = 0;
    if (dx != 0 || dy != 0) {
        if (!g_ov.on) {
            MapWin& m = map_win(win);
            g_min_y = -2.0 * m.half_h;
            g_max_y = (double)std::max(-2, MapAreaY + (2 - m.rows_above) * 2) * m.half_h;
            g_min_x = -4.0 * m.half_w;
            g_max_x = (double)std::max(-4, MapAreaX + (2 - m.cols_odd) * 2) * m.half_w;
            g_ov.at = position(win);
            g_ov.zoom = m.zoom;
            g_ov.synced = false;
        }
        g_ov.on = true;
        g_ov.pan = true;
        g_ov.hold = false;
        g_ov.at.x += dx;
        g_ov.at.y = std::clamp(g_ov.at.y + dy, g_min_y, g_max_y);
        if (flat()) g_ov.at.x = std::clamp(g_ov.at.x, g_min_x, g_max_x);
        g_last_input = now;
        draw(win);
        ui_present();
        return;
    }
    if (panning() && !g_ov.hold && now - g_last_input >= kSettleMs) {
        const Pos from = g_ov.at;
        g_ov.on = false;
        glide(win, from, 1);
    }
}

void point() {
    static int32_t tx = -1, ty = -1;
    uint8_t* win = view();
    MapWin& m = map_win(win);
    if (tx < 0) {
        tx = m.center_x;
        ty = m.center_y;
    }
    int32_t px = 0, py = 0;
    pfncTileToPoint(win, tx, ty, &px, &py);
    px += m.half_w;
    py += m.half_h;
    std::fprintf(stderr, "viewpoint: zoom %d tile %dx%d tile %d,%d at %d,%d, %d,%d from the view's centre, origin %d,%d\n",
                 m.zoom, m.tile_w, m.tile_h, tx, ty, px, py, px - ScreenWidth / 2, py - ScreenHeight / 2, m.origin_x,
                 m.origin_y);
}

void check() {
    uint8_t* win = view();
    MapWin& m = map_win(win);
    int32_t tested = 0, wrong = 0;
    for (int32_t r = 2; r < m.rows_above + m.rows_below - 4; r++) {
        const int32_t y = m.origin_y + r;
        int32_t x = m.origin_x + (r & 1);
        for (int32_t c = 1; c + 1 < m.cols_odd; c++, x += 2) {
            const int32_t tx = flat() ? x : wrap_x(x);
            if (y < 0 || y >= MapAreaY || tx < 0 || tx >= MapAreaX) continue;
            int32_t px = 0, py = 0, bx = -1, by = -1;
            pfncTileToPoint(win, tx, y, &px, &py);
            if (px < 0 || py < 0) continue;
            // The middle of the tile on a level map: pfncTileToPoint gives the top of its box.
            if (pfncPtToTile(win, px + m.half_w, py + m.half_h, &bx, &by)) continue;
            tested++;
            if (bx != tx || by != y) wrong++;
        }
    }
    std::fprintf(stderr, "viewcheck: panning %d offset %d,%d: %d tiles, %d wrong\n", panning(), m.pixel_x, m.pixel_y,
                 tested, wrong);
}

}  // namespace smoothview
