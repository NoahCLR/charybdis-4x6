#pragma once

#include "../reducer/runtime.h"

uint8_t key_runtime_core_pending_release_count(void);
bool    key_runtime_core_queue_pending_release_dispatch(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods, bool tap_commit_feedback);
bool    key_runtime_core_queue_pending_release_dispatch_for_owner(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods, bool tap_commit_feedback, uint16_t owner_token_id);
bool    key_runtime_core_pending_release_at_order(uint8_t order, pending_release_t *out);
uint8_t key_runtime_core_take_pending_release_dispatches(pending_release_t *out, uint8_t capacity);
uint8_t key_runtime_core_pending_release_count_for_keypos(keypos_t key_pos);
void    key_runtime_core_observe_release_dispatch_deferred(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods);
void    key_runtime_core_observe_release_dispatch_drained(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods);

#ifdef NOAH_HOST_TEST_ENV
bool key_runtime_core_pending_release_validate(void);
#endif
