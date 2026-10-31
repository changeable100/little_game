#include "maze_levels_selftest.h"

#include "maze_game.h"
#include "maze_levels.h"

static bool test_default_level(void)
{
    const maze_level_t *level = maze_levels_default();

    return level &&
           level->id == 1u &&
           level->cols == 16u &&
           level->rows == 16u &&
           level->cell_count == 256u &&
           level->start_col == 1u &&
           level->start_row == 1u &&
           level->goal_col == 14u &&
           level->goal_row == 13u &&
           maze_game_level_check(level) == MAZE_OK &&
           maze_levels_reachable(level);
}

static bool test_disconnected_level(void)
{
    static const uint8_t cells[9] =
    {
        MAZE_CELL_PATH, MAZE_CELL_WALL, MAZE_CELL_PATH,
        MAZE_CELL_WALL, MAZE_CELL_WALL, MAZE_CELL_WALL,
        MAZE_CELL_PATH, MAZE_CELL_WALL, MAZE_CELL_PATH
    };
    const maze_level_t level = {2u, 3u, 3u, 9u, cells, 0u, 0u, 2u, 2u};

    return maze_game_level_check(&level) == MAZE_OK &&
           !maze_levels_reachable(&level);
}

static bool test_diagonal_is_not_connected(void)
{
    static const uint8_t cells[9] =
    {
        MAZE_CELL_PATH, MAZE_CELL_WALL, MAZE_CELL_WALL,
        MAZE_CELL_WALL, MAZE_CELL_PATH, MAZE_CELL_WALL,
        MAZE_CELL_WALL, MAZE_CELL_WALL, MAZE_CELL_WALL
    };
    const maze_level_t level = {3u, 3u, 3u, 9u, cells, 0u, 0u, 1u, 1u};

    return maze_game_level_check(&level) == MAZE_OK &&
           !maze_levels_reachable(&level);
}

static bool test_malformed_levels(void)
{
    static const uint8_t invalid_cells[9] =
    {
        MAZE_CELL_PATH, MAZE_CELL_PATH, MAZE_CELL_PATH,
        MAZE_CELL_PATH, 2u, MAZE_CELL_PATH,
        MAZE_CELL_PATH, MAZE_CELL_PATH, MAZE_CELL_PATH
    };
    static const uint8_t valid_cells[9] = {0};
    maze_level_t level = {4u, 3u, 3u, 8u, valid_cells, 0u, 0u, 2u, 2u};

    if (maze_game_level_check(&level) != MAZE_INVALID ||
        maze_levels_reachable(&level))
        return false;

    level.cell_count = 9u;
    level.goal_col = 3u;
    if (maze_game_level_check(&level) != MAZE_INVALID ||
        maze_levels_reachable(&level))
        return false;

    level.goal_col = 2u;
    level.cells = invalid_cells;
    return maze_game_level_check(&level) == MAZE_INVALID &&
           !maze_levels_reachable(&level);
}

int maze_levels_selftest_run(maze_level_test_report_t report)
{
    int failures = 0;
    bool passed;

#define CHECK(name, expression) do { \
    passed = (expression); \
    failures += passed ? 0 : 1; \
    if (report) report(name, passed); \
} while (0)
    CHECK("TC-201 fixed-map-structure-and-reachability", test_default_level());
    CHECK("TC-202 disconnected-map", test_disconnected_level());
    CHECK("TC-202 diagonal-only-map", test_diagonal_is_not_connected());
    CHECK("TC-202 malformed-maps", test_malformed_levels());
#undef CHECK

    return failures;
}
