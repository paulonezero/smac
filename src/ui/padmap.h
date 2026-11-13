// The controller's mapping to the game's keys and mouse (ui/gamepad.h), as pure functions so
// that the unit tests can check it: no SDL, no game state.
//
// The scheme (a Nintendo Switch's labels), for a player who lets the game run itself: a unit
// that asks for orders gets one of four answers, and everything else is in the game's own menus.
//   A              the unit is automated (a colony pod founds its base); with no unit asking, and
//                  in windows, Enter (ends the turn, OK); the left button when the pointer shows
//   B              the unit holds its position until woken; in windows Esc; the right button when
//                  the pointer shows
//   Y              the unit skips this turn (Space)
//   X              on the map, ends the turn now (Ctrl+Enter), whoever still waits
//   left stick     on the map a flick steps the unit (the cursor in view mode, repeating) in eight
//                  directions; in windows, and in pointer mode, moves the pointer
//   D-pad          the arrow keys everywhere (repeats while held)
//   right stick    pans the map
//   ZL / ZR        on the map zoom out / in (X / Z)
//   L / R          in the base window the previous / next base; in lists Page Up / Page Down
//   Plus           on the map the menu bar (every other command), else Esc
//   Minus          pointer mode on / off (the left stick is the pointer on the map too)
//   left stick pressed   centre on the unit or cursor (C)
#pragma once
#include <cmath>
#include <cstdint>

namespace padmap {

// The buttons, in the order of SDL's Switch joystick driver (SDL_sysjoystick.c, pad_mapping_default).
enum Button : int32_t {
    kA, kB, kX, kY, kStickL, kStickR, kL, kR, kZL, kZR, kPlus, kMinus, kLeft, kUp, kRight, kDown, kButtons
};

// What has the input: the map with a unit asking for orders (a colony pod apart), the map with
// none (the turn can end, or view mode), the base window, the menu bar, or a popup or screen
// (anything that waits for its own input).
enum Context : int32_t { kMap, kMapPod, kMapIdle, kBase, kMenu, kModal };
constexpr bool on_map(int32_t context) { return context == kMap || context == kMapPod || context == kMapIdle; }

enum Kind : int32_t { kNone, kKey, kLeftButton, kRightButton, kPointerMode };
enum : int32_t { kShift = 1, kCtrl = 2, kAlt = 4 };  // platform::MOD_*

struct Action {
    int32_t kind = kNone;
    int32_t vk = 0;  // kKey: the Windows virtual-key code
    int32_t mods = 0;
    bool repeats = false;  // kKey: again while the button is held
};

constexpr Action key(int32_t vk, int32_t mods = 0, bool repeats = false) { return Action{kKey, vk, mods, repeats}; }

// pointer: the drawn pointer shows.
constexpr Action button_action(int32_t button, int32_t context, bool pointer) {
    const bool map = on_map(context), unit = context == kMap || context == kMapPod;
    switch (button) {
    case kUp: return key(0x26, 0, true);
    case kDown: return key(0x28, 0, true);
    case kLeft: return key(0x25, 0, true);
    case kRight: return key(0x27, 0, true);
    case kA:
        if (pointer) return Action{kLeftButton};
        return context == kMap ? key('A', kShift) : context == kMapPod ? key('B') : key(0x0D);
    case kB:
        if (pointer) return Action{kRightButton};
        return unit ? key('H') : map ? Action{} : key(0x1B);
    case kY: return map && !unit ? Action{} : key(0x20);
    case kX: return map ? key(0x0D, kCtrl) : Action{};
    case kL: return context == kBase ? key(0x25) : map ? Action{} : key(0x21);
    case kR: return context == kBase ? key(0x27) : map ? Action{} : key(0x22);
    case kZL: return map ? key('X') : Action{};
    case kZR: return map ? key('Z') : Action{};
    case kPlus: return map ? key(0x12) : key(0x1B);
    case kMinus: return Action{kPointerMode};
    case kStickL: return map ? key('C') : Action{};
    default: return Action{};
    }
}

// A stick's push as a keypad direction key (VK_NUMPAD1..9; 0: at rest). x right, y down, -1..1.
// was: the key of the last call: a push holds its direction until it falls below the lower
// threshold or is clearly in another sector, so a stick at a sector's edge does not flutter.
inline int32_t direction(double x, double y, int32_t was = 0) {
    const double push = std::hypot(x, y);
    if (push < (was ? 0.35 : 0.6)) return 0;
    // Eight sectors of 45 degrees, clockwise from north.
    static constexpr int32_t kKeys[8] = {0x68, 0x69, 0x66, 0x63, 0x62, 0x61, 0x64, 0x67};
    const double turn = std::atan2(x, -y) / (2 * 3.14159265358979323846);  // 0 north, 0.25 east
    const double pos = (turn < 0 ? turn + 1 : turn) * 8;
    if (was) {  // stay unless more than 0.15 of a sector past the edge
        for (int32_t i = 0; i < 8; i++) {
            if (kKeys[i] != was) continue;
            double off = std::fabs(pos - i);
            if (off > 4) off = 8 - off;
            if (off < 0.65) return was;
        }
    }
    return kKeys[(int32_t)(pos + 0.5) % 8];
}

// A stick axis as a speed factor -1..1: nothing inside the dead zone, then squared, so that a
// light push is slow and exact.
inline double response(double v) {
    const double dead = 0.2, a = std::fabs(v);
    if (a <= dead) return 0;
    const double t = (a >= 1 ? 1.0 : (a - dead) / (1 - dead));
    return (v < 0 ? -1 : 1) * t * t;
}

// A held key's repeats: the first after kRepeatDelay ms, then every kRepeatEvery.
constexpr uint32_t kRepeatDelay = 400, kRepeatEvery = 130;
struct Repeat {
    bool held = false;
    uint32_t next = 0;
};
// True when the key is to be sent now: at the press, and at each repeat when may_repeat.
inline bool step(Repeat* r, bool held, bool may_repeat, uint32_t now) {
    if (!held) {
        r->held = false;
        return false;
    }
    if (!r->held) {
        r->held = true;
        r->next = now + kRepeatDelay;
        return true;
    }
    if (!may_repeat || (int32_t)(now - r->next) < 0) return false;
    r->next = now + kRepeatEvery;
    return true;
}

// The pointer's speed at a full push, in screen pixels a second.
// Whether the left stick, held, repeats as the cursor of view mode. Not when the hold began as a
// unit's step: the unit's move ends, no unit waits, and the stick still held from that step would
// send the cursor (and the view after it) off across the map.
inline bool cursor_repeats(bool unit_mode, bool began_as_unit_step) { return !unit_mode && !began_as_unit_step; }

constexpr double kPointerSpeed = 900;
// The map's pan at a full push, in tiles a second.
constexpr double kPanSpeed = 14;

}  // namespace padmap
