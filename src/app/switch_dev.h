// The Nintendo Switch's development loop (CMake: OPENSMAC_SWITCH_DEV, off by default): the game
// driven from a computer over Wi-Fi, with no hand on the console (packaging/switch/dev.py).
//
// Two listeners, started only in a game sent by nxlink (never one started from the console's own
// menus). TCP 28280 takes a new build as the Homebrew Menu's NetLoader does: the game quits into
// it. TCP 28282 takes one command a connection:
//   keys TOKENS      scripted input, the tokens of --keys (app/opensmac.cpp), run after those waiting
//   tap X Y          a touch at X,Y of the 1280 x 720 screen, through the platform's input path
//   key CODE         a key press through the platform's input path (platform::key_inject)
//   shot             a screenshot: its size (int32), then the BMP file
//   put SIZE NAME    SIZE bytes follow: written to /switch/smac/NAME
//   get NAME         /switch/smac/NAME: its size (int32, -1: none), then its bytes
//   quit             the game ends (back to waiting for a build)
// There is no authentication: anyone on the network can run a program on the console or press its
// keys. Never build it into a copy given to somebody else.
#pragma once
#include <string>

namespace switchdev {

// The --keys tokens (comma separated) queued for the game's thread; false: an unknown token.
using KeysFn = bool (*)(const std::string& tokens);
// Starts the listeners (once; nothing without nxlink). keys: how "keys" and "shot" reach the game.
void start(KeysFn keys);
// Before the program returns to the loader: the threads end, and a game that ended by itself is
// followed by a copy that only waits for the next build.
void stop();
// --dev-wait: no game, only the listeners, until a build arrives.
int wait_for_build();

}  // namespace switchdev
