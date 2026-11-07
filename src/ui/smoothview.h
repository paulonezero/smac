// Smooth movement of the main map view (OpenSMAC's own, not in the original).
//
// The original moves the view a whole step at a time. Here the picture travels: a move of the view
// (pfncMoveMap) glides there, the mouse at a screen edge pans it by the pixel, settling on the tile
// grid when it stops, and a change of zoom scales the picture about the point that stays put. Two
// fingers on a trackpad (or a scroll wheel) zoom, about the pointer. The map is redrawn for every
// frame, so its units and names stay live.
//
// The game does not see this. A glide sets the view at once, as the original, and only the picture
// lags behind while the game waits for it. A pan runs only from the main loop, with the game
// waiting for input, and clicks during one find the right tile (MapWin pixel_x / pixel_y, which
// the tile <-> point conversions honour). At rest the view is where the original has it, but for
// a change of zoom: the view stays where the zoom left it, off the grid, until it next moves. It
// is a place to look from and nothing more: games played by keys are byte-identical with this
// on and off. A scripted click at a fixed screen position can hit another tile after a zoom.
#pragma once
#include <cstdint>
#include <string>

#include "ui/smoothview_math.h"

namespace smoothview {

// Off: the original's moves. `unattended`: also for scripted runs without a focused window (tests).
void set_enabled(bool on, bool unattended);
// The wheel's zoom: 1 (a scroll up zooms in, the default), -1 (the reverse), 0 (none). It works
// with the smooth view off too, then with the original's redraw.
void set_wheel_zoom(int32_t direction);

// pfncZoomProcessing's end: a view that is off the tile grid applies it here.
void apply(uint8_t* win);

// pfncMoveMap: whether this move of `win` is to glide (*from: where the picture is now). Any pan
// ends here.
bool capture(uint8_t* win, Pos* from);
// The view is set; the picture travels there from *from and ends with the full redraw.
void glide(uint8_t* win, Pos from, int32_t redraw);

// A change of zoom of `win`: zoom_capture before it (false: the original's redraw), zoom_glide
// after it. The map point at the anchor (the screen's centre; the pointer for the wheel's zoom)
// stays there, the picture scaling to the new size. The original puts the view on the tile grid
// of the new zoom, moving that point by up to a tile at each step; here the view stays off the
// grid until it next moves.
struct ZoomFrom {
    Pos at;
    int32_t half_w = 0, half_h = 0, zoom = 0;
    double anchor_x = 0, anchor_y = 0;  // screen pixels
};
bool zoom_capture(uint8_t* win, ZoomFrom* from);
void zoom_glide(uint8_t* win, const ZoomFrom& from);

// Units and bases scaled with the tiles while enabled. The tiles' sizes come in steps of eight
// pixels (pfncZoomProcessing's q) but the sprites and their offsets scale by (zoom + 16) / 16,
// so at each zoom a unit sits a few pixels differently on its tile: what a zoom's scaling picture
// cannot foresee, a hop at its end. ArtScale sets the engine's fixed zoom factor (ZoomNum /
// ZoomDenom, as the base window does) to q / 12 (q is 12 at zoom 0, the sprites' own size) for
// the duration, when none is set. Off (or the original's moves): nothing.
// The terrain sprites' scale (BlitDestScale / BlitSourceScale: monoliths, pods, farms, mines...)
// for a tile of `win` at `zoom`: the tiles' own scale while enabled, else the original's.
void sprite_scale(uint8_t* win, int32_t zoom);

struct ArtScale {
    explicit ArtScale(uint8_t* win);
    ~ArtScale();
    bool set = false;
    int32_t num = 0, den = 0;  // the values restored
};

// True while the view is off the tile grid.
bool panning();
// True while a pan wants frames: the main loop should not wait long for input.
bool busy();

void mouse(int32_t x, int32_t y);  // the mouse's latest position
void wheel(float dx, float dy);  // a scroll wheel or two fingers on a trackpad: zoom steps
void tick();  // the main loop

// Test aid (VIEWCHECK): every tile in view back through pfncPtToTile. Mismatches on stderr.
void check();
// Test aid (VIEWPOINT): where the tile that was at the view's centre at the first call is now.
void point();

}  // namespace smoothview
