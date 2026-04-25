#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifndef RPC_M2S_BUFFER_SIZE
#    define RPC_M2S_BUFFER_SIZE 32
#endif

typedef void (*slave_callback_t)(uint8_t initiator2target_buffer_size, const void *initiator2target_buffer, uint8_t target2initiator_buffer_size, void *target2initiator_buffer);

enum {
    PUT_SPLIT_RUNTIME_BASE_SYNC           = 1,
    PUT_SPLIT_COMBO_FEEDBACK_SYNC         = 2,
    PUT_SPLIT_KEY_FEEDBACK_SEMANTIC_SYNC  = 3,
    PUT_SPLIT_KEY_FEEDBACK_BRANCH_SYNC    = 4,
    PUT_VIA_KEYMAP_SYNC                   = 5,
};

void transaction_register_rpc(int8_t transaction_id, slave_callback_t callback);
bool transaction_rpc_send(int8_t transaction_id, uint8_t initiator2target_buffer_size, const void *initiator2target_buffer);
