#ifndef MAZE_GAME_H
#define MAZE_GAME_H

#include "maze_contract.h"

/* Allocate statically or on a long-lived stack. Treat fields as private.
 * All calls must run on one owning thread; this module does not lock. */
typedef struct
{
    maze_snapshot_t snapshot;
    const maze_level_t *level;
    float velocity_x;
    float velocity_y;
    uint32_t last_request_id;
} maze_game_t;

/* Safety checks only. B additionally validates reachability of formal maps. */
maze_result_t maze_game_level_check(const maze_level_t *level);
maze_result_t maze_game_init(maze_game_t *game, const maze_level_t *level);
maze_result_t maze_game_action(maze_game_t *game, const maze_action_t *action);
maze_result_t maze_game_calibrated(maze_game_t *game, uint32_t run_id);
maze_result_t maze_game_step(maze_game_t *game, const maze_input_t *input,
                             uint32_t dt_ms);
maze_result_t maze_game_snapshot(const maze_game_t *game,
                                 maze_snapshot_t *snapshot);

#endif
