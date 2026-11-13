#ifndef ROLL3_RUNTIME_H
#define ROLL3_RUNTIME_H

#include "roll3_contract.h"

bool roll3_runtime_init(bool selftests_passed);
/* Called by the UI thread. Busy includes a closing page's IMU cleanup. */
bool roll3_runtime_busy(void);
bool roll3_runtime_open(void);
bool roll3_runtime_open_level(unsigned level);
void roll3_runtime_close(void);
/* Age is measured at snapshot read, UINT32_MAX if no sample is available. */
bool roll3_runtime_read(r3_snapshot_t *snapshot, bool *pending, r3_result_t *result,
                        uint32_t *sample_age_ms);
void roll3_runtime_report_frame(uint32_t frames, uint32_t sample_age_ms,
                                uint32_t draw_ms, uint32_t frame_gap_ms);
void roll3_runtime_report_open(int result, unsigned depth_rows);
bool roll3_runtime_submit(r3_action_kind_t kind, uint32_t expected_run);

#endif
