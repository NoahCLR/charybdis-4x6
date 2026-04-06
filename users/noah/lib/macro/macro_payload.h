#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef bool (*macro_payload_write_byte_fn)(uint8_t byte, void *context);

bool macro_payload_validate(const char *payload);
bool macro_payload_play(const char *payload);
bool macro_payload_encode(const char *payload, uint8_t *buffer, uint16_t capacity, uint16_t *written);
bool macro_payload_encode_write(const char *payload, macro_payload_write_byte_fn write_byte, void *context, uint16_t *written);
