#ifndef ROLL3_RUNTIME_H
#define ROLL3_RUNTIME_H

#include "roll3_contract.h"

bool roll3_runtime_init(bool selftests_passed);
/* Called by the UI thread. Busy includes a closing page's IMU cleanup. */
bool roll3_runtime_busy(void);
bool roll3_runtime_open(void);
void roll3_runtime_close(void);
bool roll3_runtime_read(r3_snapshot_t *snapshot, bool *pending, r3_result_t *result);
bool roll3_runtime_submit(r3_action_kind_t kind, uint32_t expected_run);

#endif
