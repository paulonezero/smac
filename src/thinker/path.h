// Thinker's tile search and the helpers around it (its path.h / path.cpp), on OpenSMAC (thinker.h).
#pragma once
#include "thinker/compat.h"

namespace thinker {

const int NearbyTiles[][2] = {{1, -1}, {2, 0}, {1, 1}, {0, 2}, {-1, 1}, {-2, 0}, {-1, -1}, {0, -2}};
const int BaseOffsetX[] = {1, 2, 1, 0, -1, -2, -1, 0, 0};
const int BaseOffsetY[] = {-1, 0, 1, 2, 1, 0, -1, -2, 0};
const int MaxTableRange = 8;
const int TableRange[] = {1, 9, 25, 49, 81, 121, 169, 225, 289};
extern const int TableOffsetX[289];
extern const int TableOffsetY[289];

const int PathLimit = 160;
const int QueueSize = 8192;
const int MaxTileSearchType = 6;

enum TSType {
    TS_TRIAD_LAND = 0,
    TS_TRIAD_SEA = 1,
    TS_TRIAD_AIR = 2,
    TS_SEA_AND_SHORE = 3,
    TS_TERRITORY_LAND = 4,
    TS_TERRITORY_SHORE = 5,
    TS_TERRITORY_PACT = 6,
};

enum NodesetType {
    NODE_BOREHOLE,  // Borehole being built
    NODE_RAISE_LAND,  // Land raise action initiated
    NODE_SENSOR_ARRAY,  // Sensor being built
    NODE_NEED_FERRY,
    NODE_BASE_SITE,  // Skip from search
    NODE_CONVOY_SITE,  // Skip from search
    NODE_GOAL_RAISE_LAND,  // Former priority only
    NODE_NAVAL_START,
    NODE_NAVAL_BEACH,
    NODE_NAVAL_END,
    NODE_NAVAL_PICK,
    NODE_SCOUT_SITE,
    NODE_PATROL,  // Includes probes/transports
    NODE_COMBAT_PATROL,  // Only attack-capable units
};

struct PathNode {
    int x;
    int y;
    int dist;
    int prev;
};

// A breadth-first search of the tiles reachable from a start, by movement type.
class TileSearch {
    int type = 0;
    int head = 0;
    int tail = 0;
    MAP* sq = nullptr;
    void reset();
    void add_start(int x, int y);

public:
    int rx = 0, ry = 0, dist = 0, current = 0, faction_id = -1, y_skip = 0;
    std::vector<PathNode> paths;  // QueueSize entries (Thinker has the array in the object)
    Points oldtiles;
    TileSearch() : paths((size_t)QueueSize) {}
    void init(int x, int y, int ts_type);
    void init(int x, int y, int ts_type, int ts_skip);
    void init(const PointList& points, TSType ts_type, int ts_skip);
    void adjust_roads(PMTable& tbl, int value);
    bool has_zoc(int plr_id);
    MAP* get_next();
};

int route_dist(PMTable& tbl, int x1, int y1, int x2, int y2);
std::vector<MapTile> iterate_tiles(int x, int y, size_t start_index, size_t end_index);
int nearby_items(int x, int y, size_t start_index, size_t end_index, uint32_t item);
bool defend_tile(VEH* veh, MAP* sq);
bool has_transport(int x, int y, int faction_id);
int move_to_base(int veh_id, bool ally);
int escape_move(int veh_id);
int search_escape(TileSearch& ts, int veh_id, int* tx, int* ty);
int search_base(TileSearch& ts, int veh_id, bool ally, int* tx, int* ty);

}  // namespace thinker
