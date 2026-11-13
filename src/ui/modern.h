// Modern unit animations (OpenSMAC's own, not in the original): how the selected unit is marked
// and how a unit moves. Each can be switched off for the original's look, and neither is seen by
// the game: a game played by keys saves the same with them on and off.
//
// The selection ring: the original marks the unit that waits for orders by blinking it (hidden,
// or without its flag, every other 150 ms, each time with a redraw of the map). Here the unit
// stays, standing on a ring in its faction's colour that breathes. The ring and the unit over it
// are drawn over the map's picture at every presentation, so the map is not drawn again for it.
//
// Unit moves: the original turns a unit on the spot in steps, then slides it to the next tile at a
// constant speed, tile by tile. Here the move follows the clock: the unit gathers speed and comes
// to rest, turns as it goes (a turn of more than a quarter starts on the spot), and the tiles of a
// longer move join at a constant speed (scoot.cpp).
#pragma once
#include <cstdint>

struct Buffer;

namespace modern {

// ring: the selection ring in place of the blink; moves: the eased moves. Off: the original's.
void set_enabled(bool ring, bool moves);
bool ring();
bool moves();

// The ring's unit as the main view draws it (MapWin_draw_vehicles, which leaves the unit out):
// `veh` on the tile (x, y), drawn at (px, py) (veh_draw's point), the tile's centre `lift` pixels
// above the flat map.
void ring_place(int32_t veh, int32_t x, int32_t y, int32_t px, int32_t py, int32_t lift);
// A full redraw of the main view begins: no unit placed yet.
void ring_reset();
// The view's picture moved by (-dx, -dy) (a scrolled redraw).
void ring_shift(int32_t dx, int32_t dy);
// Before a presentation draws the map: a change of the unit that waits for orders has the map
// drawn again (with the unit left out, or put back).
void ring_check();
// The ring and its unit over the map's picture in `dest` (the screen at a presentation; the main
// view's canvas for the picture a zoom scales), inside (0, 0, width, height).
void ring_draw(Buffer* dest, int32_t width, int32_t height);
// The main loop: true when the ring wants a frame, at most one every `every_ms` (33: about 30 a
// second; less when the frames are held for the display, which then spaces them).
bool ring_tick(uint32_t now_ms, int32_t every_ms = 33);
// The ring shows and breathes (ring_tick would ask for frames).
bool ring_active();

}  // namespace modern
