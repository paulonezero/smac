// Thinker's plans for a faction's turn (its plan.h / plan.cpp, the part the governor and the
// formers read), on OpenSMAC (thinker.h).
#pragma once
#include "thinker/compat.h"

namespace thinker {

// A faction's plans for the turn (Thinker's AIPlans).
struct AIPlans {
    int main_region = -1;
    int main_region_x = -1;
    int main_region_y = -1;
    int main_sea_region = -1;
    int target_land_region = -1;
    int prioritize_naval = 0;
    int naval_start_x = -1;
    int naval_start_y = -1;
    int land_combat_units = 0;
    int sea_combat_units = 0;
    int air_combat_units = 0;
    int probe_units = 0;
    int missile_units = 0;
    int transport_units = 0;
    int unknown_factions = 0;
    int contacted_factions = 0;
    int enemy_factions = 0;
    int build_tubes = 0;
    // Amount of minerals a base needs to produce before it is allowed to build secret projects.
    int project_limit = 5;
    int median_limit = 5;
    int energy_limit = 15;
    // PSI combat units are only selected for production if this score is higher than zero.
    int psi_score = 0;
    int keep_fungus = 0;
    int plant_fungus = 0;
    int satellite_goal = 0;
    int enemy_odp = 0;
    int enemy_sat = 0;
    int mil_strength = 0;
    int defense_modifier = 0;
    int max_offense_value = 0;
    int max_defense_value = 0;
    float enemy_base_range = 0;
    float enemy_mil_factor = 0;
    int enemy_bases = 0;
    int captured_bases = 0;
};

// The governor's weights for the AI priorities (Thinker's WItem).
struct WItem {
    int AI_growth;
    int AI_tech;
    int AI_wealth;
    int AI_power;
    int AI_fight;
};

extern AIPlans plans[MaxPlayerNum];

// The player's plans for the turn, once a turn (Thinker's plans_upkeep in its governor mode).
void plans_upkeep(int faction_id);
// Every faction's first land base region; the faction's naval priority.
void update_main_region(int faction_id);
// What the faction wants of fungus and tubes (keep_fungus, plant_fungus, build_tubes).
void former_plans(int faction_id);
void reset_plans();  // a new game or load: the plans are stale

int faction_might(int faction_id);
int compare_might(int faction_id, int faction_id_tgt);
bool need_police(int faction_id);
int psi_score(int faction_id);
int satellite_count(int faction_id, int item_id);
int satellite_goal(int faction_id, int item_id);
int facility_score(int item_id, WItem& Wgov);
void governor_priorities(BASE& base, WItem& Wgov);

int best_armor(int faction_id, int max_cost);
int best_weapon(int faction_id);
int proto_offense(int unit_id);
int proto_defense(int unit_id);

}  // namespace thinker
