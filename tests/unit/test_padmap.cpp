// The controller's mapping (ui/padmap.h).
#include "doctest/doctest.h"
#include "ui/padmap.h"

using namespace padmap;

TEST_CASE("padmap: buttons by context") {
    const int32_t all[] = {kMap, kMapPod, kMapIdle, kBase, kMenu, kModal};
    // The D-pad is the arrow keys everywhere, and repeats.
    for (int32_t c : all) {
        CHECK(button_action(kUp, c, false).vk == 0x26);
        CHECK(button_action(kDown, c, true).vk == 0x28);
        CHECK(button_action(kLeft, c, false).repeats);
    }
    // A, a unit asking: automated (shift+A); a colony pod founds its base (B, which on an armed
    // unit would be "obliterate base": only for the pod).
    CHECK(button_action(kA, kMap, false).vk == 'A');
    CHECK(button_action(kA, kMap, false).mods == kShift);
    CHECK(button_action(kA, kMapPod, false).vk == 'B');
    CHECK(button_action(kA, kMapPod, false).mods == 0);
    // A otherwise: Enter (the turn ends, a popup's OK); the left button when the pointer shows.
    for (int32_t c : {kMapIdle, kBase, kMenu, kModal}) CHECK(button_action(kA, c, false).vk == 0x0D);
    for (int32_t c : all) CHECK(button_action(kA, c, true).kind == kLeftButton);
    // B: the unit holds; nothing with no unit (Esc on the map asks to quit); Esc in windows.
    CHECK(button_action(kB, kMap, false).vk == 'H');
    CHECK(button_action(kB, kMapPod, false).vk == 'H');
    CHECK(button_action(kB, kMapIdle, false).kind == kNone);
    for (int32_t c : {kBase, kMenu, kModal}) CHECK(button_action(kB, c, false).vk == 0x1B);
    CHECK(button_action(kB, kMap, true).kind == kRightButton);
    // Y: skip, only with a unit on the map; Space in windows.
    CHECK(button_action(kY, kMap, false).vk == 0x20);
    CHECK(button_action(kY, kMapIdle, false).kind == kNone);
    CHECK(button_action(kY, kModal, false).vk == 0x20);
    // X: the turn ends from the map, with a unit asking or not; nothing in windows.
    for (int32_t c : {kMap, kMapPod, kMapIdle}) {
        CHECK(button_action(kX, c, false).vk == 0x0D);
        CHECK(button_action(kX, c, false).mods == kCtrl);
    }
    for (int32_t c : {kBase, kMenu, kModal}) CHECK(button_action(kX, c, false).kind == kNone);
    // Plus: the menu bar from the map, Esc elsewhere. Minus: pointer mode, anywhere.
    CHECK(button_action(kPlus, kMapIdle, false).vk == 0x12);
    CHECK(button_action(kPlus, kModal, false).vk == 0x1B);
    for (int32_t c : all) CHECK(button_action(kMinus, c, true).kind == kPointerMode);
    // L / R: bases in the base window, pages in lists, nothing on the map.
    CHECK(button_action(kL, kBase, false).vk == 0x25);
    CHECK(button_action(kR, kBase, false).vk == 0x27);
    CHECK(button_action(kR, kModal, false).vk == 0x22);
    CHECK(button_action(kR, kMap, false).kind == kNone);
    // Zoom and centre only on the map; nothing repeats but the D-pad.
    CHECK(button_action(kZL, kMapIdle, false).vk == 'X');
    CHECK(button_action(kZR, kBase, false).kind == kNone);
    CHECK(button_action(kStickL, kMap, false).vk == 'C');
    CHECK(button_action(kStickR, kMap, false).kind == kNone);
    for (int32_t c : all)
        for (int32_t b = 0; b < kLeft; b++) CHECK_FALSE(button_action(b, c, false).repeats);
    CHECK(button_action(99, kMap, false).kind == kNone);
}

TEST_CASE("padmap: the stick's eight directions") {
    CHECK(direction(0, 0) == 0);
    CHECK(direction(0.3, -0.3) == 0);  // below the push that counts
    CHECK(direction(0, -1) == 0x68);  // north: keypad 8
    CHECK(direction(1, -1) == 0x69);
    CHECK(direction(1, 0) == 0x66);
    CHECK(direction(1, 1) == 0x63);
    CHECK(direction(0, 1) == 0x62);
    CHECK(direction(-1, 1) == 0x61);
    CHECK(direction(-1, 0) == 0x64);
    CHECK(direction(-1, -1) == 0x67);
    CHECK(direction(-0.05, -1) == 0x68);  // just west of north is still north (the wrap)
    // A push held: kept down to the lower threshold, and across a sector's edge.
    CHECK(direction(0, -0.4) == 0);
    CHECK(direction(0, -0.4, 0x68) == 0x68);
    CHECK(direction(0, -0.3, 0x68) == 0);
    const double edge = std::tan(22.5 * 3.14159265358979323846 / 180);  // north / north-east boundary
    CHECK(direction(edge + 0.02, -1) == 0x69);
    CHECK(direction(edge + 0.02, -1, 0x68) == 0x68);
    CHECK(direction(1, -1, 0x68) == 0x69);  // clearly another sector
}

TEST_CASE("padmap: stick response") {
    CHECK(response(0) == 0);
    CHECK(response(0.2) == 0);
    CHECK(response(-0.15) == 0);
    CHECK(response(1) == doctest::Approx(1));
    CHECK(response(-1) == doctest::Approx(-1));
    CHECK(response(1.2) == doctest::Approx(1));
    CHECK(response(0.6) == doctest::Approx(0.25));  // half way past the dead zone, squared
    CHECK(response(-0.6) == doctest::Approx(-0.25));
    CHECK(response(0.3) < response(0.5));
}

TEST_CASE("padmap: repeats of a held key") {
    Repeat r;
    CHECK_FALSE(step(&r, false, true, 1000));
    CHECK(step(&r, true, true, 1000));  // the press
    CHECK_FALSE(step(&r, true, true, 1000 + kRepeatDelay - 1));
    CHECK(step(&r, true, true, 1000 + kRepeatDelay));
    CHECK_FALSE(step(&r, true, true, 1000 + kRepeatDelay + kRepeatEvery - 1));
    CHECK(step(&r, true, true, 1000 + kRepeatDelay + kRepeatEvery));
    CHECK_FALSE(step(&r, false, true, 5000));  // let go
    CHECK(step(&r, true, false, 6000));  // pressed again: once only when it may not repeat
    CHECK_FALSE(step(&r, true, false, 9000));
    // The clock wrapping round does not stall it.
    Repeat w;
    CHECK(step(&w, true, true, 0xFFFFFF00u));
    CHECK(step(&w, true, true, 0xFFFFFF00u + kRepeatDelay));
}

TEST_CASE("padmap: a stick held from a unit's step does not become the cursor") {
    // View mode from the start: the cursor repeats.
    CHECK(cursor_repeats(false, false));
    // A unit's step: one a flick, in unit mode and after it ends in view mode.
    CHECK(!cursor_repeats(true, true));
    CHECK(!cursor_repeats(false, true));
    // (A hold that began in view mode and finds a unit selected: the unit takes no repeats.)
    CHECK(!cursor_repeats(true, false));
}
