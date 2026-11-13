// The modern unit animations' arithmetic (ui/modern_math.h).
#include "doctest/doctest.h"
#include "ui/modern_math.h"

TEST_CASE("modern: a tile's move starts and ends where it should") {
    for (bool from_rest : {false, true})
        for (bool to_rest : {false, true}) {
            CHECK(modern::step_pos(0.0, from_rest, to_rest) == doctest::Approx(0.0));
            CHECK(modern::step_pos(1.0, from_rest, to_rest) == doctest::Approx(1.0));
            // Out of range: held at the ends (a frame before the slide starts, a late last frame).
            CHECK(modern::step_pos(-0.5, from_rest, to_rest) == doctest::Approx(0.0));
            CHECK(modern::step_pos(1.5, from_rest, to_rest) == doctest::Approx(1.0));
            // Never backwards, never past the tile.
            double last = 0;
            for (int i = 1; i <= 100; i++) {
                const double p = modern::step_pos(i / 100.0, from_rest, to_rest);
                CHECK(p >= last);
                CHECK(p <= 1.0 + 1e-12);
                last = p;
            }
        }
}

TEST_CASE("modern: speed at a move's ends") {
    const double h = 1e-6;
    auto speed0 = [&](bool a, bool b) { return modern::step_pos(h, a, b) / h; };
    auto speed1 = [&](bool a, bool b) { return (1.0 - modern::step_pos(1.0 - h, a, b)) / h; };
    // From rest to rest: still at both ends, the half way at half time.
    CHECK(speed0(true, true) == doctest::Approx(0.0).epsilon(1e-3));
    CHECK(speed1(true, true) == doctest::Approx(0.0).epsilon(1e-3));
    CHECK(modern::step_pos(0.5, true, true) == doctest::Approx(0.5));
    // A tile in the middle of a longer move: the cruising speed throughout.
    for (int i = 0; i <= 10; i++) CHECK(modern::step_pos(i / 10.0, false, false) == doctest::Approx(i / 10.0));
    // The first tile ends at the cruising speed, the last one starts at it: no jump where they join.
    CHECK(speed0(true, false) == doctest::Approx(0.0).epsilon(1e-3));
    CHECK(speed1(true, false) == doctest::Approx(1.0).epsilon(1e-3));
    CHECK(speed0(false, true) == doctest::Approx(1.0).epsilon(1e-3));
    CHECK(speed1(false, true) == doctest::Approx(0.0).epsilon(1e-3));
}

TEST_CASE("modern: a turn goes the shorter way") {
    CHECK(modern::turn_eighths(0, 0) == 0);
    CHECK(modern::turn_eighths(0, 1) == 1);
    CHECK(modern::turn_eighths(0, 7) == -1);
    CHECK(modern::turn_eighths(7, 0) == 1);
    CHECK(modern::turn_eighths(6, 1) == 3);
    CHECK(modern::turn_eighths(1, 6) == -3);
    CHECK(modern::turn_eighths(2, 6) == 4);  // a half turn: the positive way, as the original's
    CHECK(modern::turn_eighths(6, 2) == 4);
    for (int from = 0; from < 8; from++)
        for (int to = 0; to < 8; to++) {
            const int e = modern::turn_eighths(from, to);
            CHECK(e >= -3);
            CHECK(e <= 4);
            CHECK(((from + e) % 8 + 8) % 8 == to);
        }
}

TEST_CASE("modern: times") {
    // The game's animation preference: smooth is the slowest, fast the quickest; sea and air quicker.
    CHECK(modern::step_ms(-1, 0, false) > modern::step_ms(0, 0, false));
    CHECK(modern::step_ms(0, 0, false) > modern::step_ms(1, 0, false));
    CHECK(modern::step_ms(0, 1, false) < modern::step_ms(0, 0, false));
    CHECK(modern::step_ms(0, 2, false) < modern::step_ms(0, 1, false));
    CHECK(modern::step_ms(-1, 0, true) <= 90);
    CHECK(modern::step_ms(1, 2, true) > 0);
    for (int mode : {-1, 0, 1}) {
        const int step = modern::step_ms(mode, 0, false);
        CHECK(modern::turn_ms(0, step) == 0);
        CHECK(modern::slide_delay_ms(0, step) == 0);
        for (int e = -3; e <= 4; e++) {
            CHECK(modern::turn_ms(e, step) == modern::turn_ms(-e, step));
            // A turn of up to a quarter is made on the way; beyond it the move waits.
            CHECK((modern::slide_delay_ms(e, step) == 0) == (e >= -2 && e <= 2));
            // The turn is over by the time the unit arrives.
            CHECK(modern::turn_ms(e, step) <= modern::slide_delay_ms(e, step) + step);
        }
    }
}

TEST_CASE("modern: the selection ring") {
    for (uint32_t ms = 0; ms < 3000; ms += 7) {
        CHECK(modern::ring_scale(ms) >= 0.9 - 1e-9);
        CHECK(modern::ring_scale(ms) <= 1.0 + 1e-9);
    }
    CHECK(modern::ring_scale(0) == doctest::Approx(modern::ring_scale(1400)));  // one period
    CHECK(modern::ring_scale(350) == doctest::Approx(1.0));
    // An ellipse 40 x 20 with a band of 4: the centre and far outside are clear; along each axis
    // come the outline, the band's two halves and the outline again.
    CHECK(modern::ring_pixel(0, 0, 40, 20, 4) == 0);
    CHECK(modern::ring_pixel(60, 0, 40, 20, 4) == 0);
    CHECK(modern::ring_pixel(0, 30, 40, 20, 4) == 0);
    CHECK(modern::ring_pixel(40.5, 0, 40, 20, 4) == 1);
    CHECK(modern::ring_pixel(39, 0, 40, 20, 4) == 2);
    CHECK(modern::ring_pixel(37, 0, 40, 20, 4) == 3);
    CHECK(modern::ring_pixel(35.5, 0, 40, 20, 4) == 1);
    CHECK(modern::ring_pixel(34, 0, 40, 20, 4) == 0);
    CHECK(modern::ring_pixel(0, -19, 40, 20, 4) == 2);
    CHECK(modern::ring_pixel(0, -17, 40, 20, 4) == 3);
    // Too small for its band: no hole, and no division by nothing.
    CHECK(modern::ring_pixel(0, 0, 3, 2, 4) != 0);
}
