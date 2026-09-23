// solver.h — Micromouse maze solver (classic 16x16, UKMARS rules)
//
// Portable C++11, header-only, no heap, no STL. Compiles unchanged on an
// ESP32 (Arduino), on a desktop for the offline harness, and behind the mms
// simulator adapter. It knows nothing about motors or sensors.
//
// Contract with the outside world (see Mouse below):
//   observe(left, front, right)  <- wall readings relative to the mouse
//   next()                       -> the move to execute (F/L/R/B/STOP)
//   applyMove(m)                 <- call after the motion layer completes it
//
// Move semantics: every move ends at the centre of an adjacent cell.
//   F: forward one cell.  L/R: turn 90° then forward one cell.  B: turn 180° then forward one.
// The motion layer may implement L/R as pivot+forward now and as an arc later
// without changing anything here.

#pragma once
#include <stdint.h>

namespace mm {

// ---------------------------------------------------------------- geometry
enum Dir : uint8_t { N = 0, E = 1, S = 2, W = 3 };
enum Move : uint8_t { MOVE_FORWARD = 0, MOVE_LEFT = 1, MOVE_RIGHT = 2, MOVE_BACK = 3, MOVE_STOP = 4 };

static const uint8_t  SIZE  = 16;
static const uint16_t CELLS = 256;
static const uint16_t INF   = 0xFFFF;

inline uint8_t cellIndex(uint8_t x, uint8_t y) { return (uint8_t)(x + SIZE * y); }
inline uint8_t cellX(uint8_t c) { return c % SIZE; }
inline uint8_t cellY(uint8_t c) { return c / SIZE; }

inline int8_t dx(Dir d) { return d == E ? 1 : d == W ? -1 : 0; }
inline int8_t dy(Dir d) { return d == N ? 1 : d == S ? -1 : 0; }
inline Dir leftOf(Dir d)     { return (Dir)((d + 3) & 3); }
inline Dir rightOf(Dir d)    { return (Dir)((d + 1) & 3); }
inline Dir opposite(Dir d)   { return (Dir)((d + 2) & 3); }
inline char dirChar(Dir d)   { return "NESW"[d]; }
inline char moveChar(Move m) { return "FLRB."[m]; }

// Which way does a move leave the mouse heading?
inline Dir headingAfter(Dir h, Move m) {
  switch (m) {
    case MOVE_LEFT:  return leftOf(h);
    case MOVE_RIGHT: return rightOf(h);
    case MOVE_BACK:  return opposite(h);
    default:         return h;
  }
}
// Which move takes heading h to absolute direction d?
inline Move moveFor(Dir h, Dir d) {
  if (d == h)            return MOVE_FORWARD;
  if (d == leftOf(h))    return MOVE_LEFT;
  if (d == rightOf(h))   return MOVE_RIGHT;
  return MOVE_BACK;
}

// ---------------------------------------------------------------- maze map
struct Maze {
  uint8_t  walls[CELLS];   // bit d set: wall known PRESENT on side d
  uint8_t  known[CELLS];   // bit d set: side d has been observed
  uint16_t dist[CELLS];    // last flood result
  uint8_t  goals[4];
  uint8_t  goalCount;

  void reset() {
    for (uint16_t c = 0; c < CELLS; ++c) { walls[c] = 0; known[c] = 0; dist[c] = INF; }
    // Outer boundary is always a wall.
    for (uint8_t i = 0; i < SIZE; ++i) {
      setWall(cellIndex(i, SIZE - 1), N, true);
      setWall(cellIndex(i, 0),        S, true);
      setWall(cellIndex(SIZE - 1, i), E, true);
      setWall(cellIndex(0, i),        W, true);
    }
    // Start cell: corner, walled on three sides, exit north (UKMARS classic).
    setWall(cellIndex(0, 0), E, true);
    setWall(cellIndex(0, 0), N, false);
    setGoalCentre();
  }

  void setGoalCentre() {
    goalCount = 4;
    goals[0] = cellIndex(7, 7); goals[1] = cellIndex(8, 7);
    goals[2] = cellIndex(7, 8); goals[3] = cellIndex(8, 8);
  }
  void setGoalSingle(uint8_t c) { goalCount = 1; goals[0] = c; }

  bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < SIZE && y < SIZE; }
  bool isGoal(uint8_t c) const {
    for (uint8_t i = 0; i < goalCount; ++i) if (goals[i] == c) return true;
    return false;
  }

  // Record a wall (or its absence) on side d of cell c, and the matching side
  // of the neighbour. Once seen, a side is never downgraded to unknown.
  void setWall(uint8_t c, Dir d, bool present) {
    uint8_t bit = (uint8_t)(1 << d);
    known[c] |= bit;
    if (present) walls[c] |= bit; else walls[c] &= (uint8_t)~bit;
    int nx = cellX(c) + dx(d), ny = cellY(c) + dy(d);
    if (inBounds(nx, ny)) {
      uint8_t n = cellIndex((uint8_t)nx, (uint8_t)ny);
      uint8_t nb = (uint8_t)(1 << opposite(d));
      known[n] |= nb;
      if (present) walls[n] |= nb; else walls[n] &= (uint8_t)~nb;
    }
  }
  bool isKnown(uint8_t c, Dir d) const { return (known[c] >> d) & 1; }
  bool hasWall(uint8_t c, Dir d) const { return (walls[c] >> d) & 1; }

  // Can the mouse move from c in direction d?
  // unknownOpen=true  -> optimistic (search): unseen walls are assumed absent.
  // unknownOpen=false -> pessimistic (speed run): unseen walls are assumed present.
  bool passable(uint8_t c, Dir d, bool unknownOpen) const {
    int nx = cellX(c) + dx(d), ny = cellY(c) + dy(d);
    if (!inBounds(nx, ny)) return false;
    if (isKnown(c, d)) return !hasWall(c, d);
    return unknownOpen;
  }

  // Breadth-first flood from the goal cells. dist[c] = moves to nearest goal.
  // 256 cells, fixed-size ring queue, no allocation: well under 1 ms on an ESP32.
  void flood(bool unknownOpen) {
    for (uint16_t c = 0; c < CELLS; ++c) dist[c] = INF;
    uint8_t queue[CELLS];
    uint16_t head = 0, tail = 0;
    for (uint8_t i = 0; i < goalCount; ++i) { dist[goals[i]] = 0; queue[tail++] = goals[i]; }
    while (head != tail) {
      uint8_t c = queue[head++];
      for (uint8_t d = 0; d < 4; ++d) {
        if (!passable(c, (Dir)d, unknownOpen)) continue;
        uint8_t n = cellIndex((uint8_t)(cellX(c) + dx((Dir)d)), (uint8_t)(cellY(c) + dy((Dir)d)));
        if (dist[n] == INF) { dist[n] = dist[c] + 1; queue[tail++] = n; }
      }
    }
  }

  // After flood(): the neighbour with the lowest distance. Ties go to straight
  // ahead, then a 90° turn, then reversing, so the search run turns less.
  // Returns false if no neighbour is reachable (mouse boxed in / map inconsistent).
  bool bestDir(uint8_t c, Dir heading, bool unknownOpen, Dir& out) const {
    static const uint8_t order[4] = { 0, 3, 1, 2 };  // offsets from heading: F, L, R, B
    uint16_t best = INF; bool found = false;
    for (uint8_t i = 0; i < 4; ++i) {
      Dir d = (Dir)((heading + order[i]) & 3);
      if (!passable(c, d, unknownOpen)) continue;
      uint8_t n = cellIndex((uint8_t)(cellX(c) + dx(d)), (uint8_t)(cellY(c) + dy(d)));
      if (dist[n] < best) { best = dist[n]; out = d; found = true; }
    }
    return found;
  }
};

// ---------------------------------------------------------------- fast path
// A planned route as a list of moves. Also available run-length compressed so
// the motion layer can accelerate over a straight of N cells.
struct Path {
  static const uint16_t MAX = 512;
  Move  moves[MAX];
  uint16_t length;
  void clear() { length = 0; }
  bool push(Move m) { if (length >= MAX) return false; moves[length++] = m; return true; }
};

struct Segment { char kind; uint8_t count; };  // kind: 'S' straight N cells, 'L', 'R', 'B'
struct SegmentPath {
  static const uint16_t MAX = 256;
  Segment segs[MAX];
  uint16_t length;
  void clear() { length = 0; }
};

// L/R/B are "turn then one cell forward", so LFFF = turn, then 4 cells straight.
inline void compressPath(const Path& in, SegmentPath& out) {
  out.clear();
  for (uint16_t i = 0; i < in.length; ++i) {
    Move m = in.moves[i];
    if (m == MOVE_FORWARD && out.length > 0 && out.segs[out.length - 1].kind == 'S' && out.segs[out.length - 1].count < 255) {
      out.segs[out.length - 1].count++;
      continue;
    }
    if (out.length >= SegmentPath::MAX) return;
    if (m == MOVE_FORWARD) { out.segs[out.length++] = Segment{ 'S', 1 }; }
    else {
      // A turn move is a turn followed by one straight cell.
      out.segs[out.length++] = Segment{ moveChar(m), 0 };
      if (out.length < SegmentPath::MAX) out.segs[out.length++] = Segment{ 'S', 1 };
    }
  }
}

// Cost-weighted planner: Dijkstra over (cell, heading) = 1024 states.
// A straight cell costs STRAIGHT; a 90° turn adds TURN90; a 180° adds TURN180.
// Uses the PESSIMISTIC map (unknown = wall) so the route only crosses walls
// the mouse has actually seen open. Set TURN90 from measurement: time ten
// straight cells against ten cells with a turn in each.
struct Planner {
  uint16_t STRAIGHT = 10;   // integer tenths: 1.0
  uint16_t TURN90   = 35;   // 3.5
  uint16_t TURN180  = 60;   // 6.0

  // unknownOpen=false: only cross sides seen open (the real speed run).
  // unknownOpen=true:  optimistic route, used to aim exploration and to test
  //                    whether anything unexplored could still beat the known route.
  // Returns false if no route exists. totalCost receives the route cost.
  bool plan(const Maze& maze, uint8_t startCell, Dir startHeading, Path& out, bool unknownOpen = false, uint16_t* totalCost = 0) const {
    static const uint16_t STATES = CELLS * 4;
    // Static so ~5 KB does not land on the (small) ESP32 task stack. Not reentrant.
    static uint16_t cost[STATES];
    static uint16_t prev[STATES];
    static bool     done[STATES];
    static Path     rev;
    for (uint16_t s = 0; s < STATES; ++s) { cost[s] = INF; prev[s] = INF; done[s] = false; }
    uint16_t s0 = (uint16_t)(startCell * 4 + startHeading);
    cost[s0] = 0;

    // O(V^2) selection is fine for 1024 states and keeps this allocation-free.
    for (;;) {
      uint16_t u = INF, best = INF;
      for (uint16_t s = 0; s < STATES; ++s) if (!done[s] && cost[s] < best) { best = cost[s]; u = s; }
      if (u == INF) break;
      done[u] = true;
      uint8_t c = (uint8_t)(u / 4); Dir h = (Dir)(u % 4);
      if (maze.isGoal(c)) {
        if (totalCost) *totalCost = cost[u];
        // Reconstruct.
        rev.clear();
        uint16_t s = u;
        while (s != s0) {
          uint16_t p = prev[s];
          Dir ph = (Dir)(p % 4), sh = (Dir)(s % 4);
          rev.push(moveFor(ph, sh));
          s = p;
        }
        out.clear();
        for (uint16_t i = rev.length; i > 0; --i) out.push(rev.moves[i - 1]);
        return true;
      }
      for (uint8_t d = 0; d < 4; ++d) {
        Dir nd = (Dir)d;
        if (!maze.passable(c, nd, unknownOpen)) continue;
        uint16_t step = STRAIGHT;
        if (nd == leftOf(h) || nd == rightOf(h)) step += TURN90;
        else if (nd == opposite(h))              step += TURN180;
        uint8_t n = cellIndex((uint8_t)(cellX(c) + dx(nd)), (uint8_t)(cellY(c) + dy(nd)));
        uint16_t v = (uint16_t)(n * 4 + nd);
        if (cost[u] + step < cost[v]) { cost[v] = cost[u] + step; prev[v] = u; }
      }
    }
    return false;
  }
};

// ---------------------------------------------------------------- controller
// Drives the whole slot: search to centre, return to start, decide whether the
// map is good enough, and then run the planned fast route.
enum Phase : uint8_t { PHASE_SEARCH_OUT, PHASE_SEARCH_BACK, PHASE_SPEED_RUN, PHASE_DONE };

struct Mouse {
  Maze    maze;
  Planner planner;
  uint8_t cell;
  Dir     heading;
  Phase   phase;
  uint8_t exploreLoops;      // completed out-and-back searches
  uint8_t maxExploreLoops;   // give up refining after this many (budget the clock)
  Path    fastPath;
  uint16_t pathIdx;
  bool    speedRunReady;     // planned route exists and is proven optimal-enough

  // Full reset: a fresh maze. Call once at power-up, never between runs
  // (the map is the whole point of the earlier runs).
  void begin() {
    maze.reset();
    maxExploreLoops = 3;
    speedRunReady = false;
    startRun();
    exploreLoops = 0;
    phase = PHASE_SEARCH_OUT;
    maze.setGoalCentre();
  }

  // Start of a timed run: mouse placed in the start cell facing north.
  // Keeps the map. Picks the phase from what we already know.
  // The fast route is re-planned from the latest map every time, so a wall
  // first seen during an earlier speed run is routed around rather than run
  // into again. If the known map no longer holds a route, search again.
  void startRun() {
    cell = cellIndex(0, 0);
    heading = N;
    pathIdx = 0;
    maze.setGoalCentre();
    if (speedRunReady) planSpeedRun();
    phase = speedRunReady ? PHASE_SPEED_RUN : PHASE_SEARCH_OUT;
  }

  // Rules: a judge-requested recovery erases the mouse's memory of the maze.
  void forgetMaze() { begin(); }

  // Feed relative wall readings taken at the centre of the current cell.
  void observe(bool left, bool front, bool right) {
    maze.setWall(cell, leftOf(heading),  left);
    maze.setWall(cell, heading,          front);
    maze.setWall(cell, rightOf(heading), right);
  }

  // True when the best turn-weighted route with unknown walls treated as open
  // costs the same as with them treated as walls: nothing unexplored can beat
  // what we already know, so exploring further only burns clock.
  bool routeProven() {
    static Path scratch;
    maze.setGoalCentre();
    uint16_t optimistic = INF, pessimistic = INF;
    if (!planner.plan(maze, cellIndex(0, 0), N, scratch, true,  &optimistic))  return false;
    if (!planner.plan(maze, cellIndex(0, 0), N, scratch, false, &pessimistic)) return false;
    return optimistic == pessimistic;
  }

  // Search-out step: follow the optimistic turn-weighted route from here, so
  // exploration is spent on the cells that could actually shorten the fast run.
  // Falls back to the plain flood if the planner finds nothing.
  Move searchMoveTowardsGoal() {
    static Path scratch;
    if (planner.plan(maze, cell, heading, scratch, true) && scratch.length > 0) return scratch.moves[0];
    maze.flood(true);
    Dir d;
    if (!maze.bestDir(cell, heading, true, d)) return MOVE_STOP;
    return moveFor(heading, d);
  }

  // Attempt to plan the fast route from the start cell over known-safe cells.
  bool planSpeedRun() {
    maze.setGoalCentre();
    speedRunReady = planner.plan(maze, cellIndex(0, 0), N, fastPath);
    return speedRunReady;
  }

  // Decide the next move. Call observe() first.
  Move next() {
    switch (phase) {
      case PHASE_SEARCH_OUT: {
        if (maze.isGoal(cell)) {
          phase = PHASE_SEARCH_BACK;
          maze.setGoalSingle(cellIndex(0, 0));
          return next();
        }
        maze.setGoalCentre();
        return searchMoveTowardsGoal();
      }
      case PHASE_SEARCH_BACK: {
        if (cell == cellIndex(0, 0)) {
          exploreLoops++;
          if (routeProven() || exploreLoops >= maxExploreLoops) {
            planSpeedRun();
            phase = speedRunReady ? PHASE_SPEED_RUN : PHASE_SEARCH_OUT;
          } else {
            phase = PHASE_SEARCH_OUT;
            maze.setGoalCentre();
          }
          return MOVE_STOP;   // run over; wait for startRun()
        }
        maze.flood(true);
        Dir d;
        if (!maze.bestDir(cell, heading, true, d)) return MOVE_STOP;
        return moveFor(heading, d);
      }
      case PHASE_SPEED_RUN: {
        if (maze.isGoal(cell)) { phase = PHASE_DONE; return MOVE_STOP; }
        if (pathIdx >= fastPath.length) return MOVE_STOP;
        return fastPath.moves[pathIdx++];
      }
      default: return MOVE_STOP;
    }
  }

  // Call after the motion layer has finished the move.
  void applyMove(Move m) {
    if (m == MOVE_STOP) return;
    heading = headingAfter(heading, m);
    cell = cellIndex((uint8_t)(cellX(cell) + dx(heading)), (uint8_t)(cellY(cell) + dy(heading)));
  }

  // Safety check the motion layer can use before executing a move: is the
  // side we are about to drive through known open?
  bool moveIsSafe(Move m) const {
    if (m == MOVE_STOP) return true;
    Dir d = headingAfter(heading, m);
    return maze.isKnown(cell, d) && !maze.hasWall(cell, d);
  }
};

} // namespace mm
