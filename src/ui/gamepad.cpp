// A controller as the game's keyboard and mouse (gamepad.h, padmap.h).
#include "ui/gamepad.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "engine/ui_bridge.h"
#include "ui/basewin.h"
#include "ui/console.h"
#include "ui/console_frame.h"
#include "ui/padmap.h"
#include "ui/smoothview.h"
#include "ui/win.h"

namespace {

using platform::Event;

bool g_held[padmap::kButtons] = {};
padmap::Repeat g_repeat[padmap::kButtons];
padmap::Action g_action[padmap::kButtons];  // what a held button was pressed as
bool g_pointer_mode = false;  // the right stick pressed: the left stick is the pointer on the map too
bool g_pointer_used = false;  // the pointer was moved in a window since the D-pad or the map
double g_px = -1, g_py = -1;  // the pointer, in screen pixels
bool g_shown = false;
int32_t g_dir = 0;  // the left stick's direction key
padmap::Repeat g_dir_repeat;
uint32_t g_last_tick = 0;
bool g_sticks = false;  // a stick was off centre at the last tick
int32_t g_last_context = -1;
// What has the input changed while the left stick was pushed (a unit stepped onto a pod: a popup):
// the push is not the pointer's until the stick has been let go.
bool g_await_centre = false;
bool g_dir_unit = false;  // the left stick's hold began as a unit's step (padmap::cursor_repeats)

bool keylog() {
    static const bool on = std::getenv("OPENSMAC_KEYLOG") != nullptr;
    return on;
}

int32_t context() {
    if (ui_modal_active()) return padmap::kModal;
    if (console_menu_open()) return padmap::kMenu;
    if (Win_is_visible(&BaseWin)) return padmap::kBase;
    if (!smoothview::map_shown()) return padmap::kModal;
    // A unit asking for orders: the console's selected unit, in unit mode, the player's own.
    const int32_t veh = console_win(&MainConsole).veh;
    if (!MapWinInVehMode || veh < 0 || veh >= VehCount || !console_win(&MainConsole).waiting) return padmap::kMapIdle;
    return Units[Vehs[veh].unit_id].plan == 8 ? padmap::kMapPod : padmap::kMap;  // (8: a colony pod's plan)
}

bool pointer_wanted(int32_t ctx) { return g_pointer_mode || (!padmap::on_map(ctx) && g_pointer_used); }

void show_pointer(bool on) {
    if (on && g_px < 0) {
        g_px = ScreenWidth / 2.0;
        g_py = ScreenHeight / 2.0;
    }
    if (on == g_shown && !on) return;
    g_shown = on;
    platform::pointer_set((int32_t)g_px, (int32_t)g_py, on);
}

void push_key(std::deque<Event>* out, int32_t vk, int32_t mods) {
    Event e;
    e.type = platform::EV_KEY_DOWN;
    e.key = vk;
    e.mods = mods;
    out->push_back(e);
    e.type = platform::EV_KEY_UP;
    out->push_back(e);
}

void push_mouse(std::deque<Event>* out, int32_t type) {
    Event e;
    e.type = type;
    e.x = (int32_t)g_px;
    e.y = (int32_t)g_py;
    out->push_back(e);
}

}  // namespace

void gamepad_mouse_used() {
    g_pointer_used = false;
    g_pointer_mode = false;
    show_pointer(false);
}

void gamepad_event(const Event& ev, std::deque<Event>* out) {
    const int32_t b = ev.key;
    if (b < 0 || b >= padmap::kButtons) return;
    const bool down = ev.type == platform::EV_PAD_DOWN;
    if (!down) {
        if (!g_held[b]) return;
        g_held[b] = false;
        // The mouse button goes up where the pointer is now (a drag), whatever shows by now.
        if (g_action[b].kind == padmap::kLeftButton) push_mouse(out, platform::EV_LBUTTON_UP);
        if (g_action[b].kind == padmap::kRightButton) push_mouse(out, platform::EV_RBUTTON_UP);
        return;
    }
    const int32_t ctx = context();
    if (b >= padmap::kLeft && b <= padmap::kDown && !g_pointer_mode) g_pointer_used = false;  // the D-pad's choice
    const bool pointer = pointer_wanted(ctx);
    show_pointer(pointer);
    const padmap::Action a = padmap::button_action(b, ctx, pointer);
    if (keylog()) std::fprintf(stderr, "pad button %d: context %d pointer %d -> kind %d key %02X mods %d\n", b, ctx, pointer, a.kind, a.vk, a.mods);
    g_held[b] = true;
    g_action[b] = a;
    g_repeat[b] = padmap::Repeat{};
    switch (a.kind) {
    case padmap::kKey:
        padmap::step(&g_repeat[b], true, a.repeats, platform::ticks_ms());
        push_key(out, a.vk, a.mods);
        break;
    case padmap::kLeftButton:
        push_mouse(out, platform::EV_MOUSE_MOVE);
        push_mouse(out, platform::EV_LBUTTON_DOWN);
        break;
    case padmap::kRightButton:
        push_mouse(out, platform::EV_MOUSE_MOVE);
        push_mouse(out, platform::EV_RBUTTON_DOWN);
        break;
    case padmap::kPointerMode:
        g_pointer_mode = !g_pointer_mode;
        g_pointer_used = g_pointer_mode;
        show_pointer(pointer_wanted(ctx));
        break;
    default: break;
    }
}

void gamepad_tick(std::deque<Event>* out) {
    const uint32_t now = platform::ticks_ms();
    const double dt = std::min<uint32_t>(now - g_last_tick, 50) / 1000.0;
    g_last_tick = now;
    float lx = 0, ly = 0, rx = 0, ry = 0;
    platform::pad_stick(0, &lx, &ly);
    platform::pad_stick(1, &rx, &ry);
    const double mx = padmap::response(lx), my = padmap::response(ly);
    const double pan_x = padmap::response(rx), pan_y = padmap::response(ry);
    g_sticks = mx != 0 || my != 0 || pan_x != 0 || pan_y != 0 || g_dir != 0;
    const int32_t ctx = context();
    if (ctx != g_last_context) {  // (from the map to a window, or between windows: not the map's own states)
        if (g_last_context >= 0 && !(padmap::on_map(ctx) && padmap::on_map(g_last_context)))
            g_await_centre = lx != 0 || ly != 0;
        g_last_context = ctx;
    }
    if (mx == 0 && my == 0) g_await_centre = false;
    // Held buttons that repeat (the D-pad).
    for (int32_t b = 0; b < padmap::kButtons; b++) {
        if (!g_held[b] || g_action[b].kind != padmap::kKey || !g_action[b].repeats) continue;
        if (padmap::step(&g_repeat[b], true, true, now)) push_key(out, g_action[b].vk, g_action[b].mods);
    }
    // The right stick: the map's pan.
    const bool map = padmap::on_map(ctx);
    smoothview::pad_pan(map ? pan_x * padmap::kPanSpeed : 0, map ? pan_y * padmap::kPanSpeed : 0);
    // The left stick: the unit or the cursor on the map, the pointer elsewhere and in pointer mode.
    if (map && !g_pointer_mode) {
        if (g_shown) show_pointer(false);
        g_pointer_used = false;
        const int32_t was = g_dir;
        g_dir = padmap::direction(lx, ly, g_dir);
        if (!was && g_dir) g_dir_unit = MapWinInVehMode != 0;  // the hold begins: a unit's step, or the cursor's
        // A unit takes one step a flick; the cursor of view mode keeps going while the stick is held.
        if (padmap::step(&g_dir_repeat, g_dir != 0, padmap::cursor_repeats(MapWinInVehMode != 0, g_dir_unit), now))
            push_key(out, g_dir, 0);
        return;
    }
    g_dir = 0;
    g_dir_repeat = padmap::Repeat{};
    if ((mx == 0 && my == 0) || g_await_centre) {
        if (g_shown != pointer_wanted(ctx)) show_pointer(pointer_wanted(ctx));
        return;
    }
    if (g_px < 0) {
        g_px = ScreenWidth / 2.0;
        g_py = ScreenHeight / 2.0;
    }
    g_pointer_used = true;
    g_px = std::clamp(g_px + mx * padmap::kPointerSpeed * dt, 0.0, ScreenWidth - 1.0);
    g_py = std::clamp(g_py + my * padmap::kPointerSpeed * dt, 0.0, ScreenHeight - 1.0);
    g_shown = true;
    platform::pointer_set((int32_t)g_px, (int32_t)g_py, true);
    push_mouse(out, platform::EV_MOUSE_MOVE);
}

bool gamepad_busy() {
    if (g_sticks) return true;
    for (int32_t b = 0; b < padmap::kButtons; b++)
        if (g_held[b]) return true;
    return false;
}
