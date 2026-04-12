#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transactions.h"
#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/key/key_runtime_trace.h"
#include "users/noah/lib/pointing/pd_modes.h"
#include "users/noah/lib/state/keyboard_mod_state.h"
#include "users/noah/lib/state/layer_ownership.h"
#include "users/noah/lib/state/runtime_shared_state.h"
#include "users/noah/lib/state/runtime_trace.h"
#include "users/noah/lib/state/split_runtime_sync.h"

static uint32_t fake_time32;
static bool     fake_is_master;
static bool     fake_dragscroll_enabled;
static bool     fake_sniping_enabled;
static uint16_t fake_default_dpi;
static uint16_t fake_last_cpi;

static int8_t                      rpc_registered_id;
static slave_callback_t            rpc_registered_callback;
static uint8_t                     rpc_send_count;
static split_runtime_sync_packet_t rpc_last_packet;

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
    rpc_registered_id       = -1;
    rpc_registered_callback = NULL;
    rpc_send_count          = 0u;
    rpc_last_packet         = (split_runtime_sync_packet_t){0};
    layer_state             = 0;

    runtime_shared_state_reset(&noah_runtime_shared_state);
    noah_runtime_trace_reset();
    layer_ownership_reset_for_test();
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

keyboard_mod_state_t keyboard_mod_state_suspend(void) {
    return (keyboard_mod_state_t){0};
}

void keyboard_mod_state_apply(keyboard_mod_state_t state) {
    (void)state;
}

void noah_emit_synthetic_qmk_tap(uint16_t keycode, noah_emit_policy_t policy) {
    (void)keycode;
    (void)policy;
}

void noah_emit_literal_tap(uint16_t keycode, noah_emit_policy_t policy) {
    (void)keycode;
    (void)policy;
}

void transaction_register_rpc(int8_t transaction_id, slave_callback_t callback) {
    rpc_registered_id       = transaction_id;
    rpc_registered_callback = callback;
}

bool transaction_rpc_send(int8_t transaction_id, uint8_t initiator2target_buffer_size, const void *initiator2target_buffer) {
    CHECK(transaction_id == PUT_SPLIT_RUNTIME_SYNC);
    CHECK(initiator2target_buffer_size == sizeof(split_runtime_sync_packet_t));

    rpc_send_count++;
    memcpy(&rpc_last_packet, initiator2target_buffer, sizeof(rpc_last_packet));
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
    noah_runtime_trace_snapshot_t snapshot;
    split_runtime_sync_packet_t   packet = {
        .pd_mode_flags        = PD_MODE_ZOOM,
        .pd_mode_locked_flags = PD_MODE_ZOOM,
        .key_preview_layer    = UINT8_MAX,
    };

    test_reset_stubs();

    CHECK(pd_mode_handle_keycode_press(VOLUME_MODE));
    CHECK(pd_mode_toggle_lock_state(PD_MODE_VOLUME));
    CHECK(pd_mode_toggle_lock_state(PD_MODE_VOLUME));
    pd_mode_apply_remote_snapshot(PD_MODE_ARROW, PD_MODE_ARROW);

    split_runtime_sync_init();
    CHECK(rpc_registered_id == PUT_SPLIT_RUNTIME_SYNC);
    CHECK(rpc_registered_callback != NULL);
    CHECK(rpc_send_count == 1u);
    CHECK(rpc_last_packet.pd_mode_flags == 0u);
    CHECK(rpc_last_packet.pd_mode_locked_flags == 0u);

    rpc_registered_callback(sizeof(packet), &packet, 0u, NULL);

    snapshot = test_trace_snapshot();

    CHECK(snapshot.count == 9u);
    CHECK(snapshot.entries[0].kind == NOAH_TRACE_PD_MODE);
    CHECK(snapshot.entries[0].event == NOAH_TRACE_PD_MODE_EVENT_ACTIVATE);
    CHECK(snapshot.entries[0].a == PD_MODE_VOLUME);
    CHECK(snapshot.entries[0].b == PD_MODE_VOLUME);

    CHECK(snapshot.entries[1].event == NOAH_TRACE_PD_MODE_EVENT_LOCK);
    CHECK(snapshot.entries[1].a == PD_MODE_VOLUME);
    CHECK(snapshot.entries[1].b == PD_MODE_VOLUME);

    CHECK(snapshot.entries[2].event == NOAH_TRACE_PD_MODE_EVENT_DEACTIVATE);
    CHECK(snapshot.entries[2].a == PD_MODE_VOLUME);
    CHECK(snapshot.entries[2].b == 0u);

    CHECK(snapshot.entries[3].event == NOAH_TRACE_PD_MODE_EVENT_UNLOCK);
    CHECK(snapshot.entries[3].a == PD_MODE_VOLUME);
    CHECK(snapshot.entries[3].b == 0u);

    CHECK(snapshot.entries[4].event == NOAH_TRACE_PD_MODE_EVENT_REMOTE_SNAPSHOT);
    CHECK(snapshot.entries[4].a == PD_MODE_ARROW);
    CHECK(snapshot.entries[4].b == PD_MODE_ARROW);

    CHECK(snapshot.entries[5].kind == NOAH_TRACE_SPLIT_SYNC);
    CHECK(snapshot.entries[5].event == NOAH_TRACE_SPLIT_SYNC_EVENT_INIT);
    CHECK(snapshot.entries[5].a == 1u);
    CHECK(snapshot.entries[5].b == 0u);

    CHECK(snapshot.entries[6].kind == NOAH_TRACE_SPLIT_SYNC);
    CHECK(snapshot.entries[6].event == NOAH_TRACE_SPLIT_SYNC_EVENT_SEND);
    CHECK(snapshot.entries[6].a == 0u);
    CHECK(snapshot.entries[6].b == 0u);

    CHECK(snapshot.entries[7].kind == NOAH_TRACE_SPLIT_SYNC);
    CHECK(snapshot.entries[7].event == NOAH_TRACE_SPLIT_SYNC_EVENT_RECEIVE);
    CHECK(snapshot.entries[7].a == PD_MODE_ZOOM);
    CHECK(snapshot.entries[7].b == PD_MODE_ZOOM);

    CHECK(snapshot.entries[8].kind == NOAH_TRACE_PD_MODE);
    CHECK(snapshot.entries[8].event == NOAH_TRACE_PD_MODE_EVENT_REMOTE_SNAPSHOT);
    CHECK(snapshot.entries[8].a == PD_MODE_ZOOM);
    CHECK(snapshot.entries[8].b == PD_MODE_ZOOM);
}

int main(void) {
    test_ring_buffer_retains_recent_tail_when_full();
    test_key_runtime_and_layer_ownership_share_one_trace_buffer();
    test_pd_mode_and_split_sync_events_share_one_trace_buffer();

    puts("runtime_trace host tests passed");
    return 0;
}
