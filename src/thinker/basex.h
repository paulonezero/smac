// Thinker's base helpers (its base.cpp, faction.cpp, game.cpp and map.cpp: the part the governor
// reads), on OpenSMAC (thinker.h).
#pragma once
#include "thinker/compat.h"

namespace thinker {

// Facilities and projects.
bool redundant(int item_id, int faction_id);  // a project gives the faction this facility
bool has_facility(int item_id, int base_id);  // built, or redundant
int facility_count(int item_id, int faction_id);
int prod_count(int item_id, int faction_id, int base_skip_id);
bool can_build(int base_id, int item_id);  // the original's availability and Thinker's usefulness checks
bool can_build_unit(int base_id, int unit_id);

// The base.
bool base_can_riot(int base_id, bool allow_staple);
int base_unused_space(int base_id);
int mineral_output_modifier(int base_id);
int mineral_cost(int base_id, int item_id);
int hurry_cost(int base_id, int item_id, int hurry_mins);
int garrison_count(int x, int y);
int defender_count(int x, int y, int veh_skip_id);

// The faction.
bool has_wmode(int faction_id, int mode);
bool has_ships(int faction_id);
bool allow_expand(int faction_id);
bool un_charter();
bool victory_done();

// The map.
bool adjacent_region(int x, int y, int owner, int threshold, bool ocean);
bool can_build_base(int x, int y, int faction_id, int tri);
class TileSearch;
bool has_base_sites(TileSearch& ts, int x, int y, int faction_id, int tri);

// The yields (Thinker's map.cpp).
int bonus_yield(int res_type);
int total_yield(int x, int y, int faction_id);
int fungus_yield(int faction_id, int res_type);
int item_yield(int x, int y, int faction_id, int bonus, uint32_t item);

}  // namespace thinker
