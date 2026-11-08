#ifndef MAZE_CONTRACT_H
#define MAZE_CONTRACT_H

#include <stdbool.h>
#include <stdint.h>

/* Contract v2 adds recoverable delay pause diagnostics; geometry is still 2D. */
#define MAZE_CONTRACT_VERSION 2u
#define MAZE_MAX_COLS 16u
#define MAZE_MAX_ROWS 16u
#define MAZE_BALL_RADIUS 0.20f
#define MAZE_MAX_SPEED 3.0f
#define MAZE_MAX_DT_MS 100u /* Per primitive model batch, not UI callback gap. */

enum maze_cell
{
    MAZE_CELL_PATH = 0,
    MAZE_CELL_WALL = 1
};

typedef enum
{
    MAZE_OK = 0,
    MAZE_INVALID = -1,
    MAZE_WRONG_STATE = -2,
    MAZE_STALE = -3,
    MAZE_EXHAUSTED = -4
} maze_result_t;

typedef enum
{
    MAZE_READY,
    MAZE_CALIBRATING,
    MAZE_RUNNING,
    MAZE_PAUSED,
    MAZE_WON,
    MAZE_ERROR
} maze_state_t;

typedef enum
{
    MAZE_INPUT_NOT_READY,
    MAZE_INPUT_VALID,
    MAZE_INPUT_FAILED
} maze_input_status_t;

typedef enum
{
    MAZE_ERROR_NONE,
    MAZE_ERROR_LEVEL,
    MAZE_ERROR_INPUT,
    MAZE_ERROR_TIMING
} maze_error_t;

typedef enum
{
    MAZE_PAUSE_NONE,
    MAZE_PAUSE_USER,
    MAZE_PAUSE_DELAY
} maze_pause_reason_t;

typedef enum
{
    MAZE_ACTION_START,
    MAZE_ACTION_PAUSE,
    MAZE_ACTION_RESUME,
    MAZE_ACTION_RESTART,
    MAZE_ACTION_BACK
} maze_action_kind_t;

typedef struct
{
    uint32_t id;
    uint8_t cols;
    uint8_t rows;
    uint16_t cell_count;
    const uint8_t *cells; /* Immutable, row-major, outlives the game instance. */
    uint8_t start_col;
    uint8_t start_row;
    uint8_t goal_col;
    uint8_t goal_row;
} maze_level_t;

typedef struct
{
    uint32_t run_id;
    uint32_t revision;
    uint32_t level_id;
    maze_state_t state;
    maze_input_status_t input_status;
    maze_error_t error_code;
    float ball_x; /* Cells, origin top left; x right, y down. */
    float ball_y;
    float ball_radius;
    uint32_t elapsed_ms; /* Completed RUNNING simulation time, saturating. */
    maze_pause_reason_t pause_reason;
    uint32_t interrupted_ms; /* Unsimulated severe-delay time, this run, saturating. */
} maze_snapshot_t;

typedef struct
{
    maze_action_kind_t action;
    uint32_t expected_run_id;
    uint32_t request_id; /* Nonzero, strictly increasing across runs. */
} maze_action_t;

typedef struct
{
    float tilt_x; /* Finite normalized [-1, 1], already calibrated/mapped. */
    float tilt_y;
    bool valid;
} maze_input_t;

/* Synchronous, nonblocking, called only on the model/UI owning thread. */
typedef maze_result_t (*maze_action_sink_t)(void *context,
                                          const maze_action_t *action);

#endif
