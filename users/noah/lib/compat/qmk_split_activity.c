#include QMK_KEYBOARD_H

#if defined(NOAH_SPLIT_ACTIVITY_COALESCE_ENABLE) && defined(SPLIT_ACTIVITY_ENABLE)
#    include "transactions.h"
#    include "sync_timer.h"
#    include "qmk_split_activity_policy.h"
#    include "../profile/runtime/effective_settings_runtime.h"

#    ifndef QMK_SPLIT_ACTIVITY_POLICY_VERSION
#        error "Activity coalescing requires the QMK split activity policy hook"
#    endif
_Static_assert(QMK_SPLIT_ACTIVITY_POLICY_VERSION == 1, "QMK activity hook contract changed");

static noah_split_activity_policy_t activity_policy;

static uint32_t activity_timeout(void) {
    uint32_t timeout = 0u;
#    ifdef RGB_MATRIX_TIMEOUT
    timeout = RGB_MATRIX_TIMEOUT;
#    endif
#    ifdef NOAH_PORTABLE_PROFILE_ENABLE
    uint32_t live = noah_setting(NOAH_SETTING_RGB_TIMEOUT, 900000u);
    if (live && (!timeout || live < timeout)) timeout = live;
#    endif
    return timeout;
}

bool split_activity_sync_should_send(const split_slave_activity_sync_t *current, const split_slave_activity_sync_t *sent, uint32_t last_success, bool sent_once, bool force) {
    uint32_t timestamps[3] = {current->matrix_timestamp, current->encoder_timestamp, current->pointing_device_timestamp};
    // Match QMK's set_activity_timestamps aggregation exactly, including its
    // existing numerical-MAX behavior around source-clock wrap.
    uint32_t latest = sent->matrix_timestamp;
    if (sent->encoder_timestamp > latest) latest = sent->encoder_timestamp;
    if (sent->pointing_device_timestamp > latest) latest = sent->pointing_device_timestamp;
    return noah_split_activity_due(&activity_policy, timestamps, timer_read32(), timer_elapsed32(last_success), sync_timer_elapsed32(latest), activity_timeout(), force || !sent_once);
}

void split_activity_sync_sent(bool success) {
    noah_split_activity_complete(&activity_policy, success);
}
#endif
