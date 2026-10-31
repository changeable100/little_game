#include "maze_game.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define MAZE_ACCELERATION 12.0f
#define MAZE_DRAG 4.0f
#define MAZE_SUBSTEP_MS 5u

static float clamp(float value, float low, float high)
{
    return value < low ? low : (value > high ? high : value);
}

maze_result_t maze_game_level_check(const maze_level_t *level)
{
    uint16_t i;
    if (!level || !level->cells || level->cols == 0 || level->rows == 0 ||
        level->cols > MAZE_MAX_COLS || level->rows > MAZE_MAX_ROWS ||
        level->cell_count != (uint16_t)(level->cols * level->rows) ||
        level->start_col >= level->cols || level->start_row >= level->rows ||
        level->goal_col >= level->cols || level->goal_row >= level->rows ||
        (level->start_col == level->goal_col &&
         level->start_row == level->goal_row))
        return MAZE_INVALID;
    for (i = 0; i < level->cell_count; ++i)
        if (level->cells[i] != MAZE_CELL_PATH &&
            level->cells[i] != MAZE_CELL_WALL)
            return MAZE_INVALID;
    if (level->cells[level->start_row * level->cols + level->start_col] !=
        MAZE_CELL_PATH ||
        level->cells[level->goal_row * level->cols + level->goal_col] !=
        MAZE_CELL_PATH)
        return MAZE_INVALID;
    return MAZE_OK;
}

static void reset_ball(maze_game_t *game)
{
    game->snapshot.ball_x = game->level->start_col + 0.5f;
    game->snapshot.ball_y = game->level->start_row + 0.5f;
    game->snapshot.elapsed_ms = 0;
    game->snapshot.error_code = MAZE_ERROR_NONE;
    game->snapshot.input_status = MAZE_INPUT_NOT_READY;
    game->velocity_x = 0;
    game->velocity_y = 0;
}

maze_result_t maze_game_init(maze_game_t *game, const maze_level_t *level)
{
    if (!game)
        return MAZE_INVALID;
    memset(game, 0, sizeof(*game));
    game->snapshot.ball_radius = MAZE_BALL_RADIUS;
    game->snapshot.revision = 1;
    if (maze_game_level_check(level) != MAZE_OK)
    {
        game->snapshot.state = MAZE_ERROR;
        game->snapshot.error_code = MAZE_ERROR_LEVEL;
        return MAZE_INVALID;
    }
    game->level = level;
    game->snapshot.level_id = level->id;
    game->snapshot.state = MAZE_READY;
    reset_ball(game);
    return MAZE_OK;
}

maze_result_t maze_game_action(maze_game_t *game, const maze_action_t *action)
{
    maze_state_t next;
    bool new_run = false;
    if (!game || !action || !game->level || action->request_id == 0)
        return MAZE_INVALID;
    if (action->expected_run_id != game->snapshot.run_id)
        return MAZE_STALE;
    if (game->last_request_id == UINT32_MAX ||
        game->snapshot.revision == UINT32_MAX)
        return MAZE_EXHAUSTED;
    if (action->request_id <= game->last_request_id)
        return MAZE_STALE;

    switch (action->action)
    {
    case MAZE_ACTION_START:
        if (game->snapshot.state != MAZE_READY)
            return MAZE_WRONG_STATE;
        next = MAZE_CALIBRATING;
        new_run = true;
        break;
    case MAZE_ACTION_RESTART:
        next = MAZE_CALIBRATING;
        new_run = true;
        break;
    case MAZE_ACTION_BACK:
        next = MAZE_READY;
        new_run = true; /* Invalidates late calibration and input callbacks. */
        break;
    case MAZE_ACTION_PAUSE:
        if (game->snapshot.state != MAZE_RUNNING)
            return MAZE_WRONG_STATE;
        next = MAZE_PAUSED;
        break;
    case MAZE_ACTION_RESUME:
        if (game->snapshot.state != MAZE_PAUSED)
            return MAZE_WRONG_STATE;
        next = MAZE_RUNNING;
        break;
    default:
        return MAZE_INVALID;
    }
    if (new_run && game->snapshot.run_id == UINT32_MAX)
        return MAZE_EXHAUSTED;
    if (new_run)
    {
        ++game->snapshot.run_id;
        reset_ball(game);
    }
    if (next == MAZE_PAUSED)
    {
        game->velocity_x = 0;
        game->velocity_y = 0;
    }
    game->last_request_id = action->request_id;
    game->snapshot.state = next;
    ++game->snapshot.revision;
    return MAZE_OK;
}

maze_result_t maze_game_calibrated(maze_game_t *game, uint32_t run_id)
{
    if (!game || !game->level)
        return MAZE_INVALID;
    if (run_id != game->snapshot.run_id)
        return MAZE_STALE;
    if (game->snapshot.state != MAZE_CALIBRATING)
        return MAZE_WRONG_STATE;
    if (game->snapshot.revision == UINT32_MAX)
        return MAZE_EXHAUSTED;
    game->snapshot.state = MAZE_RUNNING;
    game->snapshot.input_status = MAZE_INPUT_VALID;
    ++game->snapshot.revision;
    return MAZE_OK;
}

static maze_result_t fail(maze_game_t *game, maze_error_t error)
{
    if (game->snapshot.revision == UINT32_MAX)
        return MAZE_EXHAUSTED;
    game->snapshot.state = MAZE_ERROR;
    game->snapshot.error_code = error;
    if (error == MAZE_ERROR_INPUT)
        game->snapshot.input_status = MAZE_INPUT_FAILED;
    game->velocity_x = 0;
    game->velocity_y = 0;
    ++game->snapshot.revision;
    return MAZE_INVALID;
}

/* The candidate is at most 0.015 cell away on either axis. Only overlapped
 * cells are inspected; outside the map is an impassable boundary. */
static bool blocked(const maze_level_t *level, float x, float y)
{
    const float radius = MAZE_BALL_RADIUS;
    int row, col;
    int left, right, top, bottom;
    if (x < radius || y < radius ||
        x > level->cols - radius || y > level->rows - radius)
        return true;
    left = (int)floorf(x - radius);
    right = (int)floorf(x + radius);
    top = (int)floorf(y - radius);
    bottom = (int)floorf(y + radius);
    /* Tangency at the outside edge may round to cols/rows. */
    if (right >= level->cols) right = level->cols - 1;
    if (bottom >= level->rows) bottom = level->rows - 1;
    for (row = top; row <= bottom; ++row)
    {
        for (col = left; col <= right; ++col)
        {
            float dx, dy;
            if (level->cells[row * level->cols + col] != MAZE_CELL_WALL)
                continue;
            dx = x - clamp(x, (float)col, col + 1.0f);
            dy = y - clamp(y, (float)row, row + 1.0f);
            if (dx * dx + dy * dy < radius * radius)
                return true;
        }
    }
    return false;
}

maze_result_t maze_game_step(maze_game_t *game, const maze_input_t *input,
                             uint32_t dt_ms)
{
    uint32_t remaining;
    if (!game || !game->level)
        return MAZE_INVALID;
    if (game->snapshot.state != MAZE_RUNNING)
        return MAZE_OK;
    if (game->snapshot.revision == UINT32_MAX)
        return MAZE_EXHAUSTED;
    if (!input || !input->valid || !isfinite(input->tilt_x) ||
        !isfinite(input->tilt_y) || fabsf(input->tilt_x) > 1.0f ||
        fabsf(input->tilt_y) > 1.0f)
        return fail(game, MAZE_ERROR_INPUT);
    if (dt_ms > MAZE_MAX_DT_MS)
        return fail(game, MAZE_ERROR_TIMING);
    if (dt_ms == 0)
        return MAZE_OK;

    remaining = dt_ms;
    while (remaining > 0)
    {
        uint32_t step = remaining > MAZE_SUBSTEP_MS ?
                        MAZE_SUBSTEP_MS : remaining;
        float seconds = step * 0.001f;
        float next;
        game->velocity_x = clamp(game->velocity_x + seconds *
            (MAZE_ACCELERATION * input->tilt_x - MAZE_DRAG * game->velocity_x),
            -MAZE_MAX_SPEED, MAZE_MAX_SPEED);
        game->velocity_y = clamp(game->velocity_y + seconds *
            (MAZE_ACCELERATION * input->tilt_y - MAZE_DRAG * game->velocity_y),
            -MAZE_MAX_SPEED, MAZE_MAX_SPEED);
        next = game->snapshot.ball_x + game->velocity_x * seconds;
        if (blocked(game->level, next, game->snapshot.ball_y))
            game->velocity_x = 0;
        else
            game->snapshot.ball_x = next;
        next = game->snapshot.ball_y + game->velocity_y * seconds;
        if (blocked(game->level, game->snapshot.ball_x, next))
            game->velocity_y = 0;
        else
            game->snapshot.ball_y = next;

        if (game->snapshot.elapsed_ms > UINT32_MAX - step)
            game->snapshot.elapsed_ms = UINT32_MAX;
        else
            game->snapshot.elapsed_ms += step;
        remaining -= step;
        if ((int)game->snapshot.ball_x == game->level->goal_col &&
            (int)game->snapshot.ball_y == game->level->goal_row)
        {
            game->snapshot.state = MAZE_WON;
            game->velocity_x = 0;
            game->velocity_y = 0;
            break;
        }
    }
    game->snapshot.input_status = MAZE_INPUT_VALID;
    ++game->snapshot.revision;
    return MAZE_OK;
}

maze_result_t maze_game_snapshot(const maze_game_t *game,
                                 maze_snapshot_t *snapshot)
{
    if (!game || !snapshot)
        return MAZE_INVALID;
    *snapshot = game->snapshot;
    return MAZE_OK;
}
