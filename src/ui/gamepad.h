// A controller as the game's keyboard and mouse (the scheme: ui/padmap.h). The platform delivers
// the buttons (EV_PAD_DOWN / EV_PAD_UP) and keeps the sticks; this turns them into the key and
// mouse events the windows already take, by what has the input, and moves the drawn pointer.
#pragma once
#include <deque>

#include "platform/ui_platform.h"

// A button event: the key or mouse events it stands for are added to out.
void gamepad_event(const platform::Event& ev, std::deque<platform::Event>* out);
// Each pass of the message pump: the sticks (unit or cursor steps, the pointer, the map's pan) and
// the repeats of held buttons.
void gamepad_tick(std::deque<platform::Event>* out);
// A stick is pushed or a button held: the pump must come round soon.
bool gamepad_busy();
// The mouse or the touch screen was used: the drawn pointer is hidden.
void gamepad_mouse_used();
