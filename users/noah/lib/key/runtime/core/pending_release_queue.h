#pragma once

#include "runtime.h"

bool key_runtime_core_queue_pending_release_dispatch_for_owner(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods, bool tap_commit_feedback, uint16_t owner_token_id);
