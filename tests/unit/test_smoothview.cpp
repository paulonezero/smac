// The smooth map view's arithmetic (ui/smoothview_math.h).
#include "ui/console_frame.h"
#include "ui/smoothview_math.h"
#include "unit.h"

using namespace smoothview;

TEST_CASE("smoothview: a position splits into an origin tile of the grid and pixels") {
    const int32_t hw = 28, hh = 14;  // a tile of 56 x 28
    // On the grid: nothing over.
    Split s = split(Pos{10.0 * hw, 6.0 * hh}, hw, hh);
    CHECK((s.ox == 10 && s.oy == 6 && s.px == 0 && s.py == 0));
    // Origin x and y always of the same parity, whatever the position.
    for (int32_t x = -300; x <= 300; x += 7) {
        for (int32_t y = -100; y <= 100; y += 3) {
            s = split(Pos{(double)x, (double)y}, hw, hh);
            CHECK(((s.ox ^ s.oy) & 1) == 0);
            // What is left is short of the steps, never negative: nothing shows past the near edges.
            CHECK((s.px >= 0 && s.px < 2 * hw && s.py >= 0 && s.py < hh));
            // And it adds up to the position.
            CHECK(s.ox * hw + s.px == x);
            CHECK(s.oy * hh + s.py == y);
        }
    }
}

TEST_CASE("smoothview: the short way around a round map") {
    CHECK(shortest(10, 30, 0) == 20);  // flat: as it is
    CHECK(shortest(30, 10, 0) == -20);
    CHECK(shortest(10, 30, 1000) == 20);
    CHECK(shortest(10, 990, 1000) == -20);  // across the seam
    CHECK(shortest(990, 10, 1000) == 20);
    CHECK(shortest(0, 500, 1000) == 500);
    CHECK(shortest(1990, 10, 1000) == 20);  // positions a lap apart
}

TEST_CASE("smoothview: easing and durations") {
    CHECK(ease(0) == 0);
    CHECK(ease(1) == 1);
    CHECK(ease(-1) == 0);
    CHECK(ease(2) == 1);
    double last = 0;
    for (double f = 0.05; f < 1; f += 0.05) {
        CHECK(ease(f) > last);
        last = ease(f);
    }
    CHECK(glide_ms(0, 1000) == 120);
    CHECK(glide_ms(1000, 1000) == 280);
    CHECK(glide_ms(100000, 1000) == 300);
    CHECK(glide_ms(10, 0) == 120);
    CHECK(!glide_too_far(3000, 1000));
    CHECK(glide_too_far(3001, 1000));
}

TEST_CASE("smoothview: the mouse at a screen edge scrolls, faster the nearer the edge") {
    // PRACX's defaults at 1024 wide: a band of 40 pixels, 1 to 20 tiles per second.
    CHECK(edge_rate(0, 1024, 40, 1, 20) == doctest::Approx(-20));  // the edge pixel: full speed
    CHECK(edge_rate(39, 1024, 40, 1, 20) == doctest::Approx(-1.475));  // the inner pixel: nearly the slowest
    CHECK(edge_rate(20, 1024, 40, 1, 20) == doctest::Approx(-10.5));  // half way
    CHECK(edge_rate(40, 1024, 40, 1, 20) == 0);  // out of the band
    CHECK(edge_rate(983, 1024, 40, 1, 20) == 0);
    CHECK(edge_rate(984, 1024, 40, 1, 20) == doctest::Approx(1.475));
    CHECK(edge_rate(1023, 1024, 40, 1, 20) == doctest::Approx(20));
    // Symmetrical: the same depth gives the same speed either way.
    for (int32_t d = 0; d < 40; d++) CHECK(edge_rate(d, 1024, 40, 1, 20) == doctest::Approx(-edge_rate(1023 - d, 1024, 40, 1, 20)));
    // Off the screen, or no band: nothing.
    CHECK(edge_rate(-1, 1024, 40, 1, 20) == 0);
    CHECK(edge_rate(1024, 1024, 40, 1, 20) == 0);
    CHECK(edge_rate(0, 1024, 0, 1, 20) == 0);
    // One speed when both are the same.
    CHECK(edge_rate(0, 1024, 40, 5, 5) == doctest::Approx(-5));
    CHECK(edge_rate(39, 1024, 40, 5, 5) == doctest::Approx(-5));
}

TEST_CASE("smoothview: a tile's size by zoom") {
    CHECK(tile_q(0) == 12);  // 96 x 48 at zoom 0: the sprites' own size
    CHECK(tile_q(16) == 25);
    CHECK(tile_q(-14) == 1);
    CHECK(tile_q(-16) == 1);  // never less than one
    CHECK(tile_q(-10) == 4);
}

TEST_CASE("smoothview: zoom levels from the whole map to a close view") {
    // The game's screen 1470 x 1000 less the console (213), a map of 80 x 80: at zoom -10 a
    // tile is 32 wide, so the map's 40 tiles are 1280 wide and fit; at -8 (48 wide) they do not.
    // Zoom 16 (200 wide) is the engine's limit before the view shows six tiles across. Six levels
    // 5.2 apart, each rounded to the original's even grid: -10, -4, 0, 6, 10, 16.
    CHECK(zoom_levels(6, 1470, 787, 80, 80) == std::vector<int32_t>{-10, -4, 0, 6, 10, 16});
    // The tests' 1024 x 768: the map (1280 wide at -10) only fits at -12, and at zoom 8 (a tile
    // 152 wide) the view is down to six tiles across.
    CHECK(zoom_levels(6, 1024, 555, 80, 80) == std::vector<int32_t>{-12, -8, -4, 0, 4, 8});
    CHECK(zoom_levels(4, 1024, 555, 128, 128) == std::vector<int32_t>{-14, -6, 0, 8});  // a huge map: -14 at most
    // Three levels 11 apart would be -14, -4, 8: the inner one nearest to zero becomes 0.
    CHECK(zoom_levels(3, 1024, 555, 128, 128) == std::vector<int32_t>{-14, 0, 8});
    // A tiny map fits at zoom 0: the levels only zoom in.
    CHECK(zoom_levels(3, 1024, 555, 16, 16) == std::vector<int32_t>{0, 4, 8});
    // Two levels, neither 0: 0 is added between them.
    CHECK(zoom_levels(2, 1470, 787, 80, 80) == std::vector<int32_t>{-10, 0, 16});
    // Too many levels for the grid: the repeats are dropped.
    CHECK(zoom_levels(40, 1470, 787, 80, 80).size() == 14);
    CHECK(zoom_levels(1, 1470, 787, 80, 80).empty());
    // Whatever the screen and the map: increasing, even, within the engine's range, with 0.
    for (int32_t w : {1024, 1470, 1920, 2560}) {
        for (int32_t h : {555, 787, 867, 1227}) {
            for (int32_t m : {24, 64, 80, 128, 256}) {
                for (int32_t count : {2, 3, 6, 9, 16}) {
                    const std::vector<int32_t> levels = zoom_levels(count, w, h, m, m);
                    REQUIRE(levels.size() >= 2);
                    CHECK(levels.size() <= (size_t)std::max(count, 3));
                    CHECK(std::find(levels.begin(), levels.end(), 0) != levels.end());
                    for (size_t i = 0; i < levels.size(); i++) {
                        CHECK(levels[i] % 2 == 0);
                        CHECK((levels[i] >= -14 && levels[i] <= 16));
                        if (i) CHECK(levels[i] > levels[i - 1]);
                    }
                    // The outermost level shows the whole map, or is -14.
                    const int32_t q = tile_q(levels.front());
                    CHECK((levels.front() == -14 || (4 * q * m <= w && 2 * q * m <= h)));
                }
            }
        }
    }
}

TEST_CASE("smoothview: the zoom the details are drawn for") {
    for (int32_t z = -14; z <= 16; z += 2) CHECK(detail_zoom(z, false) == z);  // off: the zoom itself
    // On: the thresholds at -8 and -10 pass from -12 in (-7 is above both), as PRACX's -14 do.
    for (int32_t z = -12; z <= -8; z += 2) CHECK(detail_zoom(z, true) == -7);
    CHECK(detail_zoom(-14, true) == -14);  // the outermost zoom stays bare
    for (int32_t z = -6; z <= 16; z += 2) CHECK(detail_zoom(z, true) == z);  // in from -8: unchanged
    CHECK(detail_zoom(-7, true) > -8);
    CHECK(!(detail_zoom(-12, true) < -8));
    CHECK(!(detail_zoom(-12, true) <= -8));
    CHECK(detail_zoom(-12, true) > -10);
}

TEST_CASE("smoothview: the zoom keys step through the levels") {
    const std::vector<int32_t> levels{-10, -4, 0, 6, 10, 16};
    CHECK(zoom_to(levels, 0, 0x203) == 6);  // in
    CHECK(zoom_to(levels, 0, 0x204) == -4);  // out
    CHECK(zoom_to(levels, 16, 0x203) == 16);  // at the limits: as it is
    CHECK(zoom_to(levels, -10, 0x204) == -10);
    CHECK(zoom_to(levels, 10, 0x205) == 0);  // the original's zoom 0
    CHECK(zoom_to(levels, 10, 0x206) == -4);  // one level out from it (the original's -6)
    CHECK(zoom_to(levels, 0, 0x207) == 16);  // the innermost
    CHECK(zoom_to(levels, 0, 0x208) == -10);  // the outermost
    CHECK(zoom_to(levels, 0, 0x999) == 0);
    // A zoom between levels (a loaded game's, or the right-click menu's -8): from its nearest level,
    // the inner one when half way.
    CHECK(zoom_to(levels, -8, 0x203) == -4);
    CHECK(zoom_to(levels, -8, 0x204) == -10);
    CHECK(zoom_to(levels, -7, 0x203) == 0);  // nearest -4, then in
    CHECK(zoom_to(levels, 8, 0x204) == 6);  // half way between 6 and 10: from 10, out
    CHECK(zoom_to(levels, -20, 0x203) == -4);  // beyond the range: from the end
    // Levels without a step out from zero: it stays.
    CHECK(zoom_to(std::vector<int32_t>{0, 8, 16}, 8, 0x206) == 0);
}

TEST_CASE("smoothview: a change of zoom keeps the map point at the anchor") {
    // A map coordinate X is at screen (X - 1) * half - position.
    const auto screen = [](double map, int32_t half, double pos) { return (map - 1) * half - pos; };
    const double anchor = 735;
    for (double pos : {-500.0, 0.0, 1234.0, 9000.0}) {
        for (int32_t half0 : {4, 24, 76, 100}) {
            for (int32_t half1 : {4, 24, 76, 100}) {
                const double map = (anchor + pos) / half0 + 1;  // the point at the anchor now
                const double to = zoom_axis(pos, half0, half1, anchor);
                CHECK(screen(map, half0, pos) == doctest::Approx(anchor));
                CHECK(screen(map, half1, to) == doctest::Approx(anchor));
            }
        }
    }
    CHECK(zoom_axis(1234, 76, 76, anchor) == doctest::Approx(1234));  // the same zoom: as it is
}

TEST_CASE("smoothview: a scaling picture") {
    // Scale 1: every pixel its own, whatever the fixed point.
    for (int32_t x = 0; x < 100; x++) CHECK(scaled_source(x, 50, 1, 100) == x);
    // Twice the size about the middle: the middle stays, the middle half fills the width.
    CHECK(scaled_source(50, 50, 2, 100) == 50);
    CHECK(scaled_source(0, 50, 2, 100) == 25);
    CHECK(scaled_source(99, 50, 2, 100) == 74);
    // Half the size: what falls outside the source is its edge, never out of bounds.
    CHECK(scaled_source(0, 50, 0.5, 100) == 0);
    CHECK(scaled_source(99, 50, 0.5, 100) == 99);
    CHECK(scaled_source(-500, 50, 1, 100) == 0);
    CHECK(scaled_source(5000, 50, 1, 100) == 99);
}

TEST_CASE("smoothview: the wheel's scrolls add up to zoom steps") {
    double acc = 0;
    // Trackpad fractions: three units make a step, the rest is kept.
    CHECK(wheel_zoom(&acc, 1.0, 3.0, 1) == 0);
    CHECK(wheel_zoom(&acc, 1.0, 3.0, 1) == 0);
    CHECK(wheel_zoom(&acc, 1.5, 3.0, 1) == 1);
    CHECK(acc == doctest::Approx(0.5));
    // The other way, and back through zero.
    CHECK(wheel_zoom(&acc, -3.5, 3.0, 1) == -1);
    CHECK(acc == doctest::Approx(0.0));
    // A wheel notch (3) is a step on its own.
    CHECK(wheel_zoom(&acc, 3.0, 3.0, 1) == 1);
    // Reversed and off.
    acc = 0;
    CHECK(wheel_zoom(&acc, 3.0, 3.0, -1) == -1);
    CHECK(wheel_zoom(&acc, 30.0, 3.0, 0) == 0);
    CHECK(acc == 0);
}

TEST_CASE("console_keep: the console's picture while the view moves") {
    // Still: drawn every time. No picture yet: drawn.
    for (int32_t k = 0; k < 10; k++) CHECK(!console_keep(false, k));
    CHECK(!console_keep(true, 0));
    // Moving: drawn once, then that picture for as long as the view moves.
    int32_t kept = 0, drawn = 0;
    for (int32_t i = 0; i < 80; i++) {
        const bool keep = console_keep(true, kept);
        kept = keep ? kept + 1 : 1;
        if (!keep) drawn++;
    }
    CHECK(drawn == 1);
}
