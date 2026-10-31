#include "maze_levels.h"

#include "maze_game.h"

#include <string.h>

#define M_PATH MAZE_CELL_PATH
#define M_WALL MAZE_CELL_WALL

static const uint8_t first_level_cells[] =
{
    M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL,
    M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL,

    M_WALL, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH,
    M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_WALL,

    M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL,
    M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_PATH, M_WALL,

    M_WALL, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH,
    M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_WALL,

    M_WALL, M_PATH, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL,
    M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL,

    M_WALL, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH,
    M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_WALL,

    M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL,
    M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_PATH, M_WALL,

    M_WALL, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH,
    M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_WALL,

    M_WALL, M_PATH, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL,
    M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL,

    M_WALL, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH,
    M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_WALL,

    M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL,
    M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_PATH, M_WALL,

    M_WALL, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH,
    M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_WALL,

    M_WALL, M_PATH, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL,
    M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL,

    M_WALL, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH,
    M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_PATH, M_WALL,

    M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL,
    M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL,

    M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL,
    M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL, M_WALL
};

static const maze_level_t first_level =
{
    1u,
    MAZE_MAX_COLS,
    MAZE_MAX_ROWS,
    (uint16_t)(sizeof(first_level_cells) / sizeof(first_level_cells[0])),
    first_level_cells,
    1u, 1u,
    14u, 13u
};

const maze_level_t *maze_levels_default(void)
{
    return &first_level;
}

bool maze_levels_reachable(const maze_level_t *level)
{
    static const int8_t row_delta[4] = {-1, 1, 0, 0};
    static const int8_t col_delta[4] = {0, 0, -1, 1};
    uint8_t visited[MAZE_MAX_COLS * MAZE_MAX_ROWS] = {0};
    uint16_t queue[MAZE_MAX_COLS * MAZE_MAX_ROWS];
    uint16_t head = 0;
    uint16_t tail = 0;
    uint16_t start;
    uint16_t goal;
    unsigned direction;

    if (maze_game_level_check(level) != MAZE_OK)
        return false;

    start = (uint16_t)(level->start_row * level->cols + level->start_col);
    goal = (uint16_t)(level->goal_row * level->cols + level->goal_col);
    visited[start] = 1;
    queue[tail++] = start;

    while (head < tail)
    {
        uint16_t current = queue[head++];
        int row;
        int col;

        if (current == goal)
            return true;

        row = current / level->cols;
        col = current % level->cols;
        for (direction = 0; direction < 4; ++direction)
        {
            int next_row = row + row_delta[direction];
            int next_col = col + col_delta[direction];
            uint16_t next;

            if (next_row < 0 || next_row >= level->rows ||
                next_col < 0 || next_col >= level->cols)
                continue;

            next = (uint16_t)(next_row * level->cols + next_col);
            if (!visited[next] &&
                level->cells[next] == MAZE_CELL_PATH)
            {
                visited[next] = 1;
                queue[tail++] = next;
            }
        }
    }

    return false;
}
