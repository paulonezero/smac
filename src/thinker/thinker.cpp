// Thinker's improvements on OpenSMAC: the options and the engine's calls into them (thinker.h).
#include "thinker/thinker.h"

#include <cstdlib>
#include <cstring>

#include "engine/ai.h"
#include "engine/base.h"
#include "thinker/build.h"
#include "thinker/compat.h"
#include "thinker/move.h"

namespace thinker {

Config conf;

namespace {

struct Option {
    const char* name;
    int* value;
};

// Thinker's names (docs/thinker.ini), for the ones this layer has.
Option g_options[] = {
    {"manage_player_units", &conf.manage_player_units},
    {"manage_player_bases", &conf.manage_player_bases},
    {"base_hurry", &conf.base_hurry},
    {"skip_gov_facility", &conf.skip_gov_facility},
    {"repair_battle_ogre", &conf.repair_battle_ogre},
    {"simple_hurry_cost", &conf.simple_hurry_cost},
    {"base_spacing", &conf.base_spacing},
    {"base_nearby_limit", &conf.base_nearby_limit},
    {"max_satellites", &conf.max_satellites},
    {"debug", &conf.debug},
};
const char* g_option_names[sizeof(g_options) / sizeof(g_options[0])];

uint32_t g_random_seed = 0;
int g_upkeep_turn = -1;  // the turn move_upkeep last ran for the player

}  // namespace

bool set_option(const char* key, const char* value) {
    for (Option& o : g_options) {
        if (std::strcmp(o.name, key) == 0) {
            *o.value = std::atoi(value);
            return true;
        }
    }
    return false;
}

const char* const* option_names(int* count) {
    *count = (int)(sizeof(g_options) / sizeof(g_options[0]));
    for (int i = 0; i < *count; i++) g_option_names[i] = g_options[i].name;
    return g_option_names;
}

bool enabled() { return conf.manage_player_units || conf.manage_player_bases; }

void random_reseed(uint32_t value) { g_random_seed = value; }

// Thinker's random(): a linear congruential step, the upper bits scaled to the limit.
int32_t random(int32_t limit) {
    g_random_seed = 1664525u * g_random_seed + 1013904223u;
    return (int32_t)(((g_random_seed & 0xffff) * (uint32_t)limit) >> 16);
}

bool base_reset(int32_t base_id, int32_t has_gov) {
    if (!conf.manage_player_bases || base_id < 0 || base_id >= BaseCount) return false;
    BASE& base = Bases[base_id];
    if (base.faction_id <= 0 || !is_human(base.faction_id)) return false;
    debug("base_reset %d %d %s has_gov %d\n", CurrentTurn, base_id, base.name, has_gov);
    const int32_t choice = base_build_choice(base_id, has_gov);
    ::base_change(base_id, choice);
    return true;
}

bool base_hurry() {
    if (!conf.manage_player_bases || conf.base_hurry < 1 || CurrentBaseID < 0 || CurrentBaseID >= BaseCount) return false;
    BASE& base = Bases[CurrentBaseID];
    if (base.faction_id <= 0 || !is_human(base.faction_id)) return false;
    governor_hurry();
    return true;
}

// Thinker's mod_enemy_move, for the part of it this layer has: a player's automated former takes
// Thinker's former_move; anything else the original's enemy_move. The turn's map data is built
// at the player's first such unit each turn.
int32_t enemy_move(int32_t veh_id) {
    if (veh_id >= 0 && veh_id < VehCount) {
        debug_ver("enemy_move %d f%d %s order %d state %08X manage %d\n", veh_id, Vehs[veh_id].faction_id,
                  veh_name(&Vehs[veh_id]), Vehs[veh_id].order, Vehs[veh_id].state, conf.manage_player_units);
    }
    if (veh_id >= 0 && veh_id < VehCount && conf.manage_player_units) {
        VEH* veh = &Vehs[veh_id];
        const int faction_id = veh->faction_id;
        if (faction_id > 0 && is_human(faction_id) && mapsq(veh->x, veh->y) && is_former(veh) && !is_patrol_order(veh)) {
            if (move_upkeep_faction != faction_id || g_upkeep_turn != CurrentTurn) {
                move_upkeep(faction_id, UM_Player);
                g_upkeep_turn = CurrentTurn;
            }
            return former_move(veh_id);
        }
    }
    return ::enemy_move(veh_id);
}

}  // namespace thinker
