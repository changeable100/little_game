#include "maze_selftest.h"
#include "maze_game.h"

#include <math.h>
#include <string.h>

/* Test-only maps: B owns the future formal maze_levels module. */
static const uint8_t open_cells[49] = {0};
static const maze_level_t open_level =
{
    9001, 7, 7, 49, open_cells, 3, 3, 6, 6
};
static const uint8_t wall_cells[49] =
{
    0, 0, 0, 0, 1, 0, 0,
    0, 0, 0, 0, 1, 0, 0,
    0, 0, 0, 0, 1, 0, 0,
    0, 0, 0, 0, 1, 0, 0,
    0, 0, 0, 0, 1, 0, 0,
    0, 0, 0, 0, 1, 0, 0,
    0, 0, 0, 0, 1, 0, 0
};
static const maze_level_t wall_level =
{
    9002, 7, 7, 49, wall_cells, 3, 3, 0, 0
};
static const uint8_t corridor_cells[15] = {0};
static const maze_level_t corridor_level =
{
    9003, 5, 3, 15, corridor_cells, 0, 1, 4, 1
};

static maze_result_t action(maze_game_t *game, maze_action_kind_t kind,
                            uint32_t request)
{
    maze_action_t a = {kind, game->snapshot.run_id, request};
    return maze_game_action(game, &a);
}

static bool running(maze_game_t *game, const maze_level_t *level)
{
    return maze_game_init(game, level) == MAZE_OK &&
           action(game, MAZE_ACTION_START, 1) == MAZE_OK &&
           maze_game_calibrated(game, game->snapshot.run_id) == MAZE_OK;
}

static bool close_to(float a, float b)
{
    return fabsf(a - b) < 0.00001f;
}

static bool test_start(void)
{
    maze_game_t g;
    maze_input_t input = {1.0f, 0.0f, true};
    uint32_t initial;
    if (maze_game_init(&g, &open_level) != MAZE_OK ||
        g.snapshot.state != MAZE_READY)
        return false;
    initial = g.snapshot.run_id;
    if (action(&g, MAZE_ACTION_START, 1) != MAZE_OK ||
        g.snapshot.run_id == initial || g.snapshot.state != MAZE_CALIBRATING)
        return false;
    if (maze_game_step(&g, &input, 20) != MAZE_OK ||
        !close_to(g.snapshot.ball_x, 3.5f) || g.snapshot.elapsed_ms != 0 ||
        maze_game_calibrated(&g, initial) != MAZE_STALE)
        return false;
    return maze_game_calibrated(&g, g.snapshot.run_id) == MAZE_OK &&
           g.snapshot.state == MAZE_RUNNING;
}

static bool test_direction(float x, float y)
{
    maze_game_t g;
    maze_input_t input = {x, y, true};
    int i;
    if (!running(&g, &open_level))
        return false;
    for (i = 0; i < 20; ++i)
        if (maze_game_step(&g, &input, 20) != MAZE_OK)
            return false;
    return (x == 0 ? close_to(g.snapshot.ball_x, 3.5f) :
            (g.snapshot.ball_x - 3.5f) * x > 0) &&
           (y == 0 ? close_to(g.snapshot.ball_y, 3.5f) :
            (g.snapshot.ball_y - 3.5f) * y > 0) &&
           fabsf(g.velocity_x) <= MAZE_MAX_SPEED &&
           fabsf(g.velocity_y) <= MAZE_MAX_SPEED &&
           g.snapshot.elapsed_ms == 400;
}

static bool test_wall(void)
{
    maze_game_t g;
    maze_input_t input = {1.0f, 0.0f, true};
    int i;
    if (!running(&g, &wall_level))
        return false;
    for (i = 0; i < 100; ++i)
        if (maze_game_step(&g, &input, 100) != MAZE_OK ||
            g.snapshot.ball_x + MAZE_BALL_RADIUS > 4.00001f)
            return false;
    return g.snapshot.ball_x > 3.7f && close_to(g.snapshot.ball_y, 3.5f);
}

static bool test_edges(float x, float y)
{
    maze_game_t g;
    maze_input_t input = {x, y, true};
    int i;
    if (!running(&g, &open_level))
        return false;
    for (i = 0; i < 80; ++i)
    {
        if (maze_game_step(&g, &input, 100) != MAZE_OK ||
            g.snapshot.ball_x < MAZE_BALL_RADIUS ||
            g.snapshot.ball_x > 7.0f - MAZE_BALL_RADIUS ||
            g.snapshot.ball_y < MAZE_BALL_RADIUS ||
            g.snapshot.ball_y > 7.0f - MAZE_BALL_RADIUS)
            return false;
    }
    return true;
}

static bool test_horizontal_corner(bool corner)
{
    maze_game_t g;
    uint8_t cells[49] = {0};
    maze_level_t level = {9004, 7, 7, 49, cells, 3, 3, 0, 0};
    maze_input_t input = {corner ? 1.0f : 0.0f, 1.0f, true};
    int i;
    for (i = 0; i < 7; ++i)
    {
        cells[4 * 7 + i] = MAZE_CELL_WALL;
        if (corner) cells[i * 7 + 4] = MAZE_CELL_WALL;
    }
    if (!running(&g, &level))
        return false;
    for (i = 0; i < 100; ++i)
        if (maze_game_step(&g, &input, 100) != MAZE_OK ||
            g.snapshot.ball_y + MAZE_BALL_RADIUS > 4.00001f ||
            (corner && g.snapshot.ball_x + MAZE_BALL_RADIUS > 4.00001f))
            return false;
    return g.snapshot.ball_y > 3.7f &&
           (corner ? g.snapshot.ball_x > 3.7f :
                     close_to(g.snapshot.ball_x, 3.5f));
}

static bool test_slide(void)
{
    maze_game_t g;
    maze_input_t input = {1.0f, 1.0f, true};
    int i;
    if (!running(&g, &wall_level))
        return false;
    for (i = 0; i < 15; ++i)
        if (maze_game_step(&g, &input, 100) != MAZE_OK ||
            g.snapshot.ball_x + MAZE_BALL_RADIUS > 4.00001f)
            return false;
    return g.snapshot.ball_y > 4.5f && g.snapshot.ball_x > 3.7f;
}

static bool test_api_guards(void)
{
    maze_game_t g;
    maze_snapshot_t before;
    maze_input_t input = {0.0f, 0.0f, true};
    maze_action_t a = {MAZE_ACTION_PAUSE, 1, 0};
    if (!running(&g, &open_level))
        return false;
    before = g.snapshot;
    if (maze_game_action(&g, NULL) != MAZE_INVALID ||
        maze_game_action(&g, &a) != MAZE_INVALID ||
        maze_game_snapshot(NULL, &before) != MAZE_INVALID ||
        maze_game_snapshot(&g, NULL) != MAZE_INVALID ||
        maze_game_step(NULL, &input, 20) != MAZE_INVALID ||
        maze_game_calibrated(NULL, 0) != MAZE_INVALID ||
        action(&g, MAZE_ACTION_START, 2) != MAZE_WRONG_STATE ||
        maze_game_step(&g, &input, 0) != MAZE_OK ||
        memcmp(&before, &g.snapshot, sizeof(before)) != 0)
        return false;
    a.action = (maze_action_kind_t)99;
    a.request_id = 2;
    return maze_game_action(&g, &a) == MAZE_INVALID &&
           maze_game_step(&g, NULL, 20) == MAZE_INVALID &&
           g.snapshot.error_code == MAZE_ERROR_INPUT &&
           action(&g, MAZE_ACTION_RESTART, 2) == MAZE_OK &&
           g.snapshot.state == MAZE_CALIBRATING;
}

static bool test_pause(void)
{
    maze_game_t g;
    maze_input_t input = {1.0f, 0.0f, true};
    maze_snapshot_t before;
    if (!running(&g, &open_level) ||
        maze_game_step(&g, &input, 20) != MAZE_OK ||
        action(&g, MAZE_ACTION_PAUSE, 2) != MAZE_OK)
        return false;
    before = g.snapshot;
    if (maze_game_step(&g, &input, 100) != MAZE_OK ||
        memcmp(&before, &g.snapshot, sizeof(before)) != 0 ||
        action(&g, MAZE_ACTION_PAUSE, 3) != MAZE_WRONG_STATE ||
        action(&g, MAZE_ACTION_RESUME, 3) != MAZE_OK ||
        maze_game_step(&g, &input, 20) != MAZE_OK)
        return false;
    return g.snapshot.elapsed_ms == before.elapsed_ms + 20 &&
           g.snapshot.ball_x > before.ball_x;
}

static bool test_restart_stale(void)
{
    maze_game_t g;
    maze_action_t old;
    maze_snapshot_t s;
    if (!running(&g, &open_level))
        return false;
    old.action = MAZE_ACTION_PAUSE;
    old.expected_run_id = g.snapshot.run_id;
    old.request_id = 3;
    if (action(&g, MAZE_ACTION_RESTART, 2) != MAZE_OK ||
        maze_game_action(&g, &old) != MAZE_STALE ||
        action(&g, MAZE_ACTION_RESTART, 2) != MAZE_STALE ||
        g.snapshot.elapsed_ms != 0 || !close_to(g.snapshot.ball_x, 3.5f))
        return false;
    if (maze_game_snapshot(&g, &s) != MAZE_OK ||
        action(&g, MAZE_ACTION_BACK, 4) != MAZE_OK)
        return false;
    return g.snapshot.state == MAZE_READY &&
           maze_game_calibrated(&g, s.run_id) == MAZE_STALE &&
           s.state == MAZE_CALIBRATING;
}

static bool test_win(void)
{
    maze_game_t g;
    maze_input_t input = {1.0f, 0.0f, true};
    maze_snapshot_t before;
    int i;
    if (!running(&g, &corridor_level))
        return false;
    for (i = 0; i < 200 && g.snapshot.state == MAZE_RUNNING; ++i)
        if (maze_game_step(&g, &input, 20) != MAZE_OK)
            return false;
    if (g.snapshot.state != MAZE_WON || g.snapshot.ball_x < 4.0f)
        return false;
    before = g.snapshot;
    return maze_game_step(&g, &input, 100) == MAZE_OK &&
           memcmp(&before, &g.snapshot, sizeof(before)) == 0 &&
           action(&g, MAZE_ACTION_RESTART, 2) == MAZE_OK &&
           g.snapshot.state == MAZE_CALIBRATING && g.snapshot.elapsed_ms == 0;
}

static bool test_bad_maps(void)
{
    maze_game_t g;
    maze_level_t level = open_level;
    uint8_t invalid_cells[49] = {0};
    if (maze_game_level_check(NULL) != MAZE_INVALID ||
        maze_game_init(NULL, &level) != MAZE_INVALID)
        return false;
    level.cell_count = 48;
    if (maze_game_init(&g, &level) != MAZE_INVALID ||
        g.snapshot.state != MAZE_ERROR)
        return false;
    level = open_level;
    level.cols = 17;
    if (maze_game_level_check(&level) != MAZE_INVALID)
        return false;
    level = open_level;
    level.goal_col = 3;
    level.goal_row = 3;
    if (maze_game_level_check(&level) != MAZE_INVALID)
        return false;
    level = open_level;
    level.start_col = 7;
    if (maze_game_level_check(&level) != MAZE_INVALID)
        return false;
    level = open_level;
    level.cells = NULL;
    if (maze_game_level_check(&level) != MAZE_INVALID)
        return false;
    level.cells = invalid_cells;
    invalid_cells[0] = 2;
    if (maze_game_level_check(&level) != MAZE_INVALID)
        return false;
    invalid_cells[0] = 0;
    invalid_cells[24] = MAZE_CELL_WALL;
    return maze_game_level_check(&level) == MAZE_INVALID;
}

static bool test_bad_input(float x, bool valid, uint32_t dt)
{
    maze_game_t g;
    maze_input_t input = {x, 0.0f, valid};
    if (!running(&g, &open_level))
        return false;
    return maze_game_step(&g, &input, dt) == MAZE_INVALID &&
           g.snapshot.state == MAZE_ERROR &&
           close_to(g.snapshot.ball_x, 3.5f) &&
           g.snapshot.elapsed_ms == 0 && close_to(g.velocity_x, 0.0f);
}

static bool test_counters(void)
{
    maze_game_t g;
    maze_input_t input = {0.0f, 0.0f, true};
    if (!running(&g, &open_level))
        return false;
    g.snapshot.elapsed_ms = UINT32_MAX - 2;
    if (maze_game_step(&g, &input, 20) != MAZE_OK ||
        g.snapshot.elapsed_ms != UINT32_MAX)
        return false;
    g.snapshot.run_id = UINT32_MAX;
    if (action(&g, MAZE_ACTION_RESTART, 2) != MAZE_EXHAUSTED)
        return false;
    g.snapshot.run_id = 1;
    if (action(&g, MAZE_ACTION_PAUSE, UINT32_MAX) != MAZE_OK ||
        action(&g, MAZE_ACTION_RESUME, 1) != MAZE_EXHAUSTED)
        return false;
    if (!running(&g, &open_level))
        return false;
    g.snapshot.revision = UINT32_MAX;
    return maze_game_step(&g, &input, 20) == MAZE_EXHAUSTED &&
           g.snapshot.revision == UINT32_MAX && g.snapshot.elapsed_ms == 0;
}

int maze_selftest_run(maze_test_report_t report)
{
    int failures = 0;
#define CHECK(name, expression) do { \
    bool passed = (expression); \
    failures += passed ? 0 : 1; \
    if (report) report(name, passed); \
} while (0)
    CHECK("TC-301 start-calibrate", test_start());
    CHECK("TC-302 right", test_direction(1.0f, 0.0f));
    CHECK("TC-302 left", test_direction(-1.0f, 0.0f));
    CHECK("TC-302 down", test_direction(0.0f, 1.0f));
    CHECK("TC-302 up", test_direction(0.0f, -1.0f));
    CHECK("TC-303 thin-wall", test_wall());
    CHECK("TC-303 horizontal-wall", test_horizontal_corner(false));
    CHECK("TC-303 corner", test_horizontal_corner(true));
    CHECK("TC-303 slide", test_slide());
    CHECK("TC-303 right-edge", test_edges(1.0f, 0.0f));
    CHECK("TC-303 left-edge", test_edges(-1.0f, 0.0f));
    CHECK("TC-303 bottom-edge", test_edges(0.0f, 1.0f));
    CHECK("TC-303 top-edge", test_edges(0.0f, -1.0f));
    CHECK("TC-304 pause-resume", test_pause());
    CHECK("TC-304 restart-stale-back", test_restart_stale());
    CHECK("TC-305 win-freeze", test_win());
    CHECK("TC-306 bad-maps", test_bad_maps());
    CHECK("TC-306 invalid-sample", test_bad_input(0.0f, false, 20));
    CHECK("TC-306 nan", test_bad_input(NAN, true, 20));
    CHECK("TC-306 infinity", test_bad_input(INFINITY, true, 20));
    CHECK("TC-306 range", test_bad_input(1.1f, true, 20));
    CHECK("TC-306 long-dt", test_bad_input(0.0f, true, 101));
    CHECK("TC-306 counters", test_counters());
    CHECK("TC-306 api-guards", test_api_guards());
#undef CHECK
    return failures;
}
