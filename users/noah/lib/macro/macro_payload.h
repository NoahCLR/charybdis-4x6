#pragma once

#include <stdbool.h>
#include <stdint.h>

bool macro_payload_play(const char *payload);
bool macro_payload_encode(const char *payload, uint8_t *buffer, uint16_t capacity, uint16_t *written);
