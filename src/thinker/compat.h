// Thinker's spelling of the engine on OpenSMAC (thinker.h).
//
// Thinker gives MAP, VEH and UNIT member functions (sq->is_base(), veh->triad()); OpenSMAC's
// structs are the engine's own, so here they are free functions of the same names taking the
// pointer. Its globals are pointers (*CurrentTurn, Rules->); OpenSMAC's are references
// (CurrentTurn, Rules.), and the ported code is written that way. The engine functions Thinker
// calls under the original's names are brought into the namespace so that overloads here do not
// hide them.
#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <unordered_map>
#include <vector>

#include "engine/action.h"
#include "engine/base.h"
#include "engine/base_misc.h"
#include "engine/battle.h"
#include "engine/game_state.h"
#include "engine/globals.h"
#include "engine/map.h"
#include "engine/score.h"
#include "engine/stack.h"
#include "engine/tech.h"
#include "engine/veh.h"
#include "engine/zoc.h"
#include "thinker/thinker.h"

namespace thinker {

using std::clamp;
using std::max;
using std::min;

using ::bonus_at;
using ::crop_yield;
using ::energy_yield;
using ::goody_at;
using ::has_tech;
using ::mine_yield;
using ::region_at;
using ::terraform_cost;
using ::veh_cargo;
using ::veh_kill;
using ::veh_skip;
using ::whose_territory;
using ::zoc_any;

// enemy_move's results (Thinker's names): 0 lets the unit go on this turn, 1 ends its move.
enum VehReturn { VEH_SYNC = 0, VEH_SKIP = 1 };

// ---- containers ----------------------------------------------------------------------------------

struct Point {
    int x;
    int y;
    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
    bool operator<(const Point& o) const { return x < o.x || (x == o.x && y < o.y); }
};

struct PointHash {
    std::size_t operator()(const Point& p) const { return (std::size_t)(p.x ^ (p.y << 16)); }
};

struct MapNode {
    int x;
    int y;
    int type;
    bool operator<(const MapNode& o) const {
        return x < o.x || (x == o.x && (y < o.y || (y == o.y && type < o.type)));
    }
};

struct MapTile {
    int x;
    int y;
    int i;
    MAP* sq;
};

// A tile's values for the turn (move_upkeep).
struct PInfo {
    int former = 0;
    int safety = 0;
    int target = 0;
    int roads = 0;
    int flags = 0;
    int unit_near = 0;
    int unit_path = 0;
    int enemy = 0;
    int enemy_near = 0;
    int enemy_dist = 0;
    int enemy_rank = 0;
    int overlay = 0;
    int get_enemy_dist() const { return enemy_dist > 0 ? enemy_dist : MaxEnemyRange / 2; }
};

struct MItem {
    int x;
    int y;
    int score;
    bool operator<(const MItem& o) const {
        return score < o.score || (score == o.score && (x < o.x || (x == o.x && y < o.y)));
    }
};

typedef std::unordered_map<Point, PInfo, PointHash> PMTable;
typedef std::set<MapNode> NodeSet;
typedef std::set<Point> Points;
typedef std::vector<Point> PointList;
typedef std::map<int, int> map_int_t;
typedef std::priority_queue<MItem, std::vector<MItem>, std::less<MItem>> point_max_queue_t;

// ---- the map -------------------------------------------------------------------------------------

inline bool map_is_flat() { return (game().map.flat & 1) != 0; }

// The tile at (x, y), or null off the map (and for an x, y of different parity: no tile there).
inline MAP* mapsq(int x, int y) {
    if (x >= 0 && y >= 0 && x < MapAreaX && y < MapAreaY && !((x + y) & 1)) return map_tile(x, y);
    return nullptr;
}

inline int wrap(int x) {
    if (!map_is_flat()) {
        if (x >= 0) {
            if (x >= MapAreaX) x -= MapAreaX;
        } else {
            x += MapAreaX;
        }
    }
    return x;
}

inline int x_dist(int x1, int x2) {
    int dx = std::abs(x1 - x2);
    if (!map_is_flat() && dx > MapAreaX / 2) dx = MapAreaX - dx;
    return dx;
}

inline int map_range(int x1, int y1, int x2, int y2) {
    int dx = std::abs(x1 - x2);
    int dy = std::abs(y1 - y2);
    if (!map_is_flat() && dx > MapAreaX / 2) dx = MapAreaX - dx;
    return (dx + dy) / 2;
}

inline int alt_level(const MAP* sq) { return sq->climate >> 5; }
inline int code_at(const MAP* sq) { return (int)(sq->landmarks >> 24); }
inline int lm_items(const MAP* sq) { return (int)(sq->landmarks & 0xFFFF); }
inline bool is_owned(const MAP* sq) { return sq->owner >= 0; }
inline int sq_veh_owner(const MAP* sq) { return (sq->val2 & 0xF) >= MaxPlayerNum ? -1 : (sq->val2 & 0xF); }
inline int sq_veh_who(const MAP* sq) { return (sq->items & BIT_VEH_IN_TILE) ? sq_veh_owner(sq) : -1; }
inline int sq_anything_at(const MAP* sq) {
    return (sq->items & (BIT_VEH_IN_TILE | BIT_BASE_IN_TILE)) ? sq_veh_owner(sq) : -1;
}
inline bool is_base(const MAP* sq) { return (sq->items & BIT_BASE_IN_TILE) && sq_veh_owner(sq) >= 0; }
inline bool is_airbase(const MAP* sq) { return is_base(sq) || (sq->items & BIT_AIRBASE); }
inline bool is_base_radius(const MAP* sq) { return (sq->items & BIT_BASE_RADIUS) != 0; }
inline bool is_fungus(const MAP* sq) { return (sq->items & BIT_FUNGUS) && alt_level(sq) >= ALT_OCEAN_SHELF; }
inline bool is_rocky(const MAP* sq) { return (sq->val3 & TILE_ROCKY) && alt_level(sq) >= ALT_SHORE_LINE; }
inline bool is_rolling(const MAP* sq) { return (sq->val3 & TILE_ROLLING) != 0; }
inline bool is_rainy(const MAP* sq) { return (sq->climate & TILE_RAINY) != 0; }
inline bool is_moist(const MAP* sq) { return (sq->climate & TILE_MOIST) != 0; }
inline bool is_rainy_or_moist(const MAP* sq) { return (sq->climate & (TILE_MOIST | TILE_RAINY)) != 0; }
inline bool volcano_center(const MAP* sq) { return (sq->landmarks & LM_VOLCANO) && code_at(sq) == 0; }

inline bool is_ocean(const MAP* sq) { return alt_level(sq) < ALT_SHORE_LINE; }
inline bool is_ocean(int x, int y) { return is_ocean(mapsq(x, y)); }

// ---- factions ------------------------------------------------------------------------------------

inline bool is_human(int faction_id) { return (FactionStatus[0] & (1u << (faction_id & 31))) != 0; }
inline bool is_alive(int faction_id) { return (FactionStatus[1] & (1u << (faction_id & 31))) != 0; }
inline bool at_war(int f1, int f2) {
    return f1 != f2 && f1 >= 0 && f2 >= 0 && (Factions[f1].diplo_status[f2] & DIPLO_VENDETTA);
}
inline bool has_pact(int f1, int f2) { return f1 >= 0 && f2 >= 0 && (Factions[f1].diplo_status[f2] & DIPLO_PACT); }
inline bool both_neutral(int f1, int f2) {
    return f1 >= 0 && f2 >= 0 && f1 != f2 && !(Factions[f1].diplo_status[f2] & (DIPLO_PACT | DIPLO_VENDETTA));
}
inline bool is_aquatic(int faction_id) { return (MFactions[faction_id].rule_flags & RFLAG_AQUATIC) != 0; }
// Whether the faction owns the secret project: Thinker names it by its facility id (FAC_*, 70 and
// up); the engine's has_project takes the project's index in SecretProjects.
inline bool has_project(int facility_id, int faction_id) {
    return facility_id >= SP_ID_First && facility_id <= SP_ID_Last &&
           ::has_project(facility_id - SP_ID_First, faction_id) != 0;
}
// Whether the faction can do terraforming `item` on land or at sea (the original's terrain_avail).
inline bool has_terra(int item, bool sea, int faction_id) { return terrain_avail(item, sea, faction_id) != 0; }
inline bool has_fac_built(int facility_id, int base_id) {
    return base_id >= 0 && base_id < BaseCount && facility_id >= 0 && facility_id < 8 * 12 &&
           (Bases[base_id].facilities_built[facility_id / 8] & (1 << (facility_id % 8)));
}

// ---- units ---------------------------------------------------------------------------------------

inline const char* veh_name(const VEH* v) { return Units[v->unit_id].name; }
inline int triad(const VEH* v) { return Chassis[Units[v->unit_id].chassis_id].triad; }
inline int chassis_speed(const VEH* v) { return Chassis[Units[v->unit_id].chassis_id].speed; }
inline int chassis_range(const VEH* v) { return Chassis[Units[v->unit_id].chassis_id].range; }
inline int veh_plan(const VEH* v) { return Units[v->unit_id].plan; }
inline int reactor_type(const VEH* v) { return min(4, max(1, (int)Units[v->unit_id].reactor_id)); }
inline int weapon_mode(const VEH* v) { return Weapon[Units[v->unit_id].weapon_id].mode; }
inline int offense_value(const VEH* v) { return Weapon[Units[v->unit_id].weapon_id].offense_value; }
inline bool is_missile(const VEH* v) { return Chassis[Units[v->unit_id].chassis_id].missile != 0; }
inline bool is_combat_unit(const VEH* v) { return offense_value(v) != 0 && v->unit_id != BSC_FUNGAL_TOWER; }
inline bool is_colony(const VEH* v) { return veh_plan(v) == PLAN_COLONY; }
inline bool is_former(const VEH* v) { return veh_plan(v) == PLAN_TERRAFORM; }
inline bool is_probe(const VEH* v) { return veh_plan(v) == PLAN_PROBE; }
inline bool is_supply(const VEH* v) { return veh_plan(v) == PLAN_SUPPLY; }
inline bool is_transport(const VEH* v) { return veh_plan(v) == PLAN_NAVAL_TRANSPORT; }
inline bool is_artifact(const VEH* v) { return veh_plan(v) == PLAN_ARTIFACT; }
inline bool is_native_unit(const VEH* v) { return v->unit_id < MaxProtoFactionNum && offense_value(v) < 0; }
inline bool is_battle_ogre(int unit_id) {
    return unit_id == BSC_BATTLE_OGRE_MK1 || unit_id == BSC_BATTLE_OGRE_MK2 || unit_id == BSC_BATTLE_OGRE_MK3;
}
inline bool can_repair(int unit_id) { return conf.repair_battle_ogre > 0 || !is_battle_ogre(unit_id); }
inline bool can_monolith(int unit_id) { return conf.repair_battle_ogre >= 10 || !is_battle_ogre(unit_id); }
inline bool plr_owner(const VEH* v) { return is_human(v->faction_id); }
inline bool at_target(const VEH* v) {
    return v->order == ORDER_NONE || v->order == ORDER_HOLD || (v->waypoint_x[0] < 0 && v->waypoint_y[0] < 0) ||
           (v->x == v->waypoint_x[0] && v->y == v->waypoint_y[0] && !v->waypoint_count);
}
inline bool mid_damage(const VEH* v) { return v->damage_taken > 2 * Units[v->unit_id].reactor_id; }
inline bool need_heals(const VEH* v) { return mid_damage(v) && can_repair(v->unit_id); }
inline bool need_monolith(const VEH* v) {
    return triad(v) != TRIAD_AIR && can_monolith(v->unit_id) &&
           (need_heals(v) || (!(v->state & VSTATE_MONOLITH_UPGRADED) && v->morale < MORALE_ELITE && offense_value(v) != 0));
}
inline bool need_refuel(const VEH* v) {
    return triad(v) == TRIAD_AIR && !is_missile(v) && chassis_range(v) != 0 && chassis_range(v) <= v->movement_turns + 1;
}
inline bool is_patrol_order(const VEH* v) {
    return (v->order == ORDER_NONE || v->order == ORDER_MOVE_TO) && v->waypoint_count > 0 &&
           !(v->state & VSTATE_EXPLORE) &&
           (v->state & (VSTATE_UNK_1000000 | VSTATE_ON_ALERT)) == (VSTATE_UNK_1000000 | VSTATE_ON_ALERT);
}
inline int veh_speed(int veh_id, int skip_morale) { return speed(veh_id, skip_morale); }

// ---- random --------------------------------------------------------------------------------------

// Thinker's own generator (its random()), apart from the game's so that the game's sequence of
// random numbers is the same with a feature on and off. Reseeded by move_upkeep from the turn.
void random_reseed(uint32_t value);
int32_t random(int32_t limit);

}  // namespace thinker

// Thinker's debug log: on stderr with the option debug=1.
#define debug(...)                                                      \
    do {                                                                \
        if (thinker::conf.debug) {                                      \
            std::fprintf(stderr, "thinker: " __VA_ARGS__);              \
        }                                                               \
    } while (0)
#define debug_ver(...) debug(__VA_ARGS__)
