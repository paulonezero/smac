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
//   manage_player_bases=1   the player's governed bases take Thinker's choice of production
//                           (build.h: select_build), within the governor's own settings
//   base_hurry=1|2          with it, the governor hurries facilities (1), projects too (2), by
//                           Thinker's rules (build.h: governor_hurry)
#pragma once
#include <cstdint>

namespace thinker {

struct Config {
    int manage_player_units = 0;
    int manage_player_bases = 0;
    int base_hurry = 0;  // with manage_player_bases: the governor hurries facilities (1), projects too (2)
    int skip_gov_facility = 0;  // bit (id - 1) set: the governor never builds facility id
    int tile_output_limit[3] = {2, 2, 2};  // the original's yield caps before the restriction techs
    int repair_battle_ogre = 0;  // Thinker's: battle ogres repair (1) and use monoliths (10)
    // Thinker's rule options the ported code reads, at the values that keep the original's rules.
    int simple_hurry_cost = 0;
    int base_spacing = 3;
    int base_nearby_limit = -1;
    int expansion_limit = 100;
    int limit_project_start = 0;
    int max_satellites = 20;
    int ignore_reactor_power = 0;
    int long_range_artillery = 0;
    int modify_unit_support = 0;
    int clean_minerals = 16;
    int biology_lab_bonus = 2;
    int neural_amplifier_bonus = 50;
    int dream_twister_bonus = 50;
    int repair_base_facility = 10;
    int max_veh_num = 2048;
    int debug = 0;  // the decisions on stderr
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
// base_reset's start: with manage_player_bases, a player's governed base takes Thinker's choice
// of production (true: done, the original's choice is skipped); else false.
bool base_reset(int32_t base_id, int32_t has_gov);
// base_hurry's start: with manage_player_bases and base_hurry, the player's governor hurries by
// Thinker's rules (true: done); else false for the original's.
bool base_hurry();

}  // namespace thinker
