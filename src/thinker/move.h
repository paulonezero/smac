// Thinker's unit movement: the turn's map data (move_upkeep) and the former logic (its move.h /
// move.cpp, the part that formers use), on OpenSMAC (thinker.h).
#pragma once
#include "thinker/basex.h"
#include "thinker/compat.h"
#include "thinker/path.h"
#include "thinker/plan.h"

namespace thinker {

enum UpdateMode { UM_Full, UM_Visual, UM_Player };
enum StackType { ST_NeutralOnly, ST_NonPactOnly, ST_EnemyOnly, ST_EnemyOneUnit };
enum FormerMode { FM_Auto_Full, FM_Auto_Roads, FM_Auto_Tubes, FM_Auto_Sensors, FM_Remove_Fungus, FM_Farm_Road, FM_Mine_Road };

const uint32_t BIT_SIMPLE = (BIT_FARM | BIT_MINE | BIT_SOLAR | BIT_FOREST);
const uint32_t BIT_ADVANCED = (BIT_CONDENSER | BIT_THERMAL_BORE);

const int PM_SAFE = -20;
const int PM_NEAR_SAFE = -40;
const int PM_ShoreLine = 0x100;
const int PM_ShoreMask = 0xff;
const int PM_NearOwnBase = 0x10000;
const int PM_PsiGateBase = 0x20000;
const int PM_LandBaseRds = 0x40000;

extern PMTable mapdata;
extern NodeSet mapnodes;
extern int move_upkeep_faction;

// The turn's map data for the faction: tile values, roads wanted, safety, the former priorities.
// Reseeds Thinker's random numbers from the turn.
void move_upkeep(int faction_id, UpdateMode mode);

bool non_ally_in_tile(int x, int y, int faction_id);
bool allow_move(int x, int y, int faction_id, int triad);
bool allow_civ_move(int x, int y, int faction_id, int triad);
int coast_tiles(int x, int y);
bool near_sea_coast(int x, int y);

int set_move_to(int veh_id, int x, int y);
int set_action(int veh_id, int act, char icon);
int set_order_none(int veh_id);
int mod_veh_skip(int veh_id);

// The terraforming to do at (x, y) for the faction in the former's mode, or FORMER_NONE.
int select_item(int x, int y, int faction_id, FormerMode mode, MAP* sq);
int former_tile_score(int x, int y, int faction_id, MAP* sq);
// Thinker's move for a former: terraform here, go to the tile worth most, or wait.
int former_move(int id);

}  // namespace thinker
