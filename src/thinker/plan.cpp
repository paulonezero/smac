// Thinker's plans for a faction's turn (thinker/plan.h). From Thinker's plan.cpp, veh.cpp (the
// best equipment) and move.cpp (update_main_region); the logic is Thinker's.
#include "thinker/plan.h"

#include <climits>

#include "thinker/basex.h"
#include "thinker/path.h"

namespace thinker {

AIPlans plans[MaxPlayerNum];

namespace {
int g_plan_upkeep_turn = -1;
}

void reset_plans() { g_plan_upkeep_turn = -1; }

int faction_might(int faction_id) { return plans[faction_id].mil_strength + 8 * Factions[faction_id].pop_total; }

int compare_might(int faction_id, int faction_id_tgt) {
    return 4 * faction_might(faction_id) >= 3 * faction_might(faction_id_tgt);
}

bool need_police(int faction_id) {
    Faction& f = Factions[faction_id];
    return f.SE_police > -2 && f.SE_police < 3 && !thinker::has_project(FAC_TELEPATHIC_MATRIX, faction_id);
}

int best_armor(int faction_id, int max_cost) {
    int best_id = ARM_NO_ARMOR;
    for (int i = ARM_NO_ARMOR; i <= ARM_RESONANCE_8_ARMOR; i++) {
        if (has_tech(Armor[i].preq_tech, faction_id)) {
            int cost = Armor[i].cost;
            if (max_cost > 0 && (cost > max_cost || cost > Armor[i].defense_value + 3)) continue;
            int diff = Armor[i].defense_value - Armor[best_id].defense_value;
            if (diff > 0 || (diff == 0 && Armor[best_id].cost >= cost)) best_id = i;
        }
    }
    return best_id;
}

int best_weapon(int faction_id) {
    int best_id = WPN_HAND_WEAPONS;
    for (int i = 0; i < MaxWeaponNum; i++) {
        if (has_tech(Weapon[i].preq_tech, faction_id) && Weapon[i].mode <= WMODE_MISSILE && Weapon[i].offense_value < 99 &&
            i != WPN_CONVENTIONAL_PAYLOAD) {
            int diff = Weapon[i].offense_value - Weapon[best_id].offense_value;
            if (diff > 0 || (diff == 0 && Weapon[best_id].cost >= Weapon[i].cost)) best_id = i;
        }
    }
    return best_id;
}

int proto_offense(int unit_id) {
    UNIT* u = &Units[unit_id];
    int atk_val = Weapon[u->weapon_id].offense_value;
    if (unit_is_planet_buster(u)) return atk_val * u->reactor_id;
    return (conf.ignore_reactor_power || atk_val < 0 ? atk_val * REC_FISSION : atk_val * u->reactor_id);
}

int proto_defense(int unit_id) {
    UNIT* u = &Units[unit_id];
    int def_val = Armor[u->armor_id].defense_value;
    return (conf.ignore_reactor_power || def_val < 0 ? def_val * REC_FISSION : def_val * u->reactor_id);
}

int psi_score(int faction_id) {
    Faction& f = Factions[faction_id];
    int weapon_value = Weapon[best_weapon(faction_id)].offense_value;
    int enemy_weapon_value = 0;
    for (int i = 1; i < MaxPlayerNum; i++) {
        if (faction_id != i && is_alive(i) && !has_pact(faction_id, i)) {
            enemy_weapon_value = max(enemy_weapon_value, (int)Weapon[best_weapon(i)].offense_value);
        }
    }
    int psi = max(-6, 2 - weapon_value) + (f.SE_planet_base > 0);
    if (thinker::has_project(FAC_NEURAL_AMPLIFIER, faction_id)) psi += min(5, conf.neural_amplifier_bonus / 20);
    if (thinker::has_project(FAC_DREAM_TWISTER, faction_id)) psi += min(5, conf.dream_twister_bonus / 20);
    if (thinker::has_project(FAC_PHOLUS_MUTAGEN, faction_id)) psi++;
    if (thinker::has_project(FAC_XENOEMPATHY_DOME, faction_id)) psi++;
    if (thinker::has_project(FAC_VOICE_OF_PLANET, faction_id)) psi++;
    if ((MFactions[faction_id].rule_flags & RFLAG_WORMPOLICE) && need_police(faction_id)) psi += 2;
    if (enemy_weapon_value) psi += clamp((enemy_weapon_value - weapon_value) / 2, -3, 3);
    psi += clamp(MFactions[faction_id].rule_psi / 10, -8, 8) + clamp(f.SE_planet, -8, 8);
    return psi;
}

int satellite_count(int faction_id, int item_id) {
    Faction& f = Factions[faction_id];
    switch (item_id) {
    case FAC_SKY_HYDRO_LAB: return f.satellites_nutrient;
    case FAC_ORBITAL_POWER_TRANS: return f.satellites_energy;
    case FAC_NESSUS_MINING_STATION: return f.satellites_mineral;
    case FAC_ORBITAL_DEFENSE_POD:
    default: return f.satellites_ODP;
    }
}

int satellite_goal(int faction_id, int item_id) {
    Faction& f = Factions[faction_id];
    AIPlans& p = plans[faction_id];
    int goal = p.satellite_goal;
    if (item_id == FAC_ORBITAL_DEFENSE_POD) {
        int nukes = 0;
        for (int i = 1; i < MaxPlayerNum; i++) {
            if (faction_id != i && is_alive(i) && at_war(faction_id, i)) nukes = max(nukes, Factions[i].planet_busters);
        }
        if (p.enemy_odp > 0 || p.enemy_sat > 0 || nukes > 2) {
            goal = clamp(goal / 4, 0, 4) + clamp(f.base_count / 8 + nukes / 2, 2, 12);
        } else {
            goal = clamp(goal / 4, 0, 4) + clamp(f.base_count / 8, 2, 4);
        }
    } else {
        if (f.base_count <= 5) goal /= 2;
    }
    if (f.base_count <= 10) goal /= 2;
    return clamp(goal, 0, conf.max_satellites);
}

int facility_score(int item_id, WItem& Wgov) {
    CFacility& p = Facility[item_id];
    return Wgov.AI_fight * p.AI_fight + Wgov.AI_growth * p.AI_growth + Wgov.AI_power * p.AI_power +
           Wgov.AI_tech * p.AI_tech + Wgov.AI_wealth * p.AI_wealth;
}

void governor_priorities(BASE& base, WItem& Wgov) {
    Faction& f = Factions[base.faction_id];
    uint32_t gov = (uint32_t)base.governor_flags;
    if (is_human(base.faction_id)) {
        Wgov.AI_growth = (gov & GOV_PRIORITY_EXPLORE ? 4 : 1);
        Wgov.AI_tech = (gov & GOV_PRIORITY_DISCOVER ? 4 : 1);
        Wgov.AI_wealth = (gov & GOV_PRIORITY_BUILD ? 4 : 1);
        Wgov.AI_power = (gov & GOV_PRIORITY_CONQUER ? 4 : 1);
        Wgov.AI_fight = clamp(((base.defend_goal / 2) - 1) * 2, -2, 2);
    } else {
        Wgov.AI_growth = (f.AI_growth ? 4 : 1);
        Wgov.AI_tech = (f.AI_tech ? 4 : 1);
        Wgov.AI_wealth = (f.AI_wealth ? 4 : 1);
        Wgov.AI_power = (f.AI_power ? 4 : 1);
        Wgov.AI_fight = clamp(2 * f.AI_fight, -2, 2);
    }
}

// Thinker's plan.cpp: what the faction wants of fungus and tubes this turn.
void former_plans(int faction_id) {
    CFacility& facility = Facility[FAC_TREE_FARM];
    bool tree_farm = has_tech(facility.preq_tech, faction_id) && facility.cost + facility.maint < 20;
    bool former_fungus = (has_terra(FORMER_PLANT_FUNGUS, TRIAD_LAND, faction_id) ||
                          has_terra(FORMER_PLANT_FUNGUS, TRIAD_SEA, faction_id));
    bool improv_fungus = has_tech(Rules.tech_preq_improv_fungus, faction_id) ||
                         has_tech(Rules.tech_preq_build_road_fungus, faction_id) ||
                         thinker::has_project(FAC_XENOEMPATHY_DOME, faction_id);
    int value = fungus_yield(faction_id, RES_NONE) -
                (has_terra(FORMER_FOREST, TRIAD_LAND, faction_id)
                     ? tree_farm + ResInfo.forest_sq.nutrient + ResInfo.forest_sq.mineral + ResInfo.forest_sq.energy
                     : 2);
    plans[faction_id].keep_fungus = clamp(2 * value, 0, (improv_fungus ? 8 : 4));
    plans[faction_id].plant_fungus = former_fungus && value >= 0 &&
                                     value + improv_fungus + thinker::has_project(FAC_MANIFOLD_HARMONICS, faction_id) > 1;
    plans[faction_id].build_tubes = has_terra(FORMER_MAGTUBE, TRIAD_LAND, faction_id);
}

void update_main_region(int faction_id) {
    for (int i = 0; i < MaxPlayerNum; i++) {
        AIPlans& p = plans[i];
        p.main_region = -1;
        p.main_region_x = -1;
        p.main_region_y = -1;
        if (i == faction_id) {
            p.prioritize_naval = 0;
            p.main_sea_region = -1;
            p.target_land_region = -1;
            p.naval_start_x = -1;
            p.naval_start_y = -1;
        }
    }
    for (int i = 0, cnt = BaseCount; i < cnt; ++i) {
        BASE* base = &Bases[i];
        MAP* sq = mapsq(base->x, base->y);
        AIPlans& p = plans[base->faction_id];
        if (p.main_region < 0 && sq && !is_ocean(sq)) {
            p.main_region = sq->region;
            p.main_region_x = base->x;
            p.main_region_y = base->y;
        }
    }
    AIPlans& p = plans[faction_id];
    if (p.main_region < 0) return;
    // Prioritize naval invasions if the closest enemy is on another region
    MAP* sq;
    TileSearch ts;
    ts.init(p.main_region_x, p.main_region_y, TS_TERRITORY_SHORE, 2);
    int i = 0;
    int k = 0;
    while (++i <= 800 && (sq = ts.get_next()) != nullptr) {
        if (at_war(faction_id, sq->owner) && !is_ocean(sq) && ++k >= 10) {
            p.prioritize_naval = 0;
            return;
        }
    }
    int min_dist = INT_MAX;
    for (i = 1; i < MaxPlayerNum; i++) {
        int dist = (plans[i].main_region_x < 0
                        ? MaxEnemyRange
                        : map_range(p.main_region_x, p.main_region_y, plans[i].main_region_x, plans[i].main_region_y));
        if (at_war(faction_id, i) && dist < min_dist) {
            min_dist = dist;
            p.prioritize_naval = p.main_region != plans[i].main_region;
        }
    }
}

// Thinker's plans_upkeep for a human faction with manage_player_bases or manage_player_units:
// once a turn. (Thinker's AI factions run it every turn of their own; not ported.)
void plans_upkeep(int faction_id) {
    if (!faction_id || !is_alive(faction_id) || !is_human(faction_id)) return;
    if (g_plan_upkeep_turn == CurrentTurn) return;
    g_plan_upkeep_turn = CurrentTurn;
    std::vector<int> base_pop;
    std::vector<int> base_min;
    std::vector<int> base_eng;
    Faction* f = &Factions[faction_id];
    AIPlans* p = &plans[faction_id];
    *p = AIPlans();
    update_main_region(faction_id);
    former_plans(faction_id);

    for (int i = 0; i < MaxPlayerNum; i++) plans[i].mil_strength = 0;
    for (int i = 0, cnt = VehCount; i < cnt; ++i) {
        VEH* veh = &Vehs[i];
        if (veh->faction_id > 0) {
            int reactor_val = is_planet_buster(veh);
            plans[veh->faction_id].mil_strength +=
                (reactor_val > 0 ? 80 * reactor_val : (2 * eval_offense(veh) + eval_defense(veh))) *
                (chassis_speed(veh) > 1 ? 3 : 2);
        }
        if (veh->faction_id == faction_id) {
            int tri = triad(veh);
            if (is_combat_unit(veh) || is_garrison_unit(veh) || is_probe(veh) || is_transport(veh)) {
                if (tri == TRIAD_LAND) {
                    p->land_combat_units++;
                } else if (tri == TRIAD_SEA) {
                    p->sea_combat_units++;
                    p->transport_units += is_transport(veh);
                } else {
                    p->air_combat_units++;
                }
                if (is_probe(veh)) p->probe_units++;
                if (is_missile(veh)) p->missile_units++;
            }
            if (veh_plan(veh) <= PLAN_RECON) {
                p->max_offense_value = max(p->max_offense_value, proto_offense(veh->unit_id));
                p->max_defense_value = max(p->max_defense_value, proto_defense(veh->unit_id));
            }
        }
    }
    debug("plans_totals %d %d bases: %3d land: %3d sea: %3d air: %3d probe: %3d missile: %3d transport: %3d might: %d\n",
          CurrentTurn, faction_id, f->base_count, p->land_combat_units, p->sea_combat_units, p->air_combat_units,
          p->probe_units, p->missile_units, p->transport_units, faction_might(faction_id));

    for (int i = 1; i < MaxPlayerNum; i++) {
        if (faction_id != i && is_alive(i)) {
            if (has_treaty(faction_id, i, DIPLO_COMMLINK)) p->contacted_factions++;
            else p->unknown_factions++;
            if (at_war(faction_id, i)) {
                p->enemy_factions++;
                p->enemy_odp += Factions[i].satellites_ODP;
                p->enemy_sat += Factions[i].satellites_nutrient;
                p->enemy_sat += Factions[i].satellites_mineral;
                p->enemy_sat += Factions[i].satellites_energy;
            }
            float factor = clamp((is_human(i) ? 2.0f : 1.0f) * (has_treaty(faction_id, i, DIPLO_COMMLINK) ? 1.0f : 0.5f) *
                                     (at_war(faction_id, i) ? 1.0f : (has_pact(faction_id, i) ? 0.1f : 0.25f)) *
                                     (p->main_region == plans[i].main_region ? 1.5f : 1.0f) * faction_might(i) /
                                     max(1, faction_might(faction_id)),
                                 0.01f, 8.0f);
            p->enemy_mil_factor = max(p->enemy_mil_factor, factor);
        }
    }
    int enemy_sum = 0;
    int n = 0;
    for (int i = 0, cnt = BaseCount; i < cnt; ++i) {
        BASE* base = &Bases[i];
        if (base->faction_id == faction_id) {
            base_pop.push_back(base->pop_size);
            base_min.push_back(base->mineral_surplus);
            base_eng.push_back(base->energy_surplus);
            n++;
            // Update enemy base threat distances
            int base_region = region_at(base->x, base->y);
            int enemy_range = MaxEnemyRange;
            for (int j = 0; j < cnt; ++j) {
                BASE* b = &Bases[j];
                if (faction_id != b->faction_id && !has_pact(faction_id, b->faction_id)) {
                    int border_dist = map_range(base->x, base->y, b->x, b->y);
                    int enemy_dist = border_dist * (region_at(b->x, b->y) == base_region ? 2 : 3) *
                                     (at_war(faction_id, b->faction_id) ? 1 : 4);
                    enemy_range = min(enemy_range, enemy_dist / 2);
                }
            }
            base->defend_range = (int8_t)enemy_range;
            enemy_sum += enemy_range;
            if (base->faction_id_former != faction_id && at_war(faction_id, base->faction_id_former)) p->captured_bases++;
        } else if (base->faction_id_former == faction_id && at_war(faction_id, base->faction_id)) {
            p->enemy_bases++;
        }
    }
    p->enemy_base_range = (n > 0 ? (1.0f * enemy_sum) / n : MaxEnemyRange);
    p->psi_score = psi_score(faction_id);
    if (!p->enemy_factions) {
        p->defense_modifier = clamp(f->AI_fight + f->AI_power + f->base_count / 20, 0, 2);
    } else {
        p->defense_modifier = clamp((int)(3.0f - p->enemy_base_range / 8.0f) + min(2, f->base_count / 20) +
                                        min(4, p->enemy_bases / 2) + min(2, p->captured_bases / 2) +
                                        min(2, p->enemy_factions / 2),
                                    1, 4);
    }
    std::sort(base_pop.begin(), base_pop.end());
    std::sort(base_min.begin(), base_min.end());
    std::sort(base_eng.begin(), base_eng.end());
    bool full_value = has_tech(Facility[FAC_AEROSPACE_COMPLEX].preq_tech, faction_id) ||
                      thinker::has_project(FAC_CLOUDBASE_ACADEMY, faction_id) ||
                      thinker::has_project(FAC_SPACE_ELEVATOR, faction_id);
    if (n > 0) {
        if (n >= 40) p->project_limit = max(5, base_min[(size_t)n * 3 / 4]);
        else p->project_limit = max(5, base_min[(size_t)n * 2 / 3]);
        p->median_limit = max(5, base_min[(size_t)n / 2]);
        p->energy_limit = max(15, base_eng[(size_t)n * 3 / 4]);
        p->satellite_goal = min(conf.max_satellites, base_pop[(size_t)n * 7 / 8] * (full_value ? 1 : 2));
    } else {
        p->project_limit = 5;
        p->median_limit = 5;
        p->energy_limit = 15;
        p->satellite_goal = 0;
    }
    debug("plans_upkeep %d %d proj_limit: %2d sat_goal: %2d psi: %2d keep_fungus: %d plant_fungus: %d enemy_bases: %d "
          "enemy_mil: %.4f enemy_range: %.4f\n",
          CurrentTurn, faction_id, p->project_limit, p->satellite_goal, p->psi_score, p->keep_fungus, p->plant_fungus,
          p->enemy_bases, p->enemy_mil_factor, p->enemy_base_range);
}

}  // namespace thinker
