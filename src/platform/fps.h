// The frame rate indicator's arithmetic and digits (pure; drawn by the platform's present).
#pragma once
#include <cstdint>

namespace fps {

// What a frame costs to draw. The game draws when something changes, so the frames it shows in a
// second say how often it was asked, not how fast it is; the time spent drawing one does. Work is
// bracketed by begin and end (they nest: the outermost pair is timed, and time between pairs, the
// waits, is not). A pair that ends a frame (end's `frame`: a presentation) gives a sample when the
// main map was drawn for that frame (begin's `map`), inside it or before it: the frame's time and
// that of the last map redraw before it. Several redraws before one frame (the other factions'
// moves) count once, and work that is neither is left out. A frame without the map (the interface
// shown again) is no sample, and one of 150 ms or more is a load or a stall, not a frame (the first
// frames of a game take a quarter of a second on the Switch, and stood as the rate until the map moved). ms() is the
// median of the last kSamples, fps() what that would allow in a second.
struct Work {
    static constexpr int kSamples = 8;
    static constexpr uint64_t kLoad = 150000;
    int depth = 0;
    uint64_t start = 0, excluded = 0;  // microseconds
    bool outer_map = false, saw_map = false;
    uint64_t pending_map = 0;  // the last map redraw outside a frame
    uint64_t sample[kSamples] = {};
    int head = 0, size = 0;
    void begin(uint64_t now_us, bool map_drawn) {
        if (depth++ == 0) {
            start = now_us;
            excluded = 0;
            outer_map = map_drawn;
        }
        if (map_drawn) saw_map = true;
    }
    // Time inside the work that was a wait (the present held for the display's refresh).
    void exclude(uint64_t us) {
        if (depth > 0) excluded += us;
    }
    void end(uint64_t now_us, bool frame) {
        if (depth <= 0) return;
        if (--depth != 0) return;
        const uint64_t spent = now_us - start, took = spent > excluded ? spent - excluded : 0;
        if (!frame) {
            if (outer_map) pending_map = took;
            return;
        }
        if (saw_map && took + pending_map < kLoad) {
            sample[head] = took + pending_map;
            head = (head + 1) % kSamples;
            if (size < kSamples) size++;
        }
        pending_map = 0;
        saw_map = false;
    }
    double ms() const {  // the median: one slow frame does not show
        if (size == 0) return 0;
        uint64_t v[kSamples];
        for (int i = 0; i < size; i++) v[i] = sample[i];
        for (int i = 1; i < size; i++)
            for (int j = i; j > 0 && v[j] < v[j - 1]; j--) {
                const uint64_t t = v[j];
                v[j] = v[j - 1];
                v[j - 1] = t;
            }
        return (double)(v[(size - 1) / 2] + v[size / 2]) / 2000.0;
    }
    int fps() const {
        const double m = ms();
        return m > 0 ? (int)(1000.0 / m + 0.5) : 0;
    }
};

// A digit's 3x5 dots: bit 14 the top left, row by row.
constexpr uint16_t kDigit[10] = {0x7B6F, 0x2C97, 0x73E7, 0x73CF, 0x5BC9, 0x79CF, 0x79EF, 0x7249, 0x7BEF, 0x7BCF};
inline bool dot(int digit, int x, int y) { return (kDigit[digit] >> (14 - (y * 3 + x))) & 1; }

}  // namespace fps
