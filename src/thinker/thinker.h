// Thinker's improvements on OpenSMAC (OpenSMAC's own, not in the original).
//
// Thinker (https://github.com/induktio/thinker, MIT licence, "Thinker Mod authors") is a patch for
// the Windows game: a DLL that redirects the game's calls into its own C++ code. That code reads the
// game's structures through headers OpenSMAC's own descend from, so it ports: a feature's logic is
// copied under src/thinker/ with Thinker's spelling of the engine's helpers mapped onto OpenSMAC's
// (compat.h), and where Thinker redirects a call, the engine calls this layer instead, which calls
// the original's code unless the feature is on.
//
// Every feature is off unless its option is set, and the engine runs as the original then: the
// tests' comparisons with the original stay valid. A player's game reads the options from the
// [Thinker] section of Alpha Centauri.Ini (Thinker's own option names and values); a scripted run
// takes them from --thinker KEY=VALUE.
//
// Features so far:
//   manage_player_units=1   the player's automated formers run Thinker's former logic
//                           (move.h: former_move), not the original's
#pragma once
#include <cstdint>

namespace thinker {

struct Config {
    int manage_player_units = 0;
    int manage_player_bases = 0;
    int base_hurry = 0;
    int tile_output_limit[3] = {2, 2, 2};  // the original's yield caps before the restriction techs
    int repair_battle_ogre = 0;  // Thinker's: battle ogres repair (1) and use monoliths (10)
    int debug = 0;  // the former decisions on stderr
};
extern Config conf;

// An option by Thinker's ini name ("manage_player_units", "1"); false for a name not known here.
bool set_option(const char* key, const char* value);
// The option names, for reading them from an ini section; *count receives how many.
const char* const* option_names(int* count);
// True when any feature is on.
bool enabled();

// enemy_veh's call in place of enemy_move: Thinker's move for the unit where a feature applies
// (the player's automated formers with manage_player_units), else the original's enemy_move.
int32_t enemy_move(int32_t veh_id);

}  // namespace thinker
