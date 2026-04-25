#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transactions.h"
#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/key/runtime/trace.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/key/runtime/core/trace.h"
#include "users/noah/lib/state/runtime/keyboard_mod_state.h"
#include "users/noah/lib/state/ownership/layer_ownership.h"
#include "users/noah/lib/state/runtime/runtime_trace.h"
#include "users/noah/lib/state/runtime/split_runtime_sync.h"
#include "host_runtime_reset_fixture.h"

static uint32_t fake_time32;
static bool     fake_is_master;
static bool     fake_dragscroll_enabled;
static bool     fake_sniping_enabled;
static uint16_t fake_default_dpi;
static uint16_t fake_last_cpi;

static uint8_t                          rpc_register_count;
static int8_t                           rpc_registered_ids[3];
static slave_callback_t                 rpc_registered_callbacks[3];
static uint8_t                          rpc_send_count;
static split_runtime_base_sync_packet_t rpc_last_base_packet;

layer_state_t layer_state;

static void test_fail(const char *expr, const char *file, int line) {
    fprintf(stderr, "test failed: %s (%s:%d)\n", expr, file, line);
    exit(1);
}

#define CHECK(expr)                               \
    do {                                          \
        if (!(expr)) {                            \
            test_fail(#expr, __FILE__, __LINE__); \
        }                                         \
    } while (0)

static keypos_t test_keypos(uint8_t row, uint8_t col) {
    return (keypos_t){
        .row = row,
        .col = col,
    };
}

static uint16_t test_pack_keypos(keypos_t key_pos) {
    return (uint16_t)(((uint16_t)key_pos.row << 8) | key_pos.col);
}

static void test_reset_stubs(void) {
    fake_time32             = 1000u;
    fake_is_master          = true;
    fake_dragscroll_enabled = false;
    fake_sniping_enabled    = false;
    fake_default_dpi        = 800u;
    fake_last_cpi           = 0u;
    rpc_register_count      = 0u;
    memset(rpc_registered_ids, -1, sizeof(rpc_registered_ids));
    memset(rpc_registered_callbacks, 0, sizeof(rpc_registered_callbacks));
    rpc_send_count       = 0u;
    rpc_last_base_packet = (split_runtime_base_sync_packet_t){0};
    layer_state          = 0;

    host_runtime_fixture_reset_userspace_runtime();
}

uint32_t timer_read32(void) {
    return fake_time32;
}

uint32_t timer_elapsed32(uint32_t last) {
    return fake_time32 - last;
}

bool is_keyboard_master(void) {
    return fake_is_master;
}

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return layer < LAYER_COUNT && (state & ((layer_state_t)1u << layer)) != 0;
}

void layer_on(uint8_t layer) {
    layer_state |= (layer_state_t)1u << layer;
}

void layer_off(uint8_t layer) {
    layer_state &= (layer_state_t) ~((layer_state_t)1u << layer);
}

bool charybdis_get_pointer_dragscroll_enabled(void) {
    return fake_dragscroll_enabled;
}

bool charybdis_get_pointer_sniping_enabled(void) {
    return fake_sniping_enabled;
}

void clear_mods(void) {}
void clear_weak_mods(void) {}
void clear_oneshot_mods(void) {}
void clear_oneshot_locked_mods(void) {}
void send_keyboard_report(void) {}

uint16_t charybdis_get_pointer_default_dpi(void) {
    return fake_default_dpi;
}

void charybdis_set_pointer_dragscroll_enabled(bool enabled) {
    fake_dragscroll_enabled = enabled;
}

void charybdis_set_pointer_sniping_enabled(bool enabled) {
    fake_sniping_enabled = enabled;
}

void pointing_device_set_cpi(uint16_t cpi) {
    fake_last_cpi = cpi;
}

void keyboard_mod_ownership_register(uint16_t keycode) {
    (void)keycode;
}

void keyboard_mod_ownership_unregister(uint16_t keycode) {
    (void)keycode;
}

uint8_t keyboard_mod_ownership_managed_only_mask(uint8_t mods) {
    return mods;
}

keyboard_mod_state_t keyboard_mod_state_suspend(void) {
    return (keyboard_mod_state_t){0};
}

void keyboard_mod_state_apply(keyboard_mod_state_t state) {
    (void)state;
}

uint8_t key_feedback_preview_layer(void) {
    return UINT8_MAX;
}

uint8_t key_feedback_flash_meta(void) {
    return 0u;
}

void key_feedback_semantic_map(uint8_t *out_map) {
    key_feedback_semantic_map_clear(out_map);
}

void combo_feedback_underlay_bitmap(uint8_t *out_bitmap) {
    key_origin_bitmap_clear(out_bitmap);
}

void combo_feedback_overlay_bitmap(uint8_t *out_bitmap) {
    key_origin_bitmap_clear(out_bitmap);
}

void noah_emit_synthetic_qmk_tap(uint16_t keycode, noah_emit_policy_t policy) {
    (void)keycode;
    (void)policy;
}

void noah_emit_synthetic_qmk_tap_with_masked_keyboard_mods(uint16_t keycode, uint8_t masked_mods, bool settle_pending_fallback_holds) {
    (void)keycode;
    (void)masked_mods;
    (void)settle_pending_fallback_holds;
}

void noah_emit_literal_tap(uint16_t keycode, noah_emit_policy_t policy) {
    (void)keycode;
    (void)policy;
}

static slave_callback_t test_registered_callback(int8_t transaction_id) {
    for (uint8_t index = 0; index < rpc_register_count; index++) {
        if (rpc_registered_ids[index] == transaction_id) {
            return rpc_registered_callbacks[index];
        }
    }

    return NULL;
}

void transaction_register_rpc(int8_t transaction_id, slave_callback_t callback) {
    CHECK(rpc_register_count < 3u);
    rpc_registered_ids[rpc_register_count]       = transaction_id;
    rpc_registered_callbacks[rpc_register_count] = callback;
    rpc_register_count++;
}

bool transaction_rpc_send(int8_t transaction_id, uint8_t initiator2target_buffer_size, const void *initiator2target_buffer) {
    rpc_send_count++;

    if (transaction_id == PUT_SPLIT_RUNTIME_BASE_SYNC) {
        CHECK(initiator2target_buffer_size == sizeof(split_runtime_base_sync_packet_t));
        memcpy(&rpc_last_base_packet, initiator2target_buffer, sizeof(rpc_last_base_packet));
        return true;
    }

    if (transaction_id == PUT_SPLIT_COMBO_FEEDBACK_SYNC) {
        CHECK(initiator2target_buffer_size == sizeof(split_runtime_combo_feedback_packet_t));
        return true;
    }

    if (transaction_id == PUT_SPLIT_KEY_FEEDBACK_SYNC) {
        CHECK(initiator2target_buffer_size == sizeof(split_runtime_key_feedback_packet_t));
        return true;
    }

    CHECK(false);
    return true;
}

static noah_runtime_trace_snapshot_t test_trace_snapshot(void) {
    noah_runtime_trace_snapshot_t snapshot;

    noah_runtime_trace_snapshot(&snapshot);
    return snapshot;
}

static void test_ring_buffer_retains_recent_tail_when_full(void) {
    noah_runtime_trace_snapshot_t snapshot;

    test_reset_stubs();

    for (uint16_t i = 0; i < (uint16_t)(NOAH_RUNTIME_TRACE_CAPACITY + 3u); i++) {
        noah_runtime_trace_emit(NOAH_TRACE_SPLIT_SYNC, NOAH_TRACE_SPLIT_SYNC_EVENT_SEND, i, (uint16_t)(i + 100u));
    }

    snapshot = test_trace_snapshot();

    CHECK(snapshot.count == NOAH_RUNTIME_TRACE_CAPACITY);
    CHECK(snapshot.overflowed);
    CHECK(snapshot.entries[0].kind == NOAH_TRACE_SPLIT_SYNC);
    CHECK(snapshot.entries[0].event == NOAH_TRACE_SPLIT_SYNC_EVENT_SEND);
    CHECK(snapshot.entries[0].a == 3u);
    CHECK(snapshot.entries[0].b == 103u);
    CHECK(snapshot.entries[NOAH_RUNTIME_TRACE_CAPACITY - 1u].a == (uint16_t)(NOAH_RUNTIME_TRACE_CAPACITY + 2u));
    CHECK(snapshot.entries[NOAH_RUNTIME_TRACE_CAPACITY - 1u].b == (uint16_t)(NOAH_RUNTIME_TRACE_CAPACITY + 102u));
}

static void test_key_runtime_and_layer_ownership_share_one_trace_buffer(void) {
    noah_runtime_trace_snapshot_t snapshot;
    key_runtime_transition_plan_t plan = {
        .count      = 2u,
        .overflowed = true,
    };
    key_runtime_effect_t effect = {
        .kind = KEY_RUNTIME_EFFECT_LAYER_PRESS,
    };
    keypos_t layer_key = test_keypos(1, 2);

    test_reset_stubs();

    key_runtime_trace_plan("press", &plan);
    key_runtime_trace_effect_execute(1u, &effect);
    CHECK(layer_ownership_set_lock_state(2u, true));
    layer_ownership_momentary_press(layer_key, 3u);
    CHECK(layer_ownership_momentary_release(layer_key));

    snapshot = test_trace_snapshot();

    CHECK(snapshot.count == 5u);
    CHECK(snapshot.entries[0].kind == NOAH_TRACE_KEY_RUNTIME);
    CHECK(snapshot.entries[0].event == NOAH_TRACE_KEY_RUNTIME_EVENT_PLAN);
    CHECK(snapshot.entries[0].a == NOAH_TRACE_KEY_RUNTIME_STAGE_PRESS);
    CHECK(snapshot.entries[0].b == (uint16_t)(0x8000u | 2u));

    CHECK(snapshot.entries[1].kind == NOAH_TRACE_KEY_RUNTIME);
    CHECK(snapshot.entries[1].event == NOAH_TRACE_KEY_RUNTIME_EVENT_EFFECT_EXECUTE);
    CHECK(snapshot.entries[1].a == KEY_RUNTIME_EFFECT_LAYER_PRESS);
    CHECK(snapshot.entries[1].b == 1u);

    CHECK(snapshot.entries[2].kind == NOAH_TRACE_LAYER_OWNERSHIP);
    CHECK(snapshot.entries[2].event == NOAH_TRACE_LAYER_OWNERSHIP_EVENT_LOCK);
    CHECK(snapshot.entries[2].a == 2u);
    CHECK(snapshot.entries[2].b == 1u);

    CHECK(snapshot.entries[3].kind == NOAH_TRACE_LAYER_OWNERSHIP);
    CHECK(snapshot.entries[3].event == NOAH_TRACE_LAYER_OWNERSHIP_EVENT_MOMENTARY_PRESS);
    CHECK(snapshot.entries[3].a == 3u);
    CHECK(snapshot.entries[3].b == test_pack_keypos(layer_key));

    CHECK(snapshot.entries[4].kind == NOAH_TRACE_LAYER_OWNERSHIP);
    CHECK(snapshot.entries[4].event == NOAH_TRACE_LAYER_OWNERSHIP_EVENT_MOMENTARY_RELEASE);
    CHECK(snapshot.entries[4].a == 3u);
    CHECK(snapshot.entries[4].b == test_pack_keypos(layer_key));
}

static void test_pd_mode_and_split_sync_events_share_one_trace_buffer(void) {
    noah_runtime_trace_snapshot_t    snapshot;
    split_runtime_base_sync_packet_t packet = {
        .active_mode_id    = pd_mode_id_from_mask(PD_MODE_ZOOM),
        .locked_mode_id    = pd_mode_id_from_mask(PD_MODE_ZOOM),
        .key_preview_layer = UINT8_MAX,
    };

    test_reset_stubs();

    CHECK(pd_mode_handle_keycode_press(VOLUME_MODE));
    CHECK(pd_mode_toggle_lock_state(PD_MODE_VOLUME));
    CHECK(pd_mode_toggle_lock_state(PD_MODE_VOLUME));
    pd_mode_apply_remote_snapshot(PD_MODE_ARROW, PD_MODE_ARROW);

    split_runtime_sync_init();
    CHECK(rpc_register_count == 3u);
    CHECK(test_registered_callback(PUT_SPLIT_RUNTIME_BASE_SYNC) != NULL);
    CHECK(test_registered_callback(PUT_SPLIT_COMBO_FEEDBACK_SYNC) != NULL);
    CHECK(test_registered_callback(PUT_SPLIT_KEY_FEEDBACK_SYNC) != NULL);
    CHECK(rpc_send_count == 3u);
    CHECK(rpc_last_base_packet.active_mode_id == PD_MODE_ID_NONE);
    CHECK(rpc_last_base_packet.locked_mode_id == PD_MODE_ID_NONE);

    test_registered_callback(PUT_SPLIT_RUNTIME_BASE_SYNC)(sizeof(packet), &packet, 0u, NULL);

    snapshot = test_trace_snapshot();

    CHECK(snapshot.count == 10u);
    CHECK(snapshot.entries[0].kind == NOAH_TRACE_PD_MODE);
    CHECK(snapshot.entries[0].event == NOAH_TRACE_PD_MODE_EVENT_KEY_PRESS);
    CHECK(snapshot.entries[0].a == PD_MODE_VOLUME);
    CHECK(snapshot.entries[0].b == test_pack_keypos((keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS}));

    CHECK(snapshot.entries[1].event == NOAH_TRACE_PD_MODE_EVENT_ACTIVATE);
    CHECK(snapshot.entries[1].a == PD_MODE_VOLUME);
    CHECK(snapshot.entries[1].b == PD_MODE_VOLUME);

    CHECK(snapshot.entries[2].event == NOAH_TRACE_PD_MODE_EVENT_LOCK);
    CHECK(snapshot.entries[2].a == PD_MODE_VOLUME);
    CHECK(snapshot.entries[2].b == PD_MODE_VOLUME);

    CHECK(snapshot.entries[3].event == NOAH_TRACE_PD_MODE_EVENT_DEACTIVATE);
    CHECK(snapshot.entries[3].a == PD_MODE_VOLUME);
    CHECK(snapshot.entries[3].b == 0u);

    CHECK(snapshot.entries[4].event == NOAH_TRACE_PD_MODE_EVENT_UNLOCK);
    CHECK(snapshot.entries[4].a == PD_MODE_VOLUME);
    CHECK(snapshot.entries[4].b == 0u);

    CHECK(snapshot.entries[5].event == NOAH_TRACE_PD_MODE_EVENT_REMOTE_SNAPSHOT);
    CHECK(snapshot.entries[5].a == PD_MODE_ARROW);
    CHECK(snapshot.entries[5].b == PD_MODE_ARROW);

    CHECK(snapshot.entries[6].kind == NOAH_TRACE_SPLIT_SYNC);
    CHECK(snapshot.entries[6].event == NOAH_TRACE_SPLIT_SYNC_EVENT_INIT);
    CHECK(snapshot.entries[6].a == 1u);
    CHECK(snapshot.entries[6].b == 0u);

    CHECK(snapshot.entries[7].kind == NOAH_TRACE_SPLIT_SYNC);
    CHECK(snapshot.entries[7].event == NOAH_TRACE_SPLIT_SYNC_EVENT_SEND);
    CHECK(snapshot.entries[7].a == 0u);
    CHECK(snapshot.entries[7].b == 0u);

    CHECK(snapshot.entries[8].kind == NOAH_TRACE_SPLIT_SYNC);
    CHECK(snapshot.entries[8].event == NOAH_TRACE_SPLIT_SYNC_EVENT_RECEIVE);
    CHECK(snapshot.entries[8].a == PD_MODE_ZOOM);
    CHECK(snapshot.entries[8].b == PD_MODE_ZOOM);

    CHECK(snapshot.entries[9].kind == NOAH_TRACE_PD_MODE);
    CHECK(snapshot.entries[9].event == NOAH_TRACE_PD_MODE_EVENT_REMOTE_SNAPSHOT);
    CHECK(snapshot.entries[9].a == PD_MODE_ZOOM);
    CHECK(snapshot.entries[9].b == PD_MODE_ZOOM);
}

static void test_key_runtime_decision_events_capture_release_hold_and_multi_tap_details(void) {
    noah_runtime_trace_snapshot_t snapshot;

    test_reset_stubs();

    key_runtime_trace_release_resolution(KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING, KEY_RUNTIME_TRACE_RELEASE_OUTCOME_ACTION, KEY_RUNTIME_TRACE_RELEASE_FLAG_QUICK_TAP | KEY_RUNTIME_TRACE_RELEASE_FLAG_RELEASE_OWNED_STATE, KC_C);
    key_runtime_trace_hold_policy_decision(KEY_RUNTIME_TRACE_HOLD_POLICY_PROMOTE_LONG_HOLD, KEY_RUNTIME_TRACE_HOLD_DISPATCH_HELD, KEY_RUNTIME_TRACE_HOLD_POLICY_FLAG_COMPLETES_HOLD | KEY_RUNTIME_TRACE_HOLD_POLICY_FLAG_FEEDBACK_PULSE | KEY_RUNTIME_TRACE_HOLD_POLICY_FLAG_FEEDBACK_LONG, KC_LEFT_CTRL);
    key_runtime_trace_multi_tap_decision(KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_PRESS_FLUSH_CHAIN, 3u, KC_V);

    snapshot = test_trace_snapshot();

    CHECK(snapshot.count == 3u);

    CHECK(snapshot.entries[0].kind == NOAH_TRACE_KEY_RUNTIME);
    CHECK(snapshot.entries[0].event == NOAH_TRACE_KEY_RUNTIME_EVENT_RELEASE_RESOLUTION);
    CHECK(snapshot.entries[0].a == key_runtime_trace_pack_release_resolution(KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING, KEY_RUNTIME_TRACE_RELEASE_OUTCOME_ACTION, KEY_RUNTIME_TRACE_RELEASE_FLAG_QUICK_TAP | KEY_RUNTIME_TRACE_RELEASE_FLAG_RELEASE_OWNED_STATE));
    CHECK(snapshot.entries[0].b == KC_C);

    CHECK(snapshot.entries[1].kind == NOAH_TRACE_KEY_RUNTIME);
    CHECK(snapshot.entries[1].event == NOAH_TRACE_KEY_RUNTIME_EVENT_HOLD_POLICY_DECISION);
    CHECK(snapshot.entries[1].a == key_runtime_trace_pack_hold_policy_decision(KEY_RUNTIME_TRACE_HOLD_POLICY_PROMOTE_LONG_HOLD, KEY_RUNTIME_TRACE_HOLD_DISPATCH_HELD, KEY_RUNTIME_TRACE_HOLD_POLICY_FLAG_COMPLETES_HOLD | KEY_RUNTIME_TRACE_HOLD_POLICY_FLAG_FEEDBACK_PULSE | KEY_RUNTIME_TRACE_HOLD_POLICY_FLAG_FEEDBACK_LONG));
    CHECK(snapshot.entries[1].b == KC_LEFT_CTRL);

    CHECK(snapshot.entries[2].kind == NOAH_TRACE_KEY_RUNTIME);
    CHECK(snapshot.entries[2].event == NOAH_TRACE_KEY_RUNTIME_EVENT_MULTI_TAP_DECISION);
    CHECK(snapshot.entries[2].a == key_runtime_trace_pack_multi_tap_decision(KEY_RUNTIME_TRACE_MULTI_TAP_DECISION_PRESS_FLUSH_CHAIN, 3u));
    CHECK(snapshot.entries[2].b == KC_V);
}

static void test_key_runtime_core_input_events_round_trip_through_shared_trace_buffer(void) {
    noah_runtime_trace_snapshot_t snapshot;
    runtime_event_t               inputs[6] = {
        {
            .kind = RUNTIME_EVENT_KIND_KEY_DOWN,
            .data.key_event =
                {
                    .keycode = KC_C,
                    .key_pos = {.row = 2, .col = 3},
                },
        },
        {
            .kind = RUNTIME_EVENT_KIND_TIMER_ADVANCE,
            .data.timer_advance =
                {
                    .advance_ms = 37u,
                },
        },
        {
            .kind = RUNTIME_EVENT_KIND_POINTER_REPORT,
            .data.pointer_report =
                {
                    .report =
                        {
                            .x       = -5,
                            .y       = 7,
                            .h       = 2,
                            .v       = -3,
                            .buttons = 0x15u,
                        },
                },
        },
        {
            .kind = RUNTIME_EVENT_KIND_REMOTE_SNAPSHOT,
            .data.remote_snapshot =
                {
                    .active_mode = PD_MODE_ARROW,
                    .locked_mode = PD_MODE_ARROW,
                },
        },
        {
            .kind = RUNTIME_EVENT_KIND_KEY_UP,
            .data.key_event =
                {
                    .keycode = KC_C,
                    .key_pos = {.row = 2, .col = 3},
                },
        },
        {
            .kind = RUNTIME_EVENT_KIND_SCAN,
        },
    };
    runtime_event_t decoded[ARRAY_SIZE(inputs)];

    test_reset_stubs();

    for (uint8_t index = 0; index < ARRAY_SIZE(inputs); index++) {
        key_runtime_core_trace_record_input_event(&inputs[index]);
    }

    snapshot = test_trace_snapshot();

    CHECK(snapshot.count == 7u);
    CHECK(snapshot.entries[0].kind == NOAH_TRACE_KEY_RUNTIME_CORE);
    CHECK(snapshot.entries[0].event == NOAH_TRACE_KEY_RUNTIME_CORE_EVENT_INPUT_KEY_DOWN);
    CHECK(snapshot.entries[1].event == NOAH_TRACE_KEY_RUNTIME_CORE_EVENT_INPUT_TIMER_ADVANCE);
    CHECK(snapshot.entries[2].event == NOAH_TRACE_KEY_RUNTIME_CORE_EVENT_INPUT_POINTER_REPORT_AXES);
    CHECK(snapshot.entries[3].event == NOAH_TRACE_KEY_RUNTIME_CORE_EVENT_INPUT_POINTER_REPORT_BUTTONS);
    CHECK(snapshot.entries[4].event == NOAH_TRACE_KEY_RUNTIME_CORE_EVENT_INPUT_REMOTE_SNAPSHOT);
    CHECK(snapshot.entries[5].event == NOAH_TRACE_KEY_RUNTIME_CORE_EVENT_INPUT_KEY_UP);
    CHECK(snapshot.entries[6].event == NOAH_TRACE_KEY_RUNTIME_CORE_EVENT_INPUT_SCAN);

    CHECK(key_runtime_core_trace_decode_input_events(&snapshot, decoded, ARRAY_SIZE(decoded)) == ARRAY_SIZE(inputs));
    CHECK(decoded[0].kind == RUNTIME_EVENT_KIND_KEY_DOWN);
    CHECK(decoded[0].data.key_event.keycode == KC_C);
    CHECK(decoded[0].data.key_event.key_pos.row == 2u);
    CHECK(decoded[0].data.key_event.key_pos.col == 3u);
    CHECK(decoded[1].kind == RUNTIME_EVENT_KIND_TIMER_ADVANCE);
    CHECK(decoded[1].data.timer_advance.advance_ms == 37u);
    CHECK(decoded[2].kind == RUNTIME_EVENT_KIND_POINTER_REPORT);
    CHECK(decoded[2].data.pointer_report.report.x == -5);
    CHECK(decoded[2].data.pointer_report.report.y == 7);
    CHECK(decoded[2].data.pointer_report.report.h == 2);
    CHECK(decoded[2].data.pointer_report.report.v == -3);
    CHECK(decoded[2].data.pointer_report.report.buttons == 0x15u);
    CHECK(decoded[3].kind == RUNTIME_EVENT_KIND_REMOTE_SNAPSHOT);
    CHECK(decoded[3].data.remote_snapshot.active_mode == PD_MODE_ARROW);
    CHECK(decoded[3].data.remote_snapshot.locked_mode == PD_MODE_ARROW);
    CHECK(decoded[4].kind == RUNTIME_EVENT_KIND_KEY_UP);
    CHECK(decoded[4].data.key_event.keycode == KC_C);
    CHECK(decoded[5].kind == RUNTIME_EVENT_KIND_SCAN);
}

int main(void) {
    test_ring_buffer_retains_recent_tail_when_full();
    test_key_runtime_and_layer_ownership_share_one_trace_buffer();
    test_pd_mode_and_split_sync_events_share_one_trace_buffer();
    test_key_runtime_decision_events_capture_release_hold_and_multi_tap_details();
    test_key_runtime_core_input_events_round_trip_through_shared_trace_buffer();

    puts("runtime_trace host tests passed");
    return 0;
}
