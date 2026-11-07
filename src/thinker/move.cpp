// Thinker's unit movement (thinker/move.h). From Thinker's move.cpp, plan.cpp (former_plans) and
// map.cpp (the yields), with the engine's names as compat.h spells them; the logic is Thinker's.
// move_upkeep is the part a player's formers need (UM_Player): the AI's invasion and defence
// planning, which Thinker runs for its own factions (UM_Full), is not ported.
#include "thinker/move.h"

#include <climits>

namespace thinker {

PMTable mapdata;
NodeSet mapnodes;
int move_upkeep_faction = -1;

namespace {

std::set<int> region_enemy;
std::set<int> region_probe;
Points nonally;
int nonally_veh_count = -1;
int nonally_faction_id = -1;

void adjust_former(int x, int y, int range, int value) {
    for (auto& m : iterate_tiles(x, y, 0, TableRange[range])) mapdata[{m.x, m.y}].former += value;
}

void adjust_safety(int x, int y, int range, int value) {
    for (auto& m : iterate_tiles(x, y, 0, TableRange[range])) mapdata[{m.x, m.y}].safety += value;
}

void adjust_shore(int x, int y, int range, int value) {
    for (auto& m : iterate_tiles(x, y, 1, TableRange[range])) mapdata[{m.x, m.y}].flags += value;
}

void adjust_unit_near(int x, int y, int range, int value) {
    for (auto& m : iterate_tiles(x, y, 0, TableRange[range])) mapdata[{m.x, m.y}].unit_near += value;
}

void adjust_enemy_near(int x, int y, int range, int value) {
    for (auto& m : iterate_tiles(x, y, 0, TableRange[range])) mapdata[{m.x, m.y}].enemy_near += value;
}

bool stack_search(int x, int y, int faction_id, StackType type, int mode) {
    bool found = false;
    for (int i = VehCount - 1; i >= 0; --i) {
        VEH* veh = &Vehs[i];
        if (veh->x == x && veh->y == y) {
            if (type == ST_EnemyOneUnit && (found || !at_war(faction_id, veh->faction_id))) return false;
            found = true;
            if (type == ST_NeutralOnly && (faction_id == veh->faction_id || !both_neutral(faction_id, veh->faction_id)))
                return false;
            if (type == ST_NonPactOnly && (faction_id == veh->faction_id || has_pact(faction_id, veh->faction_id)))
                return false;
            if (mode == WMODE_COMBAT && weapon_mode(veh) > WMODE_MISSILE) return false;
            if (mode != WMODE_COMBAT && weapon_mode(veh) != mode) return false;
        }
    }
    return found;
}

bool can_alter_level(int x, int y, int faction_id, bool raise) {
    MAP* sq = mapsq(x, y);
    if (!sq) return false;
    int target_alt = alt_level(sq) + (raise ? 1 : -1);
    int dist = 0;
    if (raise) {
        if (target_alt > 3) dist = min(3, target_alt - 3);
    } else {
        target_alt = max(0, target_alt);
        if (target_alt < 3) dist = 4 - target_alt;
    }
    int num = TableRange[clamp(dist, 0, MaxTableRange)];
    for (auto& m : iterate_tiles(x, y, 0, num)) {
        int owner = whose_territory(faction_id, m.x, m.y, nullptr, 0);
        if (owner >= 0 && owner != faction_id) {
            if (Factions[faction_id].diplo_status[owner] & (DIPLO_TRUCE | DIPLO_TREATY | DIPLO_PACT)) return false;
        }
    }
    return true;
}

// Ocean refers only to main_sea_region
int ocean_coast_tiles(int x, int y) { return (mapdata[{x, y}].flags / PM_ShoreLine) & PM_ShoreMask; }

// ---- terraforming choices ------------------------------------------------------------------------

bool can_bridge(int x, int y, int faction_id, MAP* sq) {
    if (is_ocean(sq) || !has_terra(FORMER_RAISE_LAND, TRIAD_LAND, faction_id) ||
        (is_human(faction_id) && !(GamePreferences & PREF_AUTO_FORMER_RAISE_LWR_TERRAIN)) ||
        (sq->owner != faction_id && !at_war(faction_id, sq->owner)) || !can_alter_level(x, y, faction_id, true)) {
        return false;
    }
    int alt = alt_level(sq);
    if (mapnodes.count({x, y, NODE_GOAL_RAISE_LAND}) && alt >= ALT_SHORE_LINE &&
        alt <= ALT_SHORE_LINE + (mapdata[{x, y}].get_enemy_dist() < 10) && near_sea_coast(x, y)) {
        return true;
    }
    if (coast_tiles(x, y) < 3 || (sq->landmarks & LM_JUNGLE)) return false;
    const int range = 4;
    TileSearch ts;
    ts.init(x, y, TRIAD_LAND);
    int i = 0;
    while (++i <= 120 && ts.get_next() != nullptr);
    int n = 0;
    for (auto& m : iterate_tiles(x, y, 0, TableRange[range])) {
        if (!is_ocean(m.sq) && m.y > 0 && m.y < MapAreaY - 1 && !ts.oldtiles.count({m.x, m.y})) {
            n++;
            if (is_owned(m.sq) && m.sq->owner != faction_id && !compare_might(faction_id, m.sq->owner)) return false;
        }
    }
    debug("bridge %d x: %d y: %d tiles: %d\n", faction_id, x, y, n);
    return n > 4;
}

bool can_borehole(int x, int y, int faction_id, int bonus, MAP* sq) {
    if (!has_terra(FORMER_THERMAL_BORE, is_ocean(sq), faction_id)) return false;
    if (is_human(faction_id) && !(GamePreferences & PREF_AUTO_FORMER_BUILD_ADV)) return false;
    if ((sq->items & (BIT_BASE_IN_TILE | BIT_MONOLITH | BIT_THERMAL_BORE)) || bonus == RES_NUTRIENT) return false;
    if (bonus == RES_NONE && is_rolling(sq) && (sq->items & BIT_CONDENSER)) return false;
    if (mapdata[{x, y}].former < 4 && !mapnodes.count({x, y, NODE_BOREHOLE})) return false;
    int level = alt_level(sq);
    for (auto& m : iterate_tiles(x, y, 1, 9)) {
        if ((m.sq->items & BIT_THERMAL_BORE) || mapnodes.count({m.x, m.y, NODE_BOREHOLE})) return false;
        int level2 = alt_level(m.sq);
        if (level2 < level && level2 > ALT_OCEAN_SHELF) return false;
    }
    return true;
}

bool can_farm(int x, int y, int faction_id, int bonus, MAP* sq) {
    bool has_nut = has_tech(Rules.tech_preq_allow_3_nutrients_sq, faction_id);
    bool sea = is_ocean(sq);
    if (!has_terra(FORMER_FARM, sea, faction_id) || is_rocky(sq) || (sq->items & BIT_THERMAL_BORE)) return false;
    if (bonus == RES_NUTRIENT && !(sq->items & BIT_FOREST) && (sea || is_rainy_or_moist(sq) || is_rolling(sq))) return true;
    if (bonus == RES_ENERGY || bonus == RES_MINERAL || (sq->landmarks & LM_VOLCANO)) return false;
    if (!has_nut && bonus != RES_NUTRIENT && crop_yield(faction_id, -1, x, y, 0) >= conf.tile_output_limit[0]) return false;
    return (is_rolling(sq) + is_rainy_or_moist(sq) + (nearby_items(x, y, 0, 9, BIT_FARM | BIT_CONDENSER) < 2) +
            ((sq->items & (BIT_FARM | BIT_CONDENSER)) ? 1 : 0) + ((sq->items & BIT_FOREST) ? 0 : 2) +
            ((sq->landmarks & LM_JUNGLE) ? 0 : 1)) > 4;
}

bool can_solar(int x, int y, int faction_id, int bonus, MAP* sq) {
    bool sea = is_ocean(sq);
    if (!has_terra(FORMER_SOLAR, sea, faction_id) || bonus == RES_MINERAL) return false;
    if (is_rocky(sq) && bonus != RES_ENERGY) return false;
    if (!has_tech(Rules.tech_preq_allow_3_energy_sq, faction_id) && bonus != RES_ENERGY &&
        energy_yield(faction_id, -1, x, y, 0) >= 2) {
        return false;
    }
    if (!sea && has_terra(FORMER_FOREST, sea, faction_id) && ResInfo.forest_sq.energy > 0 &&
        !(is_rocky(sq) && bonus == RES_ENERGY && alt_level(sq) > ALT_TWO_ABOVE_SEA) &&
        ((sq->landmarks & LM_JUNGLE) ||
         (is_rainy(sq) + is_rolling(sq) + is_rainy_or_moist(sq) + ((sq->items & BIT_FARM) ? 1 : 0) < 3))) {
        return false;
    }
    if ((sq->items & BIT_SENSOR) && nearby_items(x, y, 0, 9, BIT_SENSOR) < 2) return false;
    return !(sq->items & (BIT_MINE | BIT_FOREST | BIT_SOLAR | BIT_ADVANCED));
}

bool can_mine(int x, int y, int faction_id, int bonus, MAP* sq) {
    bool sea = is_ocean(sq);
    if (!has_terra(FORMER_MINE, sea, faction_id) || bonus == RES_NUTRIENT) return false;
    if (!sea && !is_rocky(sq)) return false;
    if (!has_tech(Rules.tech_preq_allow_3_minerals_sq, faction_id) && bonus != RES_MINERAL &&
        mine_yield(faction_id, -1, x, y, 0) >= 2) {
        return false;
    }
    if ((sq->items & BIT_SENSOR) && nearby_items(x, y, 0, 9, BIT_SENSOR) < 2) return false;
    return !(sq->items & (BIT_MINE | BIT_FOREST | BIT_SOLAR | BIT_ADVANCED));
}

bool can_forest(int x, int y, int faction_id, MAP* sq) {
    if (!has_terra(FORMER_FOREST, is_ocean(sq), faction_id)) return false;
    if (is_rocky(sq) || (sq->landmarks & LM_VOLCANO)) return false;
    if (!has_tech(Rules.tech_preq_allow_3_nutrients_sq, faction_id) && (is_rolling(sq) || (sq->items & BIT_SOLAR)) &&
        crop_yield(faction_id, -1, x, y, 0) >= conf.tile_output_limit[0]) {
        return false;
    }
    if (is_human(faction_id) && !(GamePreferences & PREF_AUTO_FORMER_PLANT_FORESTS)) return false;
    return !(sq->items & BIT_FOREST);
}

bool can_sensor(int x, int y, int faction_id, MAP* sq) {
    if (!has_terra(FORMER_SENSOR, is_ocean(sq), faction_id)) return false;
    if (sq->items & (BIT_MINE | BIT_SOLAR | BIT_SENSOR | BIT_ADVANCED)) return false;
    if (is_fungus(sq) && !has_tech(Rules.tech_preq_improv_fungus, faction_id)) return false;
    for (auto& m : iterate_tiles(x, y, 1, 25)) {
        if (m.sq->owner == faction_id && ((m.sq->items & BIT_SENSOR) || mapnodes.count({m.x, m.y, NODE_SENSOR_ARRAY})))
            return false;
    }
    if (is_human(faction_id) && !(GameMorePreferences & MPREF_AUTO_FORMER_BUILD_SENSORS)) return false;
    return true;
}

bool keep_fungus(int x, int y, int faction_id, MAP* sq) {
    return plans[faction_id].keep_fungus && !(sq->items & (BIT_BASE_IN_TILE | BIT_MONOLITH)) &&
           alt_level(sq) >= ALT_OCEAN_SHELF &&
           nearby_items(x, y, 0, 9, BIT_FUNGUS) < (is_fungus(sq) ? 1 : 0) + plans[faction_id].keep_fungus;
}

bool plant_fungus(int x, int y, int faction_id, MAP* sq) {
    return plans[faction_id].plant_fungus && keep_fungus(x, y, faction_id, sq) && alt_level(sq) >= ALT_OCEAN_SHELF &&
           has_terra(FORMER_PLANT_FUNGUS, is_ocean(sq), faction_id);
}

bool can_level(int x, int y, int faction_id, int bonus, MAP* sq) {
    return is_rocky(sq) && has_terra(FORMER_LEVEL_TERRAIN, is_ocean(sq), faction_id) &&
           (bonus == RES_NUTRIENT ||
            (bonus == RES_NONE && !(sq->items & (BIT_MINE | BIT_FUNGUS | BIT_THERMAL_BORE)) && (sq->items & BIT_RIVER) &&
             !plans[faction_id].plant_fungus &&
             nearby_items(x, y, 0, 9, BIT_FARM | BIT_FOREST) < ((sq->landmarks & LM_JUNGLE) ? 4 : 2)));
}

bool can_river(int x, int y, int faction_id, MAP* sq) {
    if (is_ocean(sq) || !has_terra(FORMER_AQUIFER, TRIAD_LAND, faction_id)) return false;
    if (sq->items & (BIT_BASE_IN_TILE | BIT_RIVER | BIT_THERMAL_BORE)) return false;
    return !(((CurrentTurn / 4 * x) ^ y) & 15) && !coast_tiles(x, y) &&
           nearby_items(x, y, 1, 9, BIT_RIVER | BIT_THERMAL_BORE) < 2 && nearby_items(x, y, 1, 25, BIT_RIVER) < 6;
}

bool can_road(int x, int y, int faction_id, MAP* sq) {
    if (!has_terra(FORMER_ROAD, is_ocean(sq), faction_id) || (sq->items & (BIT_ROAD | BIT_BASE_IN_TILE))) return false;
    if (!is_base_radius(sq) && mapdata[{x, y}].roads < 1) return false;
    if (is_fungus(sq) && (!has_tech(Rules.tech_preq_build_road_fungus, faction_id) ||
                          (!plans[faction_id].build_tubes && thinker::has_project(FAC_XENOEMPATHY_DOME, faction_id)))) {
        return false;
    }
    if (is_human(faction_id) && (GameMorePreferences & MPREF_AUTO_FORMER_CANT_BUILD_ROADS)) return false;
    if (sq->owner != faction_id) return mapdata[{x, y}].roads > 0 && !both_neutral(faction_id, sq->owner);
    if (mapnodes.count({x, y, NODE_GOAL_RAISE_LAND})) return true;
    if (mapdata[{x, y}].roads > 0 || (sq->items & (BIT_MINE | BIT_CONDENSER | BIT_THERMAL_BORE))) return true;
    int i = 0;
    int r[] = {0, 0, 0, 0, 0, 0, 0, 0};
    for (const auto& t : NearbyTiles) {
        MAP* n = mapsq(wrap(x + t[0]), y + t[1]);
        if (n && !is_ocean(n) && n->owner == faction_id) {
            if (n->items & (BIT_ROAD | BIT_BASE_IN_TILE)) r[i] = 1;
        }
        i++;
    }
    // Determine if we should connect roads on opposite sides of the tile
    if ((r[0] && r[4] && !r[2] && !r[6]) || (r[2] && r[6] && !r[0] && !r[4]) ||
        (r[1] && r[5] && !((r[2] && r[4]) || (r[0] && r[6]))) || (r[3] && r[7] && !((r[0] && r[2]) || (r[4] && r[6])))) {
        return true;
    }
    return false;
}

bool can_magtube(int x, int y, int faction_id, MAP* sq) {
    if (!has_terra(FORMER_MAGTUBE, is_ocean(sq), faction_id) || (sq->items & (BIT_MAGTUBE | BIT_BASE_IN_TILE))) return false;
    if (both_neutral(faction_id, sq->owner)) return false;
    if (is_human(faction_id) && (GameMorePreferences & MPREF_AUTO_FORMER_CANT_BUILD_ROADS)) return false;
    return mapdata[{x, y}].roads > 0 && (sq->items & BIT_ROAD) &&
           (!is_fungus(sq) || has_tech(Rules.tech_preq_improv_fungus, faction_id));
}

}  // namespace

// ---- movement predicates -------------------------------------------------------------------------

bool non_ally_in_tile(int x, int y, int faction_id) {
    if (nonally_veh_count != VehCount || nonally_faction_id != faction_id) {
        nonally.clear();
        for (int i = VehCount - 1; i >= 0; --i) {
            VEH* veh = &Vehs[i];
            if (veh->faction_id != faction_id && !has_pact(faction_id, veh->faction_id)) nonally.insert({veh->x, veh->y});
        }
        debug("refresh %d %d %d\n", CurrentTurn, faction_id, VehCount);
        nonally_veh_count = VehCount;
        nonally_faction_id = faction_id;
    }
    return nonally.count({x, y}) > 0;
}

bool allow_move(int x, int y, int faction_id, int tri) {
    MAP* sq;
    if (!(sq = mapsq(x, y)) || non_ally_in_tile(x, y, faction_id)) return false;
    if (tri != TRIAD_AIR && is_ocean(sq) != (tri == TRIAD_SEA)) return false;
    // Exclude moves that might attack enemy bases
    return !is_owned(sq) || sq->owner == faction_id || has_pact(faction_id, sq->owner) ||
           (at_war(faction_id, sq->owner) && !is_base(sq));
}

bool allow_civ_move(int x, int y, int faction_id, int tri) {
    MAP* sq;
    if (!(sq = mapsq(x, y)) || (is_owned(sq) && sq->owner != faction_id)) return false;
    if (tri != TRIAD_AIR && !is_base(sq) && is_ocean(sq) != (tri == TRIAD_SEA)) return false;
    if (!non_ally_in_tile(x, y, faction_id)) {
        return move_upkeep_faction != faction_id || mapdata[{x, y}].safety >= PM_NEAR_SAFE;
    }
    return false;
}

// Any sea region or inland lakes
int coast_tiles(int x, int y) { return mapdata[{x, y}].flags & PM_ShoreMask; }

bool near_sea_coast(int x, int y) {
    for (const auto& m : iterate_tiles(x, y, 0, 21)) {
        if (coast_tiles(m.x, m.y)) return true;
    }
    return false;
}

// ---- orders --------------------------------------------------------------------------------------

int mod_veh_skip(int veh_id) {
    veh_skip(veh_id);
    return VEH_SKIP;
}

int set_move_to(int veh_id, int x, int y) {
    VEH* veh = &Vehs[veh_id];
    debug("set_move_to %2d %2d -> %2d %2d %s\n", veh->x, veh->y, x, y, veh_name(veh));
    veh->waypoint_x[0] = (int16_t)x;
    veh->waypoint_y[0] = (int16_t)y;
    veh->order = ORDER_MOVE_TO;
    veh->status_icon = 'G';
    if (is_former(veh)) veh->movement_turns = 0;
    mapnodes.erase({x, y, NODE_PATROL});
    mapnodes.erase({x, y, NODE_COMBAT_PATROL});
    if (veh->x == x && veh->y == y) return mod_veh_skip(veh_id);
    mapdata[{x, y}].target++;
    return VEH_SYNC;
}

int set_action(int veh_id, int act, char icon) {
    VEH* veh = &Vehs[veh_id];
    if (act == ORDER_THERMAL_BOREHOLE) mapnodes.insert({veh->x, veh->y, NODE_BOREHOLE});
    else if (act == ORDER_TERRAFORM_UP) mapnodes.insert({veh->x, veh->y, NODE_RAISE_LAND});
    else if (act == ORDER_SENSOR_ARRAY) mapnodes.insert({veh->x, veh->y, NODE_SENSOR_ARRAY});
    veh->order = (uint8_t)act;
    veh->status_icon = (uint8_t)icon;
    veh->state &= ~VSTATE_UNK_10000;
    return VEH_SYNC;
}

int set_order_none(int veh_id) {
    VEH* veh = &Vehs[veh_id];
    veh->order = ORDER_NONE;
    veh->order_auto_type = ORDERA_TERRA_AUTO_FULL;
    veh->waypoint_x[0] = -1;
    veh->waypoint_y[0] = -1;
    veh->status_icon = '-';
    return mod_veh_skip(veh_id);
}

// ---- the turn's map data -------------------------------------------------------------------------

void move_upkeep(int faction_id, UpdateMode mode) {
    int tile_count[MaxRegionNum] = {};
    Faction& f = Factions[faction_id];
    AIPlans& p = plans[faction_id];
    if (!faction_id || !is_alive(faction_id)) return;
    if (mode == UM_Player && !is_human(faction_id)) return;
    random_reseed((uint32_t)CurrentTurn * 0x9E3779B9u + (uint32_t)faction_id);
    move_upkeep_faction = faction_id;
    update_main_region(faction_id);
    if (mode == UM_Player) former_plans(faction_id);
    mapdata.clear();
    mapnodes.clear();
    region_enemy.clear();
    region_probe.clear();
    debug("move_upkeep %d region: %d x: %2d y: %2d naval: %d\n", faction_id, p.main_region, p.main_region_x,
          p.main_region_y, p.prioritize_naval);

    MAP* sq;
    for (int y = 0; y < MapAreaY; y++) {
        for (int x = y & 1; x < MapAreaX; x += 2) {
            if (!(sq = mapsq(x, y))) continue;
            if (sq->region < MaxRegionNum) {
                tile_count[sq->region]++;
                if (is_ocean(sq)) {
                    adjust_shore(x, y, 1, 1);
                    if (p.main_sea_region < 0 || tile_count[sq->region] > tile_count[p.main_sea_region]) {
                        p.main_sea_region = sq->region;
                    }
                } else if (sq->owner == faction_id && (sq->items & BIT_BASE_RADIUS) && !is_base(sq)) {
                    mapdata[{x, y}].flags |= PM_LandBaseRds;
                }
            }
            if (sq->owner == faction_id &&
                ((!p.build_tubes && (sq->items & BIT_ROAD)) || (p.build_tubes && (sq->items & BIT_MAGTUBE)))) {
                mapdata[{x, y}].roads++;
            } else if (goody_at(x, y)) {
                mapnodes.insert({x, y, NODE_PATROL});
                if (!is_owned(sq)) {
                    for (auto& m : iterate_tiles(x, y, 0, 9)) mapnodes.insert({m.x, m.y, NODE_SCOUT_SITE});
                }
            }
        }
    }
    for (int y = 0; y < MapAreaY; y++) {
        for (int x = y & 1; x < MapAreaX; x += 2) {
            if ((sq = mapsq(x, y)) && sq->region == p.main_sea_region) adjust_shore(x, y, 1, PM_ShoreLine);
        }
    }
    for (int i = 0, cnt = VehCount; i < cnt; ++i) {
        VEH* veh = &Vehs[i];
        int tri = triad(veh);
        if (!(sq = mapsq(veh->x, veh->y))) continue;
        if (veh->order == ORDER_THERMAL_BOREHOLE) mapnodes.insert({veh->x, veh->y, NODE_BOREHOLE});
        if (veh->order == ORDER_TERRAFORM_UP) mapnodes.insert({veh->x, veh->y, NODE_RAISE_LAND});
        if (veh->order == ORDER_SENSOR_ARRAY) mapnodes.insert({veh->x, veh->y, NODE_SENSOR_ARRAY});
        if (veh->faction_id == faction_id) {
            if (is_combat_unit(veh)) {
                adjust_safety(veh->x, veh->y, 1, 40);
                mapdata[{veh->x, veh->y}].safety += 60;
            }
            if (tri == TRIAD_LAND && (is_colony(veh) || is_former(veh) || is_supply(veh))) {
                if (is_ocean(sq) && is_base(sq) && coast_tiles(veh->x, veh->y) < 8) {
                    mapnodes.insert({veh->x, veh->y, NODE_NEED_FERRY});
                }
            }
            if (veh->order >= ORDER_MOVE_TO && veh->waypoint_x[0] >= 0) {
                mapnodes.erase({veh->waypoint_x[0], veh->waypoint_y[0], NODE_PATROL});
            }
            adjust_unit_near(veh->x, veh->y, 3, (tri != TRIAD_AIR ? 2 : 1));
        } else if (at_war(faction_id, veh->faction_id)) {
            int value = (is_combat_unit(veh) ? -100 : (is_probe(veh) ? -24 : -12));
            int range = (chassis_speed(veh) > 1 ? 3 : 2) + (tri == TRIAD_AIR ? 2 : 0);
            if (value == -100) adjust_safety(veh->x, veh->y, range, value / (range > 2 ? 4 : 8));
            mapdata[{veh->x, veh->y}].enemy++;
            adjust_safety(veh->x, veh->y, 1, value);
            if (veh->unit_id != BSC_FUNGAL_TOWER) adjust_enemy_near(veh->x, veh->y, 3, 1);
        } else if (has_pact(faction_id, veh->faction_id) && is_combat_unit(veh)) {
            adjust_safety(veh->x, veh->y, 1, 20);
            mapdata[{veh->x, veh->y}].safety += 30;
        }
        // Check if we can evict neutral probe teams from home territory
        // Also check if we can capture artifacts from neutral or home territory
        if (veh->faction_id != faction_id) {
            if (is_artifact(veh) && (sq->owner == faction_id || !is_owned(sq)) &&
                stack_search(veh->x, veh->y, faction_id, ST_NonPactOnly, WMODE_ARTIFACT)) {
                mapnodes.insert({veh->x, veh->y, NODE_COMBAT_PATROL});
                debug("capture_artifact %2d %2d\n", veh->x, veh->y);
            }
            if (is_probe(veh) && sq->owner == faction_id &&
                stack_search(veh->x, veh->y, faction_id, ST_NeutralOnly, WMODE_PROBE)) {
                mapnodes.insert({veh->x, veh->y, NODE_COMBAT_PATROL});
                debug("capture_probe %2d %2d\n", veh->x, veh->y);
            }
        }
    }
    TileSearch ts;
    PointList main_bases;
    PointList enemy_bases;
    bool enemy = false;
    for (int i = 0, cnt = BaseCount; i < cnt; ++i) {
        BASE* base = &Bases[i];
        if (!(sq = mapsq(base->x, base->y))) continue;
        if (base->faction_id == faction_id) {
            adjust_former(base->x, base->y, 2, base->pop_size);
            mapdata[{base->x, base->y}].safety += 10000;
            if (has_fac_built(FAC_PSI_GATE, i)) mapdata[{base->x, base->y}].flags |= PM_PsiGateBase;
            if (sq_veh_who(sq) < 0) mapnodes.insert({base->x, base->y, NODE_PATROL});  // Undefended base
            if (sq->region == p.main_region) main_bases.push_back({base->x, base->y});
            if (sq->region < MaxRegionLandNum) {
                int j = 0;
                int k = 0;
                ts.init(base->x, base->y, TS_TERRITORY_LAND);
                while (++j <= 150 && k < 5 && (sq = ts.get_next()) != nullptr) {
                    if (is_base(sq) && sq->owner == faction_id) {
                        int dist;
                        int bases = f.region_total_bases[sq->region];
                        if (bases < 4 || (dist = route_dist(mapdata, base->x, base->y, ts.rx, ts.ry)) < 0 ||
                            dist > map_range(base->x, base->y, ts.rx, ts.ry) + (bases < 16)) {
                            ts.adjust_roads(mapdata, 1);
                        }
                        k++;
                    }
                    if (ts.dist == 1) mapdata[{ts.rx, ts.ry}].flags |= PM_NearOwnBase;
                }
            }
        } else {
            if (at_war(faction_id, base->faction_id)) {
                if (!is_ocean(sq)) enemy_bases.push_back({base->x, base->y});
                region_enemy.insert(sq->region);
                enemy = true;
            } else if (has_pact(faction_id, base->faction_id)) {
                mapdata[{base->x, base->y}].safety += 5000;
            }
        }
    }
    if (f.base_count > 0 && enemy) {
        map_int_t roads;
        int total = 0;
        int max_dist = clamp(f.base_count, 10, 30);
        ts.init(main_bases, TS_TRIAD_LAND, 1);
        while ((sq = ts.get_next()) != nullptr && ts.dist < max_dist && total < 4) {
            if (is_base(sq) && at_war(faction_id, sq->owner)) {
                total++;
                if (compare_might(faction_id, sq->owner) && roads[sq->owner] < 2) {
                    ts.adjust_roads(mapdata, 1);
                    roads[sq->owner]++;
                }
            }
        }
        ts.init(enemy_bases, TS_TRIAD_LAND, 1);
        while ((sq = ts.get_next()) != nullptr) mapdata[{ts.rx, ts.ry}].enemy_dist = ts.dist;
        point_max_queue_t cover;
        for (auto& m : mapdata) {
            int x = m.first.x;
            int y = m.first.y;
            bool near_own_base = m.second.flags & PM_NearOwnBase;
            bool near_enemy_base = m.second.enemy_dist == 1;
            if (((m.second.unit_near > 0 && (m.second.enemy_rank > 0 || near_enemy_base)) ||
                 (near_own_base && m.second.enemy_dist > 0 && m.second.enemy_dist < 8)) &&
                (sq = mapsq(x, y)) && !is_base(sq) && (sq->owner == faction_id || at_war(faction_id, sq->owner))) {
                int veh_owner = sq_veh_who(sq);
                int score = clamp(m.second.unit_near + 1, 1, 800) *
                            clamp(2 * m.second.enemy_rank - min(0, m.second.safety / 32), 1, 800) *
                            (near_own_base ? 4 : 1) * (near_enemy_base ? 4 : 1) * ((sq->items & BIT_ROAD) ? 4 : 1) *
                            (veh_owner == faction_id ? 3 : (veh_owner < 0 ? 2 : 1)) * (!(x & 1) ? 2 : 1);
                cover.push({x, y, score});
            }
            m.second.enemy_rank = 0;
        }
        for (int i = 64; cover.size() > 0 && i > 0; --i) {
            auto cv = cover.top();
            mapdata[{cv.x, cv.y}].enemy_rank = i;
            cover.pop();
        }
    } else {
        for (auto& m : mapdata) m.second.enemy_rank = 0;
    }
    // Thinker's AI factions (UM_Full) go on to rank their bases for defence and to plan land raising
    // and invasions; a player's formers need none of that. The faction's own goals still count.
    for (int i = 0; i < MaxGoalsNum; i++) {
        Goal& goal = f.goals[i];
        if (goal.priority <= 0) continue;
        if (goal.type == AI_GOAL_RAISE_LAND) {
            mapnodes.insert({goal.x, goal.y, NODE_GOAL_RAISE_LAND});
            mapdata[{goal.x, goal.y}].former += 8;
            mapdata[{goal.x, goal.y}].roads += 2;
        }
    }
}

// ---- formers -------------------------------------------------------------------------------------

int select_item(int x, int y, int faction_id, FormerMode mode, MAP* sq) {
    uint32_t items = sq->items;
    int alt = alt_level(sq);
    bool sea = alt < ALT_SHORE_LINE;
    bool road = can_road(x, y, faction_id, sq);
    bool fungus = is_fungus(sq);
    bool rem_fungus = has_terra(FORMER_REMOVE_FUNGUS, sea, faction_id) &&
                      (!is_human(faction_id) || (GameMorePreferences & MPREF_AUTO_FORMER_REMOVE_FUNGUS));

    if (is_base(sq) || volcano_center(sq)) return FORMER_NONE;
    // Improvements on ocean possible for aquatic factions after Adv. Ecological Engineering
    if (alt < ALT_OCEAN_SHELF && (!is_aquatic(faction_id) || !has_tech(TECH_EcoEng2, faction_id))) return FORMER_NONE;
    if (mode == FM_Auto_Sensors) {
        if (can_sensor(x, y, faction_id, sq)) return FORMER_SENSOR;
        return FORMER_NONE;
    }
    if (mode == FM_Remove_Fungus) {
        if (fungus && rem_fungus) return FORMER_REMOVE_FUNGUS;
        return FORMER_NONE;
    }
    if ((mode == FM_Auto_Full || mode == FM_Auto_Tubes) && can_magtube(x, y, faction_id, sq)) return FORMER_MAGTUBE;
    if (mode == FM_Auto_Full && can_bridge(x, y, faction_id, sq)) {
        if (mapnodes.count({x, y, NODE_RAISE_LAND}) ||
            terraform_cost(x, y, faction_id) < Factions[faction_id].energy_credits / 8) {
            return (road ? FORMER_ROAD : FORMER_RAISE_LAND);
        }
    }
    if (road || sq->owner != faction_id || !is_base_radius(sq) || (items & BIT_MONOLITH)) {
        return (road ? FORMER_ROAD : FORMER_NONE);
    }
    if (mode == FM_Farm_Road || mode == FM_Mine_Road) {
        if (fungus) {
            if (has_terra(FORMER_REMOVE_FUNGUS, sea, faction_id)) return FORMER_REMOVE_FUNGUS;
            return FORMER_NONE;
        }
        if (!(items & BIT_ROAD) && has_terra(FORMER_ROAD, sea, faction_id)) return FORMER_ROAD;
        if (items & BIT_MONOLITH) return FORMER_NONE;
    }
    if (mode == FM_Farm_Road) {
        if ((sea || !is_rocky(sq)) && !(items & BIT_FARM) && has_terra(FORMER_FARM, sea, faction_id)) return FORMER_FARM;
        if (!(items & BIT_SOLAR) && has_terra(FORMER_SOLAR, sea, faction_id)) return FORMER_SOLAR;
    }
    if (mode == FM_Mine_Road) {
        if (!(items & BIT_MINE) && has_terra(FORMER_MINE, sea, faction_id)) return FORMER_MINE;
    }
    if (mode != FM_Auto_Full) return FORMER_NONE;  // Skip non-automated player formers
    if (can_river(x, y, faction_id, sq)) return FORMER_AQUIFER;
    int bonus = bonus_at(x, y);
    int current = total_yield(x, y, faction_id);

    bool forest = has_terra(FORMER_FOREST, sea, faction_id) && ResInfo.forest_sq.energy > 0;
    bool borehole = has_terra(FORMER_THERMAL_BORE, sea, faction_id) && ResInfo.borehole_sq.energy > 2;
    bool condenser = has_terra(FORMER_CONDENSER, sea, faction_id);
    bool use_sensor = (items & BIT_SENSOR) && nearby_items(x, y, 0, 9, BIT_SENSOR) < 2;
    bool allow_farm = (items & BIT_FARM) || can_farm(x, y, faction_id, bonus, sq);
    bool allow_forest = (items & BIT_FOREST) || can_forest(x, y, faction_id, sq);
    bool allow_fungus = fungus || plant_fungus(x, y, faction_id, sq);
    bool allow_borehole = (items & BIT_THERMAL_BORE) || can_borehole(x, y, faction_id, bonus, sq);

    int farm_val = (sea && allow_farm ? 2 * item_yield(x, y, faction_id, bonus, BIT_FARM) + ((items & BIT_FARM) ? 1 : 0) : 0);
    int forest_val =
        (allow_forest ? 2 * item_yield(x, y, faction_id, bonus, BIT_FOREST) + ((items & BIT_FOREST) ? 1 : 0) : 0);
    int fungus_val = (allow_fungus ? 2 * item_yield(x, y, faction_id, bonus, BIT_FUNGUS) - min(4, 2 * bonus_yield(bonus)) +
                                         (fungus ? 1 : 0)
                                   : 0);
    int borehole_val = (allow_borehole ? 2 * item_yield(x, y, faction_id, bonus, BIT_THERMAL_BORE) +
                                             ((items & BIT_THERMAL_BORE) ? 1 : 0)
                                       : 0);
    int max_val = 0;
    int skip_val = (current > 7);
    int crop_val = (bonus == RES_NUTRIENT) + skip_val +
                   (!(items & BIT_CONDENSER) && allow_farm && is_rainy_or_moist(sq) && is_rolling(sq));

    for (int v : {farm_val, forest_val, fungus_val, borehole_val}) max_val = max(v, max_val);
    if (farm_val == max_val && sea && max_val > 0) {
        if (fungus) return (rem_fungus ? FORMER_REMOVE_FUNGUS : FORMER_NONE);
        if (farm_val / 2 > current && !(items & BIT_FARM) && allow_farm) return FORMER_FARM;
    }
    if (forest_val == max_val && max_val > 0) {
        if (fungus) return (rem_fungus ? FORMER_REMOVE_FUNGUS : FORMER_NONE);
        if (items & BIT_FOREST) return (can_sensor(x, y, faction_id, sq) ? FORMER_SENSOR : FORMER_NONE);
        if (forest_val / 2 > current + crop_val && allow_forest) return FORMER_FOREST;
    }
    if (fungus_val == max_val && max_val > 0) {
        if (fungus) return (can_sensor(x, y, faction_id, sq) ? FORMER_SENSOR : FORMER_NONE);
        if (fungus_val / 2 > current + (bonus != RES_NONE) && allow_fungus) return FORMER_PLANT_FUNGUS;
    }
    if (borehole_val == max_val && max_val > 0) {
        if (fungus) return (rem_fungus ? FORMER_REMOVE_FUNGUS : FORMER_NONE);
        if (items & BIT_THERMAL_BORE) return FORMER_NONE;
        if (borehole_val / 2 > current && allow_borehole) return FORMER_THERMAL_BORE;
    }
    if (fungus) {
        if (keep_fungus(x, y, faction_id, sq)) return (can_sensor(x, y, faction_id, sq) ? FORMER_SENSOR : FORMER_NONE);
        return (rem_fungus ? FORMER_REMOVE_FUNGUS : FORMER_NONE);
    }
    if (can_level(x, y, faction_id, bonus, sq)) return FORMER_LEVEL_TERRAIN;
    if (sea && bonus == RES_NONE && can_sensor(x, y, faction_id, sq)) return FORMER_SENSOR;

    int solar_need = (condenser ? 0 : 1) + (forest ? 0 : 2) + (borehole ? 0 : 3) +
                     2 * max(0, alt_level(sq) - ALT_ONE_ABOVE_SEA) - nearby_items(x, y, 0, 25, BIT_SOLAR);
    if (sea) {
        solar_need = (has_terra(FORMER_MINE, sea, faction_id)
                          ? (ResInfo.improved_sea.energy - ResInfo.improved_sea.mineral) -
                                has_tech(Rules.tech_preq_mining_platform_bonus, faction_id)
                          : 6) +
                     nearby_items(x, y, 0, 25, BIT_MINE) - nearby_items(x, y, 0, 25, BIT_SOLAR);
    }
    if (can_solar(x, y, faction_id, bonus, sq) && solar_need > 0) {
        if (allow_farm && !(items & BIT_FARM)) return FORMER_FARM;
        return FORMER_SOLAR;
    }
    if (can_mine(x, y, faction_id, bonus, sq)) {
        if (sea && allow_farm && !(items & BIT_FARM)) return FORMER_FARM;
        return FORMER_MINE;
    }
    if ((items & BIT_SOLAR) && solar_need >= 0) return FORMER_NONE;
    if (allow_farm && !(items & BIT_FARM)) return FORMER_FARM;
    if (!use_sensor && (items & BIT_FARM) && !(items & BIT_CONDENSER) && has_terra(FORMER_CONDENSER, sea, faction_id) &&
        (!is_human(faction_id) || (GamePreferences & PREF_AUTO_FORMER_BUILD_ADV))) {
        return FORMER_CONDENSER;
    }
    if (!use_sensor && (items & BIT_FARM) && !(items & BIT_SOIL_ENRICHER) && has_terra(FORMER_SOIL_ENR, sea, faction_id)) {
        return FORMER_SOIL_ENR;
    }
    if (can_sensor(x, y, faction_id, sq)) return FORMER_SENSOR;
    if (forest_val > current + skip_val && can_forest(x, y, faction_id, sq)) return FORMER_FOREST;
    return FORMER_NONE;
}

int former_tile_score(int x, int y, int faction_id, MAP* sq) {
    const int priority[][2] = {
        {BIT_RIVER, 4}, {BIT_FARM, -2}, {BIT_SOLAR, -2}, {BIT_FOREST, -4}, {BIT_MINE, -4}, {BIT_CONDENSER, -4},
        {BIT_SOIL_ENRICHER, -4}, {BIT_THERMAL_BORE, -8},
    };
    int alt = alt_level(sq);
    int bonus = bonus_at(x, y);
    int score = ((lm_items(sq) & ~(LM_DUNES | LM_SARGASSO | LM_UNITY | LM_NEXUS)) ? 4 : 0);

    if (bonus != RES_NONE && !(sq->items & BIT_ADVANCED)) {
        score += ((sq->items & BIT_SIMPLE) ? 3 : 5) * (bonus == RES_NUTRIENT ? 3 : 2);
    }
    for (const int* pr : priority) {
        if (sq->items & (uint32_t)pr[0]) score += pr[1];
    }
    if (is_fungus(sq)) {
        score += ((sq->items & BIT_ADVANCED) ? 20 : 0);
        score += (plans[faction_id].keep_fungus ? -8 : (is_rocky(sq) ? 2 : -2));
        score += (plans[faction_id].plant_fungus && (sq->items & BIT_ROAD) ? -8 : 0);
    } else if (plans[faction_id].plant_fungus) {
        score += 8;
    }
    if ((sq->items & (BIT_FOREST | BIT_SENSOR)) && can_road(x, y, faction_id, sq)) score += 8;
    if (mapdata[{x, y}].roads > 0 &&
        (!(sq->items & BIT_ROAD) || (plans[faction_id].build_tubes && !(sq->items & BIT_MAGTUBE)))) {
        score += 15;
    }
    if (alt == ALT_SHORE_LINE && mapnodes.count({x, y, NODE_GOAL_RAISE_LAND})) score += 20;
    return score + min(8, mapdata[{x, y}].former) + min(0, mapdata[{x, y}].safety);
}

int former_move(const int id) {
    VEH* veh = &Vehs[id];
    MAP* sq = mapsq(veh->x, veh->y);
    int item = -1;
    int choice = -1;
    int faction_id = veh->faction_id;
    if (!sq) return VEH_SYNC;
    bool at_base = is_base(sq) && sq->owner == faction_id;
    bool safe = mapdata[{veh->x, veh->y}].safety >= PM_SAFE;
    FormerMode mode = FM_Auto_Full;
    if (sq->owner != faction_id && mapdata[{veh->x, veh->y}].roads < 1) return move_to_base(id, false);
    if (defend_tile(veh, sq)) return set_order_none(id);
    if (is_ocean(sq) && triad(veh) == TRIAD_LAND) {
        if (!has_transport(veh->x, veh->y, faction_id)) {
            mapnodes.insert({veh->x, veh->y, NODE_NEED_FERRY});
            return mod_veh_skip(id);
        }
        for (auto& m : iterate_tiles(veh->x, veh->y, 1, 9)) {
            if (allow_civ_move(m.x, m.y, faction_id, TRIAD_LAND) && !random(2)) {
                debug("former_trans %2d %2d -> %2d %2d\n", veh->x, veh->y, m.x, m.y);
                return set_move_to(id, m.x, m.y);
            }
        }
        return mod_veh_skip(id);
    }
    if (plr_owner(veh)) {
        if (veh->order_auto_type == ORDERA_TERRA_AUTO_MAGTUBE && has_terra(FORMER_MAGTUBE, is_ocean(sq), faction_id)) {
            mode = FM_Auto_Tubes;
        } else if (veh->order_auto_type == ORDERA_TERRA_AUTO_ROAD && has_terra(FORMER_ROAD, is_ocean(sq), faction_id)) {
            mode = FM_Auto_Roads;
        } else if (veh->order_auto_type == ORDERA_TERRA_AUTO_SENSOR && has_terra(FORMER_SENSOR, is_ocean(sq), faction_id)) {
            mode = FM_Auto_Sensors;
        } else if (veh->order_auto_type == ORDERA_TERRA_AUTO_FUNGUS_REM &&
                   has_terra(FORMER_REMOVE_FUNGUS, is_ocean(sq), faction_id)) {
            mode = FM_Remove_Fungus;
        } else if (veh->order_auto_type == ORDERA_TERRA_FARM_SOLAR_ROAD) {
            mode = FM_Farm_Road;
        } else if (veh->order_auto_type == ORDERA_TERRA_FARM_MINE_ROAD) {
            mode = FM_Mine_Road;
        }
    }
    int turns = (veh->order >= ORDER_FARM && veh->order < ORDER_MOVE_TO ? Terraform[veh->order - VehOrderFormerFirst].rate : 0);

    if (safe || turns >= 12 || plr_owner(veh)) {
        if (turns > 0 && !(veh->order == ORDER_DRILL_AQUIFER && nearby_items(veh->x, veh->y, 0, 9, BIT_RIVER) >= 4)) {
            return VEH_SYNC;
        }
        if (!at_target(veh) && !can_road(veh->x, veh->y, faction_id, sq) && !can_magtube(veh->x, veh->y, faction_id, sq)) {
            return VEH_SYNC;
        }
        item = select_item(veh->x, veh->y, faction_id, mode, sq);
        if (item >= 0) {
            int cost = 0;
            mapdata[{veh->x, veh->y}].former -= 2;
            if (item == FORMER_RAISE_LAND && !mapnodes.count({veh->x, veh->y, NODE_RAISE_LAND})) {
                cost = terraform_cost(veh->x, veh->y, faction_id);
                Factions[faction_id].energy_credits -= cost;
            }
            debug("former_action %2d %2d cost: %d %s\n", veh->x, veh->y, cost, Terraform[item].name);
            return set_action(id, item + VehOrderFormerFirst, *Terraform[item].shortcuts);
        }
    } else if (!safe) {
        return escape_move(id);
    }
    if (mode == FM_Farm_Road || mode == FM_Mine_Road) {
        veh->state &= ~VSTATE_ON_ALERT;  // Request new automation orders
        veh->order = ORDER_NONE;
        return VEH_SYNC;
    }
    bool home_base_only = false;
    bool full_search = (CurrentTurn + id) % 4 == 0;
    int limit = full_search ? 320 : 80;
    int best_score = INT_MIN;
    int score;
    int i = 0;
    int tx = -1;
    int ty = -1;
    int bx = veh->x;
    int by = veh->y;
    if (veh->home_base_id >= 0 && Bases[veh->home_base_id].faction_id == faction_id) {
        bx = Bases[veh->home_base_id].x;
        by = Bases[veh->home_base_id].y;
        if (plr_owner(veh) && veh->order_auto_type == ORDERA_TERRA_AUTOIMPROVE_BASE &&
            region_at(veh->x, veh->y) == region_at(bx, by)) {
            home_base_only = true;
        }
    }
    TileSearch ts;
    ts.init(veh->x, veh->y, triad(veh));

    while (++i <= limit && (sq = ts.get_next()) != nullptr) {
        auto& mp = mapdata[{ts.rx, ts.ry}];
        if (is_base(sq) || (sq->owner != faction_id && mp.roads < 1) ||
            (home_base_only && map_range(bx, by, ts.rx, ts.ry) > 2) || (mp.former < 1 && mp.roads < 1) ||
            mp.safety < PM_SAFE || non_ally_in_tile(ts.rx, ts.ry, faction_id)) {
            continue;
        }
        if (mode == FM_Auto_Full) {
            score = former_tile_score(ts.rx, ts.ry, faction_id, sq) - map_range(bx, by, ts.rx, ts.ry) / 2;
        } else {
            score = former_tile_score(ts.rx, ts.ry, faction_id, sq) - 2 * map_range(veh->x, veh->y, ts.rx, ts.ry);
        }
        if (score > best_score && (choice = select_item(ts.rx, ts.ry, faction_id, mode, sq)) >= 0) {
            tx = ts.rx;
            ty = ts.ry;
            best_score = score;
            item = choice;
        }
    }
    if (tx >= 0) {
        mapdata[{tx, ty}].former -= 2;
        debug("former_move %2d %2d -> %2d %2d score: %d %s\n", veh->x, veh->y, tx, ty, best_score,
              (item >= 0 ? Terraform[item].name : ""));
        return set_move_to(id, tx, ty);
    }

    debug("former_skip %2d %2d %s\n", veh->x, veh->y, veh_name(veh));
    if (region_at(veh->x, veh->y) == region_at(bx, by) && !(veh->x == bx && veh->y == by) &&
        (home_base_only || map_range(veh->x, veh->y, bx, by) < random(32))) {
        return set_move_to(id, bx, by);
    }
    if (!at_base && search_base(ts, id, false, &tx, &ty)) return set_move_to(id, tx, ty);
    // Thinker's AI formers (never a player's) go on to look for a route to build, or disband
    // when a base cannot support them; not ported.
    return mod_veh_skip(id);
}

}  // namespace thinker
