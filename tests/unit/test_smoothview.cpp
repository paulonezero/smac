// The smooth map view's arithmetic (ui/smoothview_math.h).
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

TEST_CASE("smoothview: the mouse at a screen edge") {
    CHECK(edge_axis(0, 1470, 2) == -1);
    CHECK(edge_axis(1, 1470, 2) == -1);
    CHECK(edge_axis(2, 1470, 2) == 0);
    CHECK(edge_axis(1467, 1470, 2) == 0);
    CHECK(edge_axis(1468, 1470, 2) == 1);
    CHECK(edge_axis(1469, 1470, 2) == 1);
}
