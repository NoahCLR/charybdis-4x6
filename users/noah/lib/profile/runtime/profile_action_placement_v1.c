// ────────────────────────────────────────────────────────────────────────────
// Profile-Wire v1 Action Placement
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "profile_action_placement_v1.h"

#include "profile_action_runtime_v1.h"
#include "../../action/action_dispatch.h"

bool noah_profile_action_placement_v1_supported(const noah_profile_action_v1_t *action, noah_profile_validator_v1_placement_t placement) {
    uint16_t native;

    if (noah_profile_action_runtime_v1_to_native(action, &native) != NOAH_PROFILE_ACTION_RUNTIME_V1_OK) {
        return false;
    }

    switch (placement) {
        case NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_KEY:
            return noah_action_supported_at(native, NOAH_ACTION_PLACEMENT_KEY);
        case NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_TAP:
            return noah_action_supported_at(native, NOAH_ACTION_PLACEMENT_BEHAVIOR_TAP);
        case NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_HOLD_PRESS_AND_HOLD:
            return noah_action_supported_at(native, NOAH_ACTION_PLACEMENT_BEHAVIOR_HOLD_PRESS_AND_HOLD);
        case NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_HOLD_OTHER:
            return noah_action_supported_at(native, NOAH_ACTION_PLACEMENT_BEHAVIOR_HOLD_OTHER);
        case NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_COMBO_OUTPUT:
            return noah_action_supported_at(native, NOAH_ACTION_PLACEMENT_COMBO_OUTPUT);
        default:
            return false;
    }
}
