// ────────────────────────────────────────────────────────────────────────────
// Action Kind Registry List
// ────────────────────────────────────────────────────────────────────────────
//
// Single-source action-kind definitions. Keep enum identity, classification
// priority, metadata, and dispatch hooks synchronized from this list.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#define NOAH_ACTION_POLICY_NONE 0u

#define NOAH_ACTION_KIND_REGISTRY(X) \
    X(LITERAL, 0u, noah_action_kind_match_literal, \
      NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED | \
          NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED | \
          NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED | \
          NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED, \
      true, false, NOAH_ACTION_DISPATCH_STANDARD, \
      NOAH_ACTION_POLICY_PRESS_AND_HOLD_USES_HELD_LIFECYCLE, \
      noah_action_tap_literal, noah_action_press_literal, noah_action_release_literal) \
    X(LAYER_LOCK, 100u, noah_action_kind_match_layer_lock, \
      NOAH_ACTION_CAP_PRESS_ONLY | \
          NOAH_ACTION_CAP_LAYER_AFFECTING | \
          NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED | \
          NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED | \
          NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED | \
          NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED | \
          NOAH_ACTION_CAP_CONSUMES_DIRECT_PRESS, \
      false, false, 0, \
      NOAH_ACTION_POLICY_RELEASES_MOMENTARY_LAYER_BEFORE_ACTION, \
      noah_action_tap_layer_lock, noah_action_press_layer_lock, noah_action_release_noop) \
    X(LAYER_HOLD, 90u, noah_action_kind_match_layer_hold, \
      NOAH_ACTION_CAP_REQUIRES_KEY_OWNER | \
          NOAH_ACTION_CAP_LAYER_AFFECTING | \
          NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED | \
          NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED, \
      false, true, NOAH_ACTION_DISPATCH_STANDARD, \
      NOAH_ACTION_POLICY_RUNTIME_HANDLED_KEYCODE | \
          NOAH_ACTION_POLICY_MOMENTARY_LAYER_KEYCODE | \
          NOAH_ACTION_POLICY_PRESS_AND_HOLD_USES_HELD_LIFECYCLE, \
      noah_action_tap_unsupported_layer, noah_action_press_owned_momentary_layer, noah_action_release_owned_momentary_layer) \
    X(LAYER_TAP, 80u, noah_action_kind_match_layer_tap, \
      NOAH_ACTION_CAP_LAYER_AFFECTING | \
          NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED, \
      false, false, NOAH_ACTION_DISPATCH_STANDARD, \
      NOAH_ACTION_POLICY_AUTHORED_LAYER_TAP_CONTRACT | \
          NOAH_ACTION_POLICY_DEFAULT_TAP_USES_LAYER_TAP_KEYCODE, \
      noah_action_tap_unsupported_layer, noah_action_press_unsupported_layer, noah_action_release_unsupported_layer) \
    X(UNSUPPORTED_LAYER_ACTION, 70u, noah_action_kind_match_unsupported_layer_action, \
      NOAH_ACTION_CAP_LAYER_AFFECTING, \
      false, false, NOAH_ACTION_DISPATCH_STANDARD, \
      NOAH_ACTION_POLICY_NONE, \
      noah_action_tap_unsupported_layer, noah_action_press_unsupported_layer, noah_action_release_unsupported_layer) \
    X(MACRO, 40u, noah_action_kind_match_macro, \
      NOAH_ACTION_CAP_PRESS_ONLY | \
          NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED | \
          NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED | \
          NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED | \
          NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED, \
      true, false, NOAH_ACTION_DISPATCH_STANDARD, \
      NOAH_ACTION_POLICY_NONE, \
      noah_action_tap_noop, noah_action_press_noop, noah_action_release_noop) \
    X(QMK_BEHAVIOR, 30u, noah_action_kind_match_qmk_behavior, \
      NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED | \
          NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED | \
          NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED | \
          NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED, \
      true, false, NOAH_ACTION_DISPATCH_STANDARD, \
      NOAH_ACTION_POLICY_PRESS_AND_HOLD_USES_HELD_LIFECYCLE, \
      noah_action_tap_qmk_behavior, noah_action_press_qmk_behavior, noah_action_release_qmk_behavior) \
    X(KEYMAP_CUSTOM, 20u, noah_action_kind_match_keymap_custom, \
      NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED | \
          NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED | \
          NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED | \
          NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED, \
      true, false, NOAH_ACTION_DISPATCH_STANDARD, \
      NOAH_ACTION_POLICY_PRESS_AND_HOLD_USES_HELD_LIFECYCLE, \
      noah_action_tap_keymap_custom, noah_action_press_keymap_custom, noah_action_release_keymap_custom) \
    X(PD_MODE_HOLD, 60u, noah_action_kind_match_pd_mode_hold, \
      NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED | \
          NOAH_ACTION_CAP_PD_MODE_AFFECTING | \
          NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED | \
          NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED | \
          NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED, \
      false, false, NOAH_ACTION_DISPATCH_STANDARD, \
      NOAH_ACTION_POLICY_RUNTIME_HANDLED_KEYCODE | \
          NOAH_ACTION_POLICY_PRESS_AND_HOLD_USES_HELD_LIFECYCLE, \
      noah_action_tap_literal, noah_action_press_literal, noah_action_release_literal) \
    X(PD_MODE_LOCK, 50u, noah_action_kind_match_pd_mode_lock, \
      NOAH_ACTION_CAP_PRESS_ONLY | \
          NOAH_ACTION_CAP_PD_MODE_AFFECTING | \
          NOAH_ACTION_CAP_BEHAVIOR_KEYCODE_SUPPORTED | \
          NOAH_ACTION_CAP_AUTHORED_TAP_SUPPORTED | \
          NOAH_ACTION_CAP_AUTHORED_HOLD_PRESS_AND_HOLD_SUPPORTED | \
          NOAH_ACTION_CAP_AUTHORED_HOLD_OTHER_SUPPORTED | \
          NOAH_ACTION_CAP_CONSUMES_DIRECT_PRESS, \
      false, false, 0, \
      NOAH_ACTION_POLICY_NONE, \
      noah_action_tap_pd_mode_lock, noah_action_press_pd_mode_lock, noah_action_release_noop)
