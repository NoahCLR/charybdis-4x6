#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifndef RPC_M2S_BUFFER_SIZE
#    define RPC_M2S_BUFFER_SIZE 32
#endif

typedef void (*slave_callback_t)(uint8_t initiator2target_buffer_size, const void *initiator2target_buffer, uint8_t target2initiator_buffer_size, void *target2initiator_buffer);

enum {
    PUT_SPLIT_RUNTIME_SYNC = 1,
    PUT_VIA_KEYMAP_SYNC    = 2,
};

void transaction_register_rpc(int8_t transaction_id, slave_callback_t callback);
bool transaction_rpc_send(int8_t transaction_id, uint8_t initiator2target_buffer_size, const void *initiator2target_buffer);
