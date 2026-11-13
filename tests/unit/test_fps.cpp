// The frame rate indicator's arithmetic (platform/fps.h).
#include "doctest/doctest.h"
#include "platform/fps.h"

TEST_CASE("fps: a frame's work") {
    fps::Work w;
    CHECK(w.ms() == 0);
    CHECK(w.fps() == 0);  // no frame yet
    // The map drawn (6 ms), a wait (not work), the present (4 ms): a frame of 10 ms, 100 a second.
    w.begin(1000, true);
    w.end(7000, false);
    w.begin(50000, false);
    w.end(54000, true);
    CHECK(w.ms() == doctest::Approx(10.0));
    CHECK(w.fps() == 100);
    // A frame without the map (the interface shown again) is no sample.
    w.begin(60000, false);
    w.end(62000, true);
    CHECK(w.ms() == doctest::Approx(10.0));
    // Nested work counts once: the map drawn from inside the present.
    w.begin(100000, false);
    w.begin(101000, true);
    w.end(111000, false);
    w.end(120000, true);
    CHECK(w.ms() == doctest::Approx(15.0));  // 10 and 20
    CHECK(w.fps() == 67);
}

TEST_CASE("fps: what is not a frame's work") {
    // Several redraws before one frame (the other factions' moves): the last one counts.
    fps::Work w;
    uint64_t t = 0;
    for (int i = 0; i < 5; i++, t += 40000) {
        w.begin(t, true);
        w.end(t + 30000, false);
    }
    w.begin(t, true);
    w.end(t + 6000, false);
    w.begin(t + 10000, false);
    w.end(t + 14000, true);
    CHECK(w.ms() == doctest::Approx(10.0));
    // Work that draws neither the map nor a frame (the world map's own redraw) is left out.
    w.begin(500000, false);
    w.end(590000, false);
    w.begin(600000, true);
    w.end(606000, false);
    w.begin(610000, false);
    w.end(614000, true);
    CHECK(w.ms() == doctest::Approx(10.0));
    // The wait for the display inside a frame is taken out: 30 ms with 25 of them held.
    fps::Work h;
    h.begin(1000, true);
    h.exclude(25000);
    h.end(31000, true);
    CHECK(h.ms() == doctest::Approx(5.0));
    // 150 ms or more is a load: no sample. Just under is one.
    fps::Work l;
    l.begin(0, true);
    l.end(260000, true);
    CHECK(l.size == 0);
    l.begin(300000, true);
    l.end(449000, true);
    CHECK(l.size == 1);
    // The median of the last eight: one slow frame among quick ones does not show.
    fps::Work m;
    t = 0;
    for (int i = 0; i < 8; i++, t += 400000) {
        m.begin(t, true);
        m.end(t + (i == 3 ? 120000 : 5000), true);
    }
    CHECK(m.ms() == doctest::Approx(5.0));
    CHECK(m.fps() == 200);
    // An end without its begin does nothing.
    m.end(t, true);
    CHECK(m.depth == 0);
}

TEST_CASE("fps: digits") {
    // An 8 has every dot of a 0 and the middle one; a 1's top row is the middle dot alone.
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 3; x++)
            if (fps::dot(0, x, y)) CHECK(fps::dot(8, x, y));
    CHECK(fps::dot(8, 1, 2));
    CHECK(!fps::dot(0, 1, 2));
    CHECK(!fps::dot(1, 0, 0));
    CHECK(fps::dot(1, 1, 0));
    CHECK(!fps::dot(1, 2, 0));
}
