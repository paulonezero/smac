// Smooth movement of the main map view (OpenSMAC's own, not in the original).
//
// The original moves the view a whole step at a time. Here the picture travels: a move of the view
// (pfncMoveMap) glides there, and the mouse at a screen edge or two fingers on a trackpad pan it by
// the pixel, settling on the tile grid when they stop. The map is redrawn for every frame, so its
// units and names stay live.
//
// The game does not see this. A glide sets the view at once, as the original, and only the picture
// lags behind while the game waits for it. A pan runs only from the main loop, with the game
// waiting for input, and clicks during one find the right tile (MapWin pixel_x / pixel_y, which
// the tile <-> point conversions honour). At rest the view is exactly where the original has it.
#pragma once
#include <cstdint>
#include <string>

#include "ui/smoothview_math.h"

namespace smoothview {

// Off: the original's moves. `unattended`: also for scripted runs without a focused window (tests).
void set_enabled(bool on, bool unattended);

// pfncZoomProcessing's end: a view that is off the tile grid applies it here.
void apply(uint8_t* win);

// pfncMoveMap: whether this move of `win` is to glide (*from: where the picture is now). Any pan
// ends here.
bool capture(uint8_t* win, Pos* from);
// The view is set; the picture travels there from *from and ends with the full redraw.
void glide(uint8_t* win, Pos from, int32_t redraw);

// True while the view is off the tile grid.
bool panning();
// True while a pan wants frames: the main loop should not wait long for input.
bool busy();

void mouse(int32_t x, int32_t y);  // the mouse's latest position
void wheel(float dx, float dy);  // a scroll wheel or two fingers on a trackpad
void tick();  // the main loop

// Test aid (VIEWCHECK): every tile in view back through pfncPtToTile. Mismatches on stderr.
void check();

}  // namespace smoothview
