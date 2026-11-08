#ifndef MAZE_CONTROLLER_H
#define MAZE_CONTROLLER_H

#include "maze_game.h"

#define MAZE_CALIBRATION_SAMPLES 32u
#define MAZE_CALIBRATION_TIMEOUT_MS 5000u
#define MAZE_SAMPLE_MAX_AGE_MS 100u
#define MAZE_MAX_CATCHUP_MS 250u
#define MAZE_FIXED_STEP_MS 5u

typedef struct
{
    float a[3]; /* g, sensor axes; x/y screen mapping awaits board validation. */
    float g[3]; /* degrees/second */
    uint32_t sequence;
    uint32_t sampled_ms;
    uint32_t run_id;
    bool valid;
} maze_motion_t;

/* Private fields. Only the UI owning thread may access this instance. */
typedef struct
{
    maze_game_t game;
    uint32_t allocated_request, anchor_ms, started_ms;
    uint32_t sample_ms, sample_sequence, calibration_count;
    uint32_t pending_ms, delay_paused_ms;
    float sum[2], bias[2], previous[3], filtered[2];
    maze_input_t input;
    bool have_sample;
    bool recovery_needs_sample;
} maze_controller_t;

maze_result_t maze_controller_init(maze_controller_t *c,
                                    const maze_level_t *level, uint32_t now_ms);
maze_result_t maze_controller_next_request(maze_controller_t *c, uint32_t *id);
maze_result_t maze_controller_action(maze_controller_t *c,
                                      const maze_action_t *action, uint32_t now_ms);
/* Same guard ordering; optional latest publication for PAUSE/RESUME settlement. */
maze_result_t maze_controller_action_with_sample(maze_controller_t *c,
    const maze_action_t *action, uint32_t now_ms, const maze_motion_t *sample);
maze_result_t maze_controller_tick(maze_controller_t *c, uint32_t now_ms,
                                    const maze_motion_t *sample);
maze_result_t maze_controller_input_failed(maze_controller_t *c, uint32_t run_id);
/* New acquisition session for an existing paused/calibrating game. Preserves
 * model position, time, run_id and request allocator; drops old sample history. */
maze_result_t maze_controller_reconnect(maze_controller_t *c, uint32_t now_ms);
maze_result_t maze_controller_snapshot(const maze_controller_t *c,
                                        maze_snapshot_t *out);

#endif
