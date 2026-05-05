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

bool macro_payload_validate(const char *payload);
bool macro_payload_play(const char *payload);
bool macro_payload_compile(const char *payload, macro_payload_ir_t *ir);
bool macro_payload_play_ir(const macro_payload_ir_t *ir);
bool macro_payload_play_ir_with_text_output(const macro_payload_ir_t *ir, macro_payload_text_output_t text_output, uint8_t interval);
bool macro_payload_decode_qmk_stream(macro_payload_ir_t *ir, uint16_t length, macro_payload_read_byte_fn read_byte, void *context);
bool macro_payload_encode_ir(const macro_payload_ir_t *ir, uint8_t *buffer, uint16_t capacity, uint16_t *written);
bool macro_payload_encode_ir_write(const macro_payload_ir_t *ir, macro_payload_write_byte_fn write_byte, void *context, uint16_t *written);
bool macro_payload_encode(const char *payload, uint8_t *buffer, uint16_t capacity, uint16_t *written);
bool macro_payload_encode_write(const char *payload, macro_payload_write_byte_fn write_byte, void *context, uint16_t *written);
