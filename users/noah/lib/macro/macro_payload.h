#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef bool (*macro_payload_write_byte_fn)(uint8_t byte, void *context);
typedef bool (*macro_payload_read_byte_fn)(uint16_t offset, uint8_t *byte, void *context);

#define MACRO_PAYLOAD_IR_MAX_BYTES 512

typedef enum {
    MACRO_PAYLOAD_IR_OP_TEXT = 1,
    MACRO_PAYLOAD_IR_OP_DELAY,
    MACRO_PAYLOAD_IR_OP_KEY_DOWN,
    MACRO_PAYLOAD_IR_OP_KEY_UP,
    MACRO_PAYLOAD_IR_OP_TAP_LIST,
} macro_payload_ir_opcode_t;

typedef enum {
    MACRO_PAYLOAD_TEXT_OUTPUT_PLAIN = 0,
    MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED,
} macro_payload_text_output_t;

typedef struct {
    uint16_t length;
    uint8_t  bytes[MACRO_PAYLOAD_IR_MAX_BYTES];
} macro_payload_ir_t;

typedef enum {
    MACRO_PAYLOAD_SOURCE_DIRECT = 0,
    MACRO_PAYLOAD_SOURCE_HARDCODED,
    MACRO_PAYLOAD_SOURCE_VIA,
} macro_payload_source_t;

typedef enum {
    MACRO_PAYLOAD_START_STARTED = 0,
    MACRO_PAYLOAD_START_BUSY,
    MACRO_PAYLOAD_START_INVALID,
    MACRO_PAYLOAD_START_EMPTY,
} macro_payload_start_result_t;

typedef enum {
    MACRO_PAYLOAD_FINISH_NONE = 0,
    MACRO_PAYLOAD_FINISH_SUCCESS,
    MACRO_PAYLOAD_FINISH_CANCELLED,
    MACRO_PAYLOAD_FINISH_RUNTIME_ERROR,
} macro_payload_finish_result_t;

typedef enum {
    MACRO_PAYLOAD_ENGINE_IDLE = 0,
    MACRO_PAYLOAD_ENGINE_READY,
    MACRO_PAYLOAD_ENGINE_WAITING,
    MACRO_PAYLOAD_ENGINE_OUTPUT,
    MACRO_PAYLOAD_ENGINE_CLEANUP,
} macro_payload_engine_state_t;

typedef void (*macro_payload_finish_fn)(macro_payload_finish_result_t result, void *context);

typedef struct {
    macro_payload_engine_state_t  state;
    macro_payload_finish_result_t last_finish;
    macro_payload_source_t        active_source;
    uint8_t                       active_slot;
    uint8_t                       active_hold_count;
    uint8_t                       active_hold_high_water;
    uint32_t                      operations_executed;
    uint16_t                      completed_count;
    uint16_t                      busy_rejection_count;
    uint16_t                      cancellation_count;
    uint16_t                      runtime_error_count;
    uint32_t                      maximum_lateness_ms;
} macro_payload_debug_snapshot_t;

bool                         macro_payload_validate(const char *payload);
bool                         macro_payload_compile(const char *payload, macro_payload_ir_t *ir);
macro_payload_start_result_t macro_payload_start_ir(const macro_payload_ir_t *ir, macro_payload_text_output_t text_output, uint8_t interval, macro_payload_source_t source, uint8_t slot, macro_payload_finish_fn finish, void *context);
void                         macro_payload_engine_scan(void);
bool                         macro_payload_engine_cancel(void);
void                         macro_payload_engine_init(void);
void                         macro_payload_debug_snapshot(macro_payload_debug_snapshot_t *out);
bool                         macro_payload_decode_qmk_stream(macro_payload_ir_t *ir, uint16_t length, macro_payload_read_byte_fn read_byte, void *context);
bool                         macro_payload_encode_ir(const macro_payload_ir_t *ir, uint8_t *buffer, uint16_t capacity, uint16_t *written);
bool                         macro_payload_encode_ir_write(const macro_payload_ir_t *ir, macro_payload_write_byte_fn write_byte, void *context, uint16_t *written);
bool                         macro_payload_encode(const char *payload, uint8_t *buffer, uint16_t capacity, uint16_t *written);
bool                         macro_payload_encode_write(const char *payload, macro_payload_write_byte_fn write_byte, void *context, uint16_t *written);
