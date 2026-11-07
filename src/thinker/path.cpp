// Thinker's tile search and the helpers around it (thinker/path.h). From Thinker's path.cpp,
// with the engine's names as compat.h spells them; the logic is Thinker's.
#include "thinker/path.h"

#include "thinker/move.h"

namespace thinker {

const int TableOffsetX[289] = {
    0,   1,   2,   1,   0,   -1,  -2,  -1,  0,   2,   2,   -2,  -2,  1,   3,   3,   1,   -1,  -3,  -3,  -1,  4,
    -4,  0,   0,   1,   2,   3,   4,   5,   5,   4,   3,   2,   1,   -1,  -2,  -3,  -4,  -5,  -5,  -4,  -3,  -2,
    -1,  0,   6,   0,   -6,  0,   1,   2,   3,   4,   5,   6,   7,   8,   7,   6,   5,   4,   3,   2,   1,   0,
    -1,  -2,  -3,  -4,  -5,  -6,  -7,  -8,  -7,  -6,  -5,  -4,  -3,  -2,  -1,  0,   1,   2,   3,   4,   5,   6,
    7,   8,   9,   10,  9,   8,   7,   6,   5,   4,   3,   2,   1,   0,   -1,  -2,  -3,  -4,  -5,  -6,  -7,  -8,
    -9,  -10, -9,  -8,  -7,  -6,  -5,  -4,  -3,  -2,  -1,  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,
    11,  12,  11,  10,  9,   8,   7,   6,   5,   4,   3,   2,   1,   0,   -1,  -2,  -3,  -4,  -5,  -6,  -7,  -8,
    -9,  -10, -11, -12, -11, -10, -9,  -8,  -7,  -6,  -5,  -4,  -3,  -2,  -1,  0,   1,   2,   3,   4,   5,   6,
    7,   8,   9,   10,  11,  12,  13,  14,  13,  12,  11,  10,  9,   8,   7,   6,   5,   4,   3,   2,   1,   0,
    -1,  -2,  -3,  -4,  -5,  -6,  -7,  -8,  -9,  -10, -11, -12, -13, -14, -13, -12, -11, -10, -9,  -8,  -7,  -6,
    -5,  -4,  -3,  -2,  -1,  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,
    15,  14,  13,  12,  11,  10,  9,   8,   7,   6,   5,   4,   3,   2,   1,   0,   -1,  -2,  -3,  -4,  -5,  -6,
    -7,  -8,  -9,  -10, -11, -12, -13, -14, -15, -16, -15, -14, -13, -12, -11, -10, -9,  -8,  -7,  -6,  -5,  -4,
    -3,  -2,  -1,
};

const int TableOffsetY[289] = {
    0,   -1,  0,   1,   2,   1,   0,   -1,  -2,  -2,  2,   2,   -2,  -3,  -1,  1,   3,   3,   1,   -1,  -3,  0,
    0,   4,   -4,  -5,  -4,  -3,  -2,  -1,  1,   2,   3,   4,   5,   5,   4,   3,   2,   1,   -1,  -2,  -3,  -4,
    -5,  6,   0,   -6,  0,   -8,  -7,  -6,  -5,  -4,  -3,  -2,  -1,  0,   1,   2,   3,   4,   5,   6,   7,   8,
    7,   6,   5,   4,   3,   2,   1,   0,   -1,  -2,  -3,  -4,  -5,  -6,  -7,  -10, -9,  -8,  -7,  -6,  -5,  -4,
    -3,  -2,  -1,  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  9,   8,   7,   6,   5,   4,   3,   2,
    1,   0,   -1,  -2,  -3,  -4,  -5,  -6,  -7,  -8,  -9,  -12, -11, -10, -9,  -8,  -7,  -6,  -5,  -4,  -3,  -2,
    -1,  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  11,  10,  9,   8,   7,   6,   5,   4,
    3,   2,   1,   0,   -1,  -2,  -3,  -4,  -5,  -6,  -7,  -8,  -9,  -10, -11, -14, -13, -12, -11, -10, -9,  -8,
    -7,  -6,  -5,  -4,  -3,  -2,  -1,  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,
    13,  12,  11,  10,  9,   8,   7,   6,   5,   4,   3,   2,   1,   0,   -1,  -2,  -3,  -4,  -5,  -6,  -7,  -8,
    -9,  -10, -11, -12, -13, -16, -15, -14, -13, -12, -11, -10, -9,  -8,  -7,  -6,  -5,  -4,  -3,  -2,  -1,  0,
    1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  15,  14,  13,  12,  11,  10,
    9,   8,   7,   6,   5,   4,   3,   2,   1,   0,   -1,  -2,  -3,  -4,  -5,  -6,  -7,  -8,  -9,  -10, -11, -12,
    -13, -14, -15,
};

void TileSearch::reset() {
    type = 0;
    head = 0;
    tail = 0;
    dist = 0;
    current = 0;
    y_skip = 0;
    faction_id = -1;
    oldtiles.clear();
}

void TileSearch::add_start(int x, int y) {
    assert(type >= 0 && type <= MaxTileSearchType);
    if (tail < QueueSize && mapsq(x, y) && !oldtiles.count({x, y})) {
        paths[(size_t)tail] = {x, y, 0, -1};
        oldtiles.insert({x, y});
        tail++;
    }
}

void TileSearch::init(int x, int y, int ts_type) {
    reset();
    type = ts_type;
    add_start(x, y);
}

void TileSearch::init(int x, int y, int ts_type, int ts_skip) {
    reset();
    type = ts_type;
    y_skip = ts_skip;
    add_start(x, y);
}

void TileSearch::init(const PointList& points, TSType ts_type, int ts_skip) {
    reset();
    type = ts_type;
    y_skip = ts_skip;
    for (auto& p : points) add_start(p.x, p.y);
}

void TileSearch::adjust_roads(PMTable& tbl, int value) {
    for (int p = current, i = 0; p >= 0 && i <= PathLimit; ++i) {
        tbl[{paths[(size_t)p].x, paths[(size_t)p].y}].roads += value;
        p = paths[(size_t)p].prev;
    }
}

// Traverse current search path and check for zones of control.
bool TileSearch::has_zoc(int plr_id) {
    bool prev_zoc = false;
    for (int p = current, i = 0; p >= 0 && i <= PathLimit; ++i) {
        if (zoc_any(paths[(size_t)p].x, paths[(size_t)p].y, plr_id)) {
            if (prev_zoc) return true;
            prev_zoc = true;
        } else {
            prev_zoc = false;
        }
        p = paths[(size_t)p].prev;
    }
    return false;
}

// Implement a breadth-first search of adjacent map tiles to iterate possible movement paths.
// Pathnodes are also used to keep track of the route to reach the current destination.
MAP* TileSearch::get_next() {
    while (head < tail) {
        rx = paths[(size_t)head].x;
        ry = paths[(size_t)head].y;
        dist = paths[(size_t)head].dist;
        current = head;
        head++;
        if (!(sq = mapsq(rx, ry))) continue;
        if (!current && faction_id < 0) {
            faction_id = sq_veh_owner(sq);
            if (faction_id < 0) faction_id = sq->owner;
        }
        bool foreign = is_owned(sq) && sq->owner != faction_id && !has_pact(faction_id, sq->owner);
        bool our_base = is_base(sq) && !foreign;
        bool skip = (is_base(sq) && !our_base) || (type == TS_TRIAD_LAND && is_ocean(sq)) ||
                    (type == TS_TRIAD_SEA && !is_ocean(sq) && !our_base) ||
                    (type == TS_SEA_AND_SHORE && !is_ocean(sq) && !our_base) ||
                    (type == TS_TERRITORY_LAND && (is_ocean(sq) || sq->owner != faction_id)) ||
                    (type == TS_TERRITORY_SHORE && (is_ocean(sq) || sq->owner != faction_id)) ||
                    (type == TS_TERRITORY_PACT && (is_ocean(sq) || foreign));
        if (dist > 0 && skip) {
            if ((type == TS_TRIAD_LAND && is_ocean(sq)) || (type == TS_TRIAD_SEA && !is_ocean(sq)) ||
                (type == TS_TERRITORY_LAND && (is_ocean(sq) || sq->owner != faction_id)) ||
                (type == TS_TERRITORY_PACT && foreign)) {
                continue;
            }
            return sq;
        }
        for (const auto& p : NearbyTiles) {
            int x2 = wrap(rx + p[0]);
            int y2 = ry + p[1];
            if (y2 >= y_skip && y2 < MapAreaY - y_skip && x2 >= 0 && x2 < MapAreaX && tail < QueueSize &&
                dist < PathLimit && !oldtiles.count({x2, y2})) {
                paths[(size_t)tail] = {x2, y2, dist + 1, current};
                tail++;
                oldtiles.insert({x2, y2});
            }
        }
        if (dist > 0) return sq;
    }
    return nullptr;
}

int route_dist(PMTable& tbl, int x1, int y1, int x2, int y2) {
    Points visited;
    std::list<PathNode> items;
    items.push_back({x1, y1, 0, 0});
    int limit = max(8, map_range(x1, y1, x2, y2) * 2);
    int i = 0;
    while (items.size() > 0 && ++i <= PathLimit) {
        PathNode cur = items.front();
        items.pop_front();
        if (cur.x == x2 && cur.y == y2 && cur.dist <= limit) {
            debug_ver("route_dist %2d %2d -> %2d %2d = %d %d\n", x1, y1, x2, y2, i, cur.dist);
            return cur.dist;
        }
        for (const auto& t : NearbyTiles) {
            int rx = wrap(cur.x + t[0]);
            int ry = cur.y + t[1];
            if (mapsq(rx, ry) && tbl[{rx, ry}].roads > 0 && !visited.count({rx, ry})) {
                items.push_back({rx, ry, cur.dist + 1, 0});
                visited.insert({rx, ry});
            }
        }
    }
    return -1;
}

std::vector<MapTile> iterate_tiles(int x, int y, size_t start_index, size_t end_index) {
    std::vector<MapTile> tiles;
    assert(start_index < end_index && end_index <= (size_t)TableRange[MaxTableRange]);
    for (size_t i = start_index; i < end_index; i++) {
        int x2 = wrap(x + TableOffsetX[i]);
        int y2 = y + TableOffsetY[i];
        MAP* sq = mapsq(x2, y2);
        if (sq) tiles.push_back({x2, y2, (int)i, sq});
    }
    return tiles;
}

int nearby_items(int x, int y, size_t start_index, size_t end_index, uint32_t item) {
    assert(start_index < end_index && end_index <= (size_t)TableRange[MaxTableRange]);
    int n = 0;
    for (size_t i = start_index; i < end_index; i++) {
        int x2 = wrap(x + TableOffsetX[i]);
        int y2 = y + TableOffsetY[i];
        MAP* sq = mapsq(x2, y2);  // Thinker reads bit_at: off the map it is 0 here
        if (sq && (sq->items & item)) n++;
    }
    return n;
}

bool defend_tile(VEH* veh, MAP* sq) {
    if (!sq || ((sq->items & BIT_MONOLITH) && need_monolith(veh))) return true;
    if (is_fungus(sq) && is_native_unit(veh) && need_heals(veh)) return true;
    if (sq->items & (BIT_BASE_IN_TILE | BIT_BUNKER)) {
        if (need_heals(veh)) return true;
        if (!is_combat_unit(veh)) {
            int n = 0;
            for (auto& m : iterate_tiles(veh->x, veh->y, 1, 9)) {
                if (mapdata[{m.x, m.y}].safety < PM_SAFE && ++n > 3) return true;
            }
        }
    }
    return false;
}

bool has_transport(int x, int y, int faction_id) {
    for (int i = VehCount - 1; i >= 0; --i) {
        VEH* veh = &Vehs[i];
        if (veh->x == x && veh->y == y && is_transport(veh) && veh->faction_id == faction_id) return true;
    }
    return false;
}

int move_to_base(int veh_id, bool ally) {
    VEH* veh = &Vehs[veh_id];
    int tx;
    int ty;
    TileSearch ts;
    search_base(ts, veh_id, ally, &tx, &ty);
    if (tx >= 0) {
        debug("move_to_base %2d %2d -> %2d %2d %s\n", veh->x, veh->y, tx, ty, veh_name(veh));
        return set_move_to(veh_id, tx, ty);
    }
    return mod_veh_skip(veh_id);
}

int escape_move(int veh_id) {
    VEH* veh = &Vehs[veh_id];
    MAP* sq = mapsq(veh->x, veh->y);
    if (defend_tile(veh, sq)) return set_order_none(veh_id);
    int tx = -1;
    int ty = -1;
    TileSearch ts;
    if (search_escape(ts, veh_id, &tx, &ty)) return set_move_to(veh_id, tx, ty);
    return mod_veh_skip(veh_id);
}

static int escape_score(int x, int y, int range, VEH* veh, MAP* sq) {
    return mapdata[{x, y}].safety - 32 * mapdata[{x, y}].target - 128 * range +
           ((sq->items & BIT_MONOLITH) && need_monolith(veh) ? 2000 : 0) + ((sq->items & BIT_BUNKER) ? 500 : 0) +
           ((sq->items & BIT_FOREST) || is_rocky(sq) ? 200 : 0) + (mapnodes.count({x, y, NODE_PATROL}) ? 500 : 0);
}

int search_escape(TileSearch& ts, int veh_id, int* tx, int* ty) {
    VEH* veh = &Vehs[veh_id];
    MAP* sq = mapsq(veh->x, veh->y);
    *tx = -1;
    *ty = -1;
    if (!sq || veh_id < 0) return false;
    int best_score = escape_score(veh->x, veh->y, 0, veh, sq);
    int score;
    ts.init(veh->x, veh->y, triad(veh));
    while ((sq = ts.get_next()) != nullptr && ts.dist <= 8) {
        if (ts.dist > 2 && best_score > 500) break;
        if (!non_ally_in_tile(ts.rx, ts.ry, veh->faction_id) &&
            (!is_base(sq) || sq->owner == veh->faction_id || has_pact(veh->faction_id, sq->owner)) &&
            (score = escape_score(ts.rx, ts.ry, ts.dist, veh, sq)) > best_score &&
            !ts.has_zoc(veh->faction_id)) {  // Always check for zocs just in case
            *tx = ts.rx;
            *ty = ts.ry;
            best_score = score;
            debug("escape_score %2d %2d -> %2d %2d dist: %d score: %d\n", veh->x, veh->y, ts.rx, ts.ry, ts.dist,
                  score);
        }
    }
    return *tx >= 0;
}

int search_base(TileSearch& ts, int veh_id, bool ally, int* tx, int* ty) {
    VEH* veh = &Vehs[veh_id];
    MAP* sq = mapsq(veh->x, veh->y);
    *tx = -1;
    *ty = -1;
    if (veh_id < 0 || !sq) return 0;
    if (is_base(sq) && (sq->owner == veh->faction_id || ally)) return 0;
    int tri = triad(veh);
    int type = (tri == TRIAD_SEA ? TS_SEA_AND_SHORE : tri);
    int score;
    int best_score;
    int max_dist;
    if (tri == TRIAD_AIR && need_refuel(veh)) {
        best_score = INT32_MIN;
        max_dist = max(0, (veh_speed(veh_id, 0) - veh->moves_spent) / Rules.move_rate_roads);
    } else {
        best_score = escape_score(veh->x, veh->y, 0, veh, sq);
        max_dist = 20;
    }
    bool found = false;
    ts.init(veh->x, veh->y, type, 1);
    while ((sq = ts.get_next()) != nullptr && ts.dist <= max_dist) {
        if (is_base(sq)) {
            if (sq->owner == veh->faction_id || (ally && has_pact(veh->faction_id, sq->owner))) {
                *tx = ts.rx;
                *ty = ts.ry;
                found = true;
                if (tri == TRIAD_AIR || random(2)) break;
            }
        } else if (!found && (ts.dist <= 5 || tri == TRIAD_AIR) && allow_move(ts.rx, ts.ry, veh->faction_id, tri) &&
                   (score = escape_score(ts.rx, ts.ry, ts.dist, veh, sq)) > best_score) {
            if ((tri == TRIAD_AIR && is_airbase(sq)) || (tri != TRIAD_AIR && !ts.has_zoc(veh->faction_id))) {
                *tx = ts.rx;
                *ty = ts.ry;
                best_score = score;
            }
        }
    }
    return *tx >= 0;
}

}  // namespace thinker
