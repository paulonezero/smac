// Thinker's base helpers (thinker/basex.h). From Thinker's base.cpp, faction.cpp, game.cpp,
// map.cpp and path.cpp, with the engine's names as compat.h spells them; the logic is Thinker's.
#include "thinker/basex.h"

#include "engine/diplomacy.h"
#include "engine/tech.h"
#include "thinker/move.h"
#include "thinker/path.h"
#include "thinker/plan.h"

namespace thinker {

namespace {

const int ManifoldHarmonicsBonus[][3] = {
    {0, 0, 0},  // Planet < 0
    {0, 0, 1},  // Planet = 0
    {1, 0, 1},  // Planet = 1
    {1, 1, 1},  // Planet = 2
    {1, 1, 2},  // Planet > 2
};

bool is_shore_level(const MAP* sq) { return alt_level(sq) == ALT_SHORE_LINE; }

// Thinker: a base may build a Headquarters (relocating it) only when the faction has none and
// this base is the best placed of its bases for it.
bool valid_relocate_base(int base_id) {
    int faction_id = Bases[base_id].faction_id;
    int best_id = -1;
    int best_score = 0;
    if (Bases[base_id].mineral_surplus < 2) return false;
    if (!has_tech(Facility[FAC_HEADQUARTERS].preq_tech, faction_id)) return false;
    for (int i = 0; i < BaseCount; i++) {
        BASE* b = &Bases[i];
        if (b->faction_id == faction_id) {
            if (has_fac_built(FAC_HEADQUARTERS, i)) return false;
            if (base_item(b) == -FAC_HEADQUARTERS) return base_id == i;
            int score = 8 * (i == base_id) + 2 * b->pop_size + b->mineral_surplus - b->assimilation_turns_left;
            if (best_id < 0 || score > 2 * best_score) {
                best_id = i;
                best_score = score;
            }
        }
    }
    return best_id == base_id;
}

}  // namespace

// ---- facilities ------------------------------------------------------------------------------------

bool redundant(int item_id, int faction_id) {
    int project_id;
    switch (item_id) {
    case FAC_NAVAL_YARD: project_id = FAC_MARITIME_CONTROL_CENTER; break;
    case FAC_PERIMETER_DEFENSE: project_id = FAC_CITIZENS_DEFENSE_FORCE; break;
    case FAC_COMMAND_CENTER: project_id = FAC_COMMAND_NEXUS; break;
    case FAC_BIOENHANCEMENT_CENTER: project_id = FAC_CYBORG_FACTORY; break;
    case FAC_QUANTUM_CONVERTER: project_id = FAC_SINGULARITY_INDUCTOR; break;
    case FAC_AEROSPACE_COMPLEX: project_id = FAC_CLOUDBASE_ACADEMY; break;
    case FAC_ENERGY_BANK: project_id = FAC_PLANETARY_ENERGY_GRID; break;
    default: return false;
    }
    return thinker::has_project(project_id, faction_id);
}

bool has_facility(int item_id, int base_id) {
    if (item_id <= 0) return false;  // zero slot unused
    if (item_id >= SP_ID_First) return project_base(item_id) == base_id;
    return has_fac_built(item_id, base_id) || redundant(item_id, Bases[base_id].faction_id);
}

int facility_count(int item_id, int faction_id) {
    int n = 0;
    for (int i = 0; i < BaseCount; i++) {
        BASE* base = &Bases[i];
        if (base->faction_id == faction_id && has_fac_built(item_id, i)) n++;
    }
    return n;
}

int prod_count(int item_id, int faction_id, int base_skip_id) {
    int n = 0;
    for (int i = 0; i < BaseCount; i++) {
        BASE* base = &Bases[i];
        if (base->faction_id == faction_id && base_item(base) == item_id && i != base_skip_id) n++;
    }
    return n;
}

bool can_build(int base_id, int item_id) {
    BASE* base = &Bases[base_id];
    int faction_id = base->faction_id;
    Faction* f = &Factions[faction_id];
    // First check strict facility availability by the original games rules
    // Then check various other usefulness conditions to avoid unnecessary builds
    if (!facility_avail(item_id, faction_id, base_id, 0)) return false;
    if (item_id == FAC_STOCKPILE_ENERGY) return base->mineral_consumption > base->mineral_surplus && random(4);
    if (item_id == FAC_ASCENT_TO_TRANSCENDENCE || item_id == FAC_VOICE_OF_PLANET) {
        if (victory_done()) return false;
        if (is_alien(faction_id) && has_tech(Facility[FAC_SUBSPACE_GENERATOR].preq_tech, faction_id)) return false;
    }
    if (item_id >= SP_ID_First && item_id <= SP_ID_Last) {
        int tech_id = Facility[item_id].preq_tech;
        return tech_id == TECH_None || DiffLevel >= conf.limit_project_start ||
               (tech_id >= 0 && (TechOwners[tech_id] & FactionStatus[0]));
    }
    if (item_id == FAC_HEADQUARTERS && !valid_relocate_base(base_id)) return false;
    if ((item_id == FAC_RECREATION_COMMONS || item_id == FAC_HOLOGRAM_THEATRE || item_id == FAC_PUNISHMENT_SPHERE ||
         item_id == FAC_PARADISE_GARDEN) &&
        !base_can_riot(base_id, false)) {
        return false;
    }
    if (item_id == FAC_PUNISHMENT_SPHERE &&
        (project_base(FAC_SUPERCOLLIDER) == base_id || project_base(FAC_THEORY_OF_EVERYTHING) == base_id)) {
        return false;
    }
    if (GameRules & RULES_SCN_NO_TECH_ADVANCES) {
        if (item_id == FAC_RESEARCH_HOSPITAL || item_id == FAC_NANOHOSPITAL || item_id == FAC_FUSION_LAB ||
            item_id == FAC_QUANTUM_LAB) {
            return false;
        }
    }
    if (item_id == FAC_GENEJACK_FACTORY && base_can_riot(base_id, false) &&
        base->talent_total + 1 < base->drone_total + Rules.drones_induced_genejack_factory) {
        return false;
    }
    if ((item_id == FAC_HAB_COMPLEX || item_id == FAC_HABITATION_DOME) &&
        (base->nutrient_surplus < 0 || base_unused_space(base_id) > 1)) {
        return false;
    }
    if (item_id == FAC_PRESSURE_DOME && (ClimateFutureChange <= 0 || !is_shore_level(mapsq(base->x, base->y)))) return false;
    if (item_id >= FAC_EMPTY_FACILITY_42 && item_id <= FAC_EMPTY_FACILITY_64 && Facility[item_id].maint >= 0) {
        return false;  // Negative maintenance or income is possible
    }
    if ((item_id == FAC_COMMAND_CENTER || item_id == FAC_NAVAL_YARD) && (!conf.repair_base_facility || f->SE_morale < -1)) {
        return false;
    }
    if (item_id == FAC_BIOENHANCEMENT_CENTER && f->SE_morale < -1) return false;
    if (item_id == FAC_COVERT_OPS_CENTER && thinker::has_project(FAC_HUNTER_SEEKER_ALGORITHM, faction_id)) return false;
    if (item_id == FAC_PSI_GATE && 4 * facility_count(FAC_PSI_GATE, faction_id) >= f->base_count) return false;
    if (item_id == FAC_SUBSPACE_GENERATOR) {
        if (!is_alien(faction_id) || base->pop_size < Rules.base_size_subspace_gen) return false;
        int n = 0;
        for (int i = 0; i < BaseCount; i++) {
            BASE* b = &Bases[i];
            if (b->faction_id == faction_id && has_facility(FAC_SUBSPACE_GENERATOR, i) &&
                b->pop_size >= Rules.base_size_subspace_gen && ++n >= Rules.subspace_gen_req) {
                return false;
            }
        }
    }
    if (item_id >= FAC_SKY_HYDRO_LAB && item_id <= FAC_ORBITAL_DEFENSE_POD) {
        int prod_num = prod_count(-item_id, faction_id, base_id);
        int goal_num = satellite_goal(faction_id, item_id);
        int built_num = satellite_count(faction_id, item_id);
        if (built_num + prod_num >= goal_num) return false;
    }
    if (item_id == FAC_FLECHETTE_DEFENSE_SYS && f->satellites_ODP > 0) return false;
    if (item_id == FAC_GEOSYNC_SURVEY_POD || item_id == FAC_FLECHETTE_DEFENSE_SYS) {
        for (int i = 0; i < BaseCount; i++) {
            BASE* b = &Bases[i];
            if (b->faction_id == faction_id && i != base_id && map_range(base->x, base->y, b->x, b->y) <= 3 &&
                (has_facility(FAC_GEOSYNC_SURVEY_POD, i) || has_facility(FAC_FLECHETTE_DEFENSE_SYS, i) ||
                 base_item(b) == -FAC_GEOSYNC_SURVEY_POD || base_item(b) == -FAC_FLECHETTE_DEFENSE_SYS)) {
                return false;
            }
        }
    }
    return true;
}

bool can_build_unit(int base_id, int unit_id) {
    BASE* b = &Bases[base_id];
    if (unit_id >= 0 && !(unit_id < MaxProtoFactionNum || (unit_id / MaxProtoFactionNum) == b->faction_id)) return false;
    if (unit_id >= 0 && Units[unit_id].plan == PLAN_COLONY && b->nutrient_surplus <= 0 && b->pop_size == 1 &&
        Factions[b->faction_id].diff_level > 1 && (DiffLevel <= 3 || is_human(b->faction_id))) {
        return false;
    }
    if (unit_id >= 0 && unit_triad(unit_id) == TRIAD_SEA && !adjacent_region(b->x, b->y, -1, 10, TRIAD_SEA)) return false;
    return (VehCount + 32 < conf.max_veh_num) || (VehCount + random(32) < conf.max_veh_num);
}

// ---- the base ------------------------------------------------------------------------------------

bool base_can_riot(int base_id, bool allow_staple) {
    BASE* b = &Bases[base_id];
    return (!allow_staple || !b->nerve_staple_turns_left) && !thinker::has_project(FAC_TELEPATHIC_MATRIX, b->faction_id) &&
           !has_fac_built(FAC_PUNISHMENT_SPHERE, base_id);
}

int base_unused_space(int base_id) {
    BASE* base = &Bases[base_id];
    int limit_mod = (thinker::has_project(FAC_ASCETIC_VIRTUES, base->faction_id) ? 2 : 0) -
                    MFactions[base->faction_id].rule_population;
    if (!has_fac_built(FAC_HAB_COMPLEX, base_id)) return max(0, Rules.pop_limit_wo_hab_complex + limit_mod - base->pop_size);
    if (!has_fac_built(FAC_HABITATION_DOME, base_id)) return max(0, Rules.pop_limit_wo_hab_dome + limit_mod - base->pop_size);
    return max(0, MaxBasePopSize - base->pop_size);
}

int mineral_output_modifier(int base_id) {
    int value = 0;
    if (has_facility(FAC_QUANTUM_CONVERTER, base_id)) value++;  // The Singularity Inductor also possible
    if (has_fac_built(FAC_ROBOTIC_ASSEMBLY_PLANT, base_id)) value++;
    if (has_fac_built(FAC_GENEJACK_FACTORY, base_id)) value++;
    if (has_fac_built(FAC_NANOREPLICATOR, base_id)) value++;
    return value;
}

int mineral_cost(int base_id, int item_id) {
    bool valid = base_id >= 0 && base_id < BaseCount && item_id >= -SP_ID_Last && item_id < MaxProtoNum;
    if (valid) {
        // Take possible prototype costs into account in veh_cost
        int factor = cost_factor(Bases[base_id].faction_id, 1, -1);
        if (item_id >= 0) return veh_cost(item_id, base_id, nullptr) * factor;
        return Facility[-item_id].cost * factor;
    }
    return 0;
}

int hurry_cost(int base_id, int item_id, int hurry_mins) {
    BASE* b = &Bases[base_id];
    MFaction* m = &MFactions[b->faction_id];
    int mins = max(0, mineral_cost(base_id, item_id) - b->minerals_accumulated);
    int cost = (item_id < 0 ? 2 * mins : mins * mins / 20 + 2 * mins);
    if (!conf.simple_hurry_cost && b->minerals_accumulated < Rules.retool_exemption) cost *= 2;
    if (item_id <= -SP_ID_First) cost *= (b->minerals_accumulated < 4 * cost_factor(b->faction_id, 1, -1) ? 4 : 2);
    if (project_base(FAC_VOICE_OF_PLANET) >= 0) cost *= 2;
    cost = cost * m->rule_hurry / 100;
    if (hurry_mins > 0 && mins > 0) return hurry_mins * cost / mins + (((hurry_mins * cost) % mins) != 0);
    return 0;
}

int garrison_count(int x, int y) {
    int num = 0;
    for (int i = VehCount - 1; i >= 0; --i) {
        VEH* veh = &Vehs[i];
        if (veh->x == x && veh->y == y && is_garrison_unit(veh) && veh->order != ORDER_SENTRY_BOARD && at_target(veh)) num++;
    }
    return num;
}

int defender_count(int x, int y, int veh_skip_id) {
    int num = 0;
    for (int i = VehCount - 1; i >= 0; --i) {
        VEH* veh = &Vehs[i];
        if (veh->x == x && veh->y == y && veh->order != ORDER_SENTRY_BOARD && at_target(veh) && i != veh_skip_id) {
            num += eval_garrison(veh);
        }
    }
    return (num + 1) / 4;
}

// ---- the faction ---------------------------------------------------------------------------------

bool has_wmode(int faction_id, int mode) {
    for (int i = 0; i < MaxWeaponNum; i++) {
        if (Weapon[i].mode == mode && has_tech(Weapon[i].preq_tech, faction_id)) return true;
    }
    for (int i = 0; i < MaxProtoFactionNum; i++) {
        if (veh_avail(i, faction_id, -1) && unit_weapon_mode(i) == mode) return true;
    }
    return false;
}

bool has_ships(int faction_id) {
    for (int i = 0; i < MaxChassisNum; i++) {
        if (Chassis[i].triad == TRIAD_SEA && has_tech(Chassis[i].preq_tech, faction_id)) return true;
    }
    for (int i = 0; i < MaxProtoFactionNum; i++) {
        if (veh_avail(i, faction_id, -1) && unit_triad(i) == TRIAD_SEA) return true;
    }
    return false;
}

bool allow_expand(int faction_id) {
    if ((GameRules & RULES_SCN_NO_COLONY_PODS) || BaseCount >= MaxBaseNum) return false;
    if (is_human(faction_id)) return true;
    int pods = 0;
    for (int i = 0; i < VehCount; i++) {
        VEH* veh = &Vehs[i];
        if (veh->faction_id == faction_id && is_colony(veh)) pods++;
    }
    return Factions[faction_id].base_count + pods < conf.expansion_limit;
}

bool un_charter() { return ProposalPassCount[PROP_REPEAL_UN_CHARTER] <= ProposalPassCount[PROP_REINSTATE_UN_CHARTER]; }

bool victory_done() { return (GameState & STATE_GAME_DONE) != 0; }

// ---- the map -------------------------------------------------------------------------------------

bool adjacent_region(int x, int y, int owner, int threshold, bool ocean) {
    for (auto& m : iterate_tiles(x, y, 1, 9)) {
        if (is_ocean(m.sq) != ocean) continue;
        if ((owner < 0 || m.sq->owner < 0 || m.sq->owner == owner) && !bad_reg(m.sq->region) &&
            Continents[m.sq->region].tile_count >= threshold) {
            return true;
        }
    }
    return false;
}

bool can_build_base(int x, int y, int faction_id, int tri) {
    MAP* sq;
    if (!(sq = mapsq(x, y)) || BaseCount >= MaxBaseNum) return false;
    const bool allow_base = !(sq->items & (BIT_BASE_IN_TILE | BIT_MONOLITH)) && !is_fungus(sq) && !is_rocky(sq);
    if (!allow_base || (sq->items & BIT_THERMAL_BORE)) return false;
    int limit = clamp(MapAreaTiles / 512, 1, 3);
    if (y < limit || y >= MapAreaY - limit) return false;
    if (map_is_flat() && (x < 2 || x >= MapAreaX - 2)) return false;
    // Allow base building on smaller maps on owned territory when a new faction is spawning
    if (sq->owner >= 0 && sq->owner != faction_id && Factions[faction_id].base_count > 0 &&
        !has_treaty(faction_id, sq->owner, DIPLO_VENDETTA) &&
        has_treaty(faction_id, sq->owner, DIPLO_TRUCE | DIPLO_TREATY | DIPLO_PACT)) {
        return false;
    }
    if (non_ally_in_tile(x, y, faction_id)) return false;
    int range = conf.base_spacing - (conf.base_nearby_limit < 0 ? 1 : 0);
    int num = 0;
    int i = 0;
    for (auto& m : iterate_tiles(x, y, 0, TableRange[range])) {
        if (is_base(m.sq)) {
            if (conf.base_nearby_limit < 0 || i < TableRange[range - 1] || ++num > conf.base_nearby_limit) return false;
        }
        i++;
    }
    int alt = alt_level(sq);
    if (tri != TRIAD_SEA && alt >= ALT_SHORE_LINE) return true;
    if ((tri == TRIAD_SEA || tri == TRIAD_AIR) && (alt == ALT_OCEAN || alt == ALT_OCEAN_SHELF)) return true;
    return false;
}

bool has_base_sites(TileSearch& ts, int x, int y, int faction_id, int tri) {
    MAP* sq;
    int value = 0;
    int bases = 0;
    int i = 0;
    int limit = ((CurrentTurn ^ x) & 7 ? 50 : 200) * clamp(conf.base_spacing, 4, 8);
    ts.init(x, y, tri, 1);
    while (++i <= limit && (sq = ts.get_next()) != nullptr) {
        if (is_base(sq)) {
            bases++;
        } else if (can_build_base(ts.rx, ts.ry, faction_id, tri)) {
            value += (alt_level(sq) >= ALT_OCEAN_SHELF ? 4 : 1);
            if (value > 40) break;
        }
    }
    debug("has_base_sites %2d %2d triad: %d value: %d bases: %d\n", x, y, tri, value, bases);
    return value > min(40, 4 * (bases + 4));
}

// ---- yields (Thinker's map.cpp) ------------------------------------------------------------------

int bonus_yield(int res_type) {
    switch (res_type) {
    case RES_NUTRIENT: return ResInfo.bonus_sq.nutrient;
    case RES_MINERAL: return ResInfo.bonus_sq.mineral;
    case RES_ENERGY: return ResInfo.bonus_sq.energy;
    default: return 0;
    }
}

int total_yield(int x, int y, int faction_id) {
    return crop_yield(faction_id, -1, x, y, 0) + mine_yield(faction_id, -1, x, y, 0) +
           energy_yield(faction_id, -1, x, y, 0);
}

int fungus_yield(int faction_id, int res_type) {
    Faction* f = &Factions[faction_id];
    int p = clamp(f->SE_planet_pending, -3, 0);
    int N = clamp(f->tech_fungus_nutrient + p, 0, 99);
    int M = clamp(f->tech_fungus_mineral + p, 0, 99);
    int E = clamp(f->tech_fungus_energy + p + (f->SE_economy_pending >= 2), 0, 99);
    if (thinker::has_project(FAC_MANIFOLD_HARMONICS, faction_id)) {
        int m = clamp(f->SE_planet_pending + 1, 0, 4);
        N += ManifoldHarmonicsBonus[m][0];
        M += ManifoldHarmonicsBonus[m][1];
        E += ManifoldHarmonicsBonus[m][2];
    }
    if (res_type == RES_NUTRIENT) return N;
    if (res_type == RES_MINERAL) return M;
    if (res_type == RES_ENERGY) return E;
    return N + M + E;
}

int item_yield(int x, int y, int faction_id, int bonus, uint32_t item) {
    MAP* sq = mapsq(x, y);
    if (!sq) return 0;
    int N = 0, M = 0, E = 0;
    if (item != BIT_FUNGUS) {
        if (bonus == RES_NUTRIENT) N += ResInfo.bonus_sq.nutrient;
        if (bonus == RES_MINERAL) M += ResInfo.bonus_sq.mineral;
        if (bonus == RES_ENERGY) E += ResInfo.bonus_sq.energy;
        if ((sq->items & BIT_RIVER) && !is_ocean(sq)) E++;
        if ((sq->landmarks & LM_CRATER) && code_at(sq) < 9) M++;
        if ((sq->landmarks & LM_VOLCANO) && code_at(sq) < 9) M++, E++;
        if (sq->landmarks & LM_JUNGLE) N++;
        if (sq->landmarks & LM_URANIUM) E++;
        if ((sq->landmarks & LM_FRESH) && is_ocean(sq)) N++;
        if (sq->landmarks & LM_GEOTHERMAL) E++;
        if (sq->landmarks & LM_RIDGE) E++;
        if (sq->landmarks & LM_CANYON) M++;
        if ((sq->landmarks & LM_FOSSIL) && code_at(sq) < 6) M++;
        if (Factions[faction_id].SE_economy_pending >= 2) E++;
    }
    if (item == BIT_FOREST) {
        N += ResInfo.forest_sq.nutrient;
        M += ResInfo.forest_sq.mineral;
        E += ResInfo.forest_sq.energy;
    } else if (item == BIT_THERMAL_BORE) {
        N += ResInfo.borehole_sq.nutrient;
        M += ResInfo.borehole_sq.mineral;
        E += ResInfo.borehole_sq.energy;
        N -= (N > 0);
    } else if (item == BIT_FUNGUS) {
        N = fungus_yield(faction_id, RES_NUTRIENT);
        M = fungus_yield(faction_id, RES_MINERAL);
        E = fungus_yield(faction_id, RES_ENERGY);
    } else if (item == BIT_FARM && is_ocean(sq)) {
        N += ResInfo.ocean_sq.nutrient + ResInfo.improved_sea.nutrient;
        M += ResInfo.ocean_sq.mineral;
        E += ResInfo.ocean_sq.energy;
        if (sq->items & BIT_MINE) {
            M += ResInfo.improved_sea.mineral + has_tech(Rules.tech_preq_mining_platform_bonus, faction_id);
            N -= (N > 0);
        }
        if (sq->items & BIT_SOLAR) E += ResInfo.improved_sea.energy;
    }
    if (N > conf.tile_output_limit[0] && bonus != RES_NUTRIENT && !has_tech(Rules.tech_preq_allow_3_nutrients_sq, faction_id))
        N = conf.tile_output_limit[0];
    if (M > conf.tile_output_limit[1] && bonus != RES_MINERAL && !has_tech(Rules.tech_preq_allow_3_minerals_sq, faction_id))
        M = conf.tile_output_limit[1];
    if (E > conf.tile_output_limit[2] && bonus != RES_ENERGY && !has_tech(Rules.tech_preq_allow_3_energy_sq, faction_id))
        E = conf.tile_output_limit[2];
    return N + M + E;
}

}  // namespace thinker
