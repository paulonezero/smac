// Thinker's improvements on OpenSMAC: the options and the engine's calls into them (thinker.h).
#include "thinker/thinker.h"

#include <cstdlib>
#include <cstring>

#include "engine/ai.h"
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
    {"repair_battle_ogre", &conf.repair_battle_ogre},
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
