#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef bool (*macro_payload_write_byte_fn)(uint8_t byte, void *context);

#define MACRO_PAYLOAD_IR_MAX_BYTES 128

typedef enum {
    MACRO_PAYLOAD_IR_OP_TEXT = 1,
    MACRO_PAYLOAD_IR_OP_DELAY,
    MACRO_PAYLOAD_IR_OP_KEY_DOWN,
    MACRO_PAYLOAD_IR_OP_KEY_UP,
    MACRO_PAYLOAD_IR_OP_TAP_LIST,
} macro_payload_ir_opcode_t;

typedef struct {
    uint16_t length;
    uint8_t  bytes[MACRO_PAYLOAD_IR_MAX_BYTES];
} macro_payload_ir_t;

bool macro_payload_validate(const char *payload);
bool macro_payload_play(const char *payload);
bool macro_payload_compile(const char *payload, macro_payload_ir_t *ir);
bool macro_payload_play_ir(const macro_payload_ir_t *ir);
bool macro_payload_encode(const char *payload, uint8_t *buffer, uint16_t capacity, uint16_t *written);
bool macro_payload_encode_write(const char *payload, macro_payload_write_byte_fn write_byte, void *context, uint16_t *written);
