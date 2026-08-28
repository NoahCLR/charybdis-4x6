#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dynamic_keymap.h"
#include "transactions.h"
#include "users/noah/lib/compat/qmk_via_split_mirror.h"
#include "users/noah/lib/compat/qmk_via_storage_contract.h"
#include "via.h"

enum {
    TEST_RPC_SIZE = RPC_M2S_BUFFER_SIZE,
    TEST_PAYLOAD_MAX = TEST_RPC_SIZE - 4u,
    TEST_SINK_SIZE = UINT8_MAX,
};

static slave_callback_t registered_callback;
static uint8_t          keymap_sink[TEST_SINK_SIZE];
static uint8_t          macro_sink[TEST_SINK_SIZE];
static uint16_t         keymap_write_count;
static uint16_t         macro_write_count;
static uint16_t         rgb_invalidate_count;
static uint16_t         macro_invalidate_count;
static uint16_t         storage_changed_count;

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

static void test_reset(void) {
    registered_callback    = NULL;
    keymap_write_count     = 0u;
    macro_write_count      = 0u;
    rgb_invalidate_count   = 0u;
    macro_invalidate_count = 0u;
    storage_changed_count  = 0u;
    memset(keymap_sink, 0xA5, sizeof(keymap_sink));
    memset(macro_sink, 0x5A, sizeof(macro_sink));
    noah_qmk_via_split_mirror_init();
    CHECK(registered_callback != NULL);
}

static void mirror_enqueue_frame(const uint8_t *frame, uint8_t length) {
    CHECK(frame != NULL);
    CHECK(length <= TEST_RPC_SIZE);
    registered_callback(length, frame, 0u, NULL);
}

static void mirror_receive_frame(const uint8_t *frame, uint8_t length) {
    mirror_enqueue_frame(frame, length);
    CHECK(noah_qmk_via_split_mirror_matrix_scan_step());
}

static void mirror_receive(uint8_t command, uint8_t payload_size, uint8_t length) {
    uint8_t frame[TEST_RPC_SIZE] = {0};

    CHECK(length <= sizeof(frame));
    frame[0] = command;
    frame[1] = 0u;
    frame[2] = 0u;
    frame[3] = payload_size;
    for (uint8_t index = 4u; index < sizeof(frame); index++) {
        frame[index] = index;
    }

    mirror_receive_frame(frame, length);
}

void transaction_register_rpc(int8_t transaction_id, slave_callback_t callback) {
    CHECK(transaction_id == PUT_VIA_KEYMAP_MIRROR);
    CHECK(callback != NULL);
    registered_callback = callback;
}

bool transaction_rpc_send(int8_t transaction_id, uint8_t size, const void *data) {
    (void)size;
    (void)data;
    CHECK(transaction_id == PUT_VIA_KEYMAP_MIRROR);
    return true;
}

bool is_keyboard_master(void) {
    return false;
}

void dynamic_keymap_set_keycode(uint8_t layer, uint8_t row, uint8_t column, uint16_t keycode) {
    (void)layer;
    (void)row;
    (void)column;
    (void)keycode;
}

void dynamic_keymap_set_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    CHECK(offset == 0u);
    CHECK(size <= sizeof(keymap_sink));
    keymap_write_count++;
    memcpy(keymap_sink, data, size);
}

void dynamic_keymap_reset(void) {}

void dynamic_keymap_macro_reset(void) {}

#ifdef ENCODER_MAP_ENABLE
void dynamic_keymap_set_encoder(uint8_t layer, uint8_t encoder_id, bool clockwise, uint16_t keycode) {
    (void)layer;
    (void)encoder_id;
    (void)clockwise;
    (void)keycode;
}
#endif

uint16_t dynamic_keymap_macro_get_buffer_size(void) {
    return 128u;
}

void dynamic_keymap_macro_set_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    CHECK(offset == 0u);
    CHECK(size <= sizeof(macro_sink));
    macro_write_count++;
    memcpy(macro_sink, data, size);
}

bool via_eeprom_is_valid(void) {
    return true;
}

void noah_rgb_runtime_invalidate_layer_maps(void) {
    rgb_invalidate_count++;
}

void via_macro_provider_invalidate_all(void) {
    macro_invalidate_count++;
}

void noah_qmk_via_split_sync_note_local_storage_changed(void) {
    storage_changed_count++;
}

static void test_maximum_valid_payload_is_applied_by_real_receiver(void) {
    test_reset();

    mirror_receive(id_dynamic_keymap_set_buffer, TEST_PAYLOAD_MAX, TEST_RPC_SIZE);
    CHECK(keymap_write_count == 1u);
    CHECK(macro_write_count == 0u);
    CHECK(rgb_invalidate_count == 1u);
    CHECK(macro_invalidate_count == 0u);
    CHECK(storage_changed_count == 1u);
    for (uint8_t index = 0u; index < TEST_PAYLOAD_MAX; index++) {
        CHECK(keymap_sink[index] == (uint8_t)(index + 4u));
    }

    mirror_receive(id_dynamic_keymap_macro_set_buffer, TEST_PAYLOAD_MAX, TEST_RPC_SIZE);
    CHECK(keymap_write_count == 1u);
    CHECK(macro_write_count == 1u);
    CHECK(rgb_invalidate_count == 1u);
    CHECK(macro_invalidate_count == 1u);
    CHECK(storage_changed_count == 2u);
    for (uint8_t index = 0u; index < TEST_PAYLOAD_MAX; index++) {
        CHECK(macro_sink[index] == (uint8_t)(index + 4u));
    }
}

static void test_callback_only_queues_and_scan_owns_storage(void) {
    uint8_t frame[TEST_RPC_SIZE] = {id_dynamic_keymap_set_buffer, 0u, 0u, 1u, 0xA6u};

    test_reset();
    mirror_enqueue_frame(frame, sizeof(frame));
    CHECK(keymap_write_count == 0u);
    CHECK(rgb_invalidate_count == 0u);
    CHECK(storage_changed_count == 0u);

    CHECK(noah_qmk_via_split_mirror_matrix_scan_step());
    CHECK(keymap_write_count == 1u);
    CHECK(keymap_sink[0] == 0xA6u);
    CHECK(rgb_invalidate_count == 1u);
    CHECK(storage_changed_count == 1u);
    CHECK(!noah_qmk_via_split_mirror_matrix_scan_step());
}

static void test_full_mailbox_keeps_first_best_effort_frame(void) {
    uint8_t keymap[TEST_RPC_SIZE] = {id_dynamic_keymap_set_buffer, 0u, 0u, 1u, 0xB1u};
    uint8_t macro[TEST_RPC_SIZE]  = {id_dynamic_keymap_macro_set_buffer, 0u, 0u, 1u, 0xB2u};

    test_reset();
    mirror_enqueue_frame(keymap, sizeof(keymap));
    mirror_enqueue_frame(macro, sizeof(macro));
    CHECK(noah_qmk_via_split_mirror_matrix_scan_step());
    CHECK(keymap_write_count == 1u);
    CHECK(macro_write_count == 0u);
    CHECK(keymap_sink[0] == 0xB1u);
    CHECK(!noah_qmk_via_split_mirror_matrix_scan_step());
}

static void test_one_byte_over_payload_limit_is_rejected(void) {
    test_reset();

    mirror_receive(id_dynamic_keymap_set_buffer, TEST_PAYLOAD_MAX + 1u, TEST_RPC_SIZE);
    mirror_receive(id_dynamic_keymap_macro_set_buffer, TEST_PAYLOAD_MAX + 1u, TEST_RPC_SIZE);

    CHECK(keymap_write_count == 0u);
    CHECK(macro_write_count == 0u);
    CHECK(rgb_invalidate_count == 0u);
    CHECK(macro_invalidate_count == 0u);
    CHECK(storage_changed_count == 0u);
}

static void test_wrapping_payload_sizes_are_rejected_without_touching_storage(void) {
    for (uint16_t size = 252u; size <= UINT8_MAX; size++) {
        test_reset();

        mirror_receive(id_dynamic_keymap_set_buffer, (uint8_t)size, TEST_RPC_SIZE);
        mirror_receive(id_dynamic_keymap_macro_set_buffer, (uint8_t)size, TEST_RPC_SIZE);

        CHECK(keymap_write_count == 0u);
        CHECK(macro_write_count == 0u);
        CHECK(rgb_invalidate_count == 0u);
        CHECK(macro_invalidate_count == 0u);
        CHECK(storage_changed_count == 0u);
        for (uint16_t index = 0u; index < TEST_SINK_SIZE; index++) {
            CHECK(keymap_sink[index] == 0xA5u);
            CHECK(macro_sink[index] == 0x5Au);
        }
    }
}

// Compile the production classifier and receiver into one test and exercise
// every command id. Any classifier path that advertises write-through must be
// accepted by the actual registered receiver and restart its storage digest.
static void test_every_classified_mirror_command_is_implemented(void) {
    for (uint16_t command = 0u; command <= UINT8_MAX; command++) {
        uint8_t frame[TEST_RPC_SIZE] = {0};
        uint8_t effects             = NOAH_QMK_VIA_COMMAND_EFFECT_NONE;

        frame[0] = (uint8_t)command;
        if (frame[0] == id_set_keyboard_value) {
            frame[1] = id_layout_options;
        }

        if (!noah_qmk_via_classify_mutation(frame, sizeof(frame), &effects) || !(effects & NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR)) {
            continue;
        }

        test_reset();
        mirror_receive_frame(frame, sizeof(frame));
        CHECK(storage_changed_count == 1u);
    }
}

static void test_durable_only_commands_are_not_advertised_as_mirrored(void) {
    uint8_t layout[] = {id_set_keyboard_value, id_layout_options, 0u, 0u, 0u, 1u};
    uint8_t reset[]  = {id_eeprom_reset};
    uint8_t effects  = NOAH_QMK_VIA_COMMAND_EFFECT_NONE;

    CHECK(noah_qmk_via_classify_mutation(layout, sizeof(layout), &effects));
    CHECK((effects & NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR) == 0u);

    effects = NOAH_QMK_VIA_COMMAND_EFFECT_NONE;
    CHECK(noah_qmk_via_classify_mutation(reset, sizeof(reset), &effects));
    CHECK((effects & NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR) == 0u);
}

int main(void) {
    test_maximum_valid_payload_is_applied_by_real_receiver();
    test_callback_only_queues_and_scan_owns_storage();
    test_full_mailbox_keeps_first_best_effort_frame();
    test_one_byte_over_payload_limit_is_rejected();
    test_wrapping_payload_sizes_are_rejected_without_touching_storage();
    test_every_classified_mirror_command_is_implemented();
    test_durable_only_commands_are_not_advertised_as_mirrored();
    puts("qmk via split mirror tests passed");
    return 0;
}
