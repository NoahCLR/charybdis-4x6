# Shared Noah userspace runtime and build settings.
#
# Loaded automatically for keymaps named `noah`. This userspace now owns the
# full ruleset for the current Charybdis keymap, including feature toggles and
# reusable runtime sources.

# VIA support: enables runtime key remapping via the VIA desktop app.
VIA_ENABLE = yes

# Key combos: press multiple keys simultaneously to trigger an action.
COMBO_ENABLE = yes

# Link-time optimization: reduces binary size.
LTO_ENABLE = yes

SRC += runtime_init.c
SRC += hooks.c
SRC += synthetic_record.c
SRC += lib/split_role.c
SRC += lib/key/key_behavior_lookup.c
SRC += lib/key/key_runtime.c
SRC += lib/key/key_runtime_process.c
SRC += lib/key/key_runtime_scan.c
SRC += lib/key/delayed_action.c
SRC += lib/key/held_action.c
SRC += lib/action/action_dispatch.c
SRC += lib/action/macro_dispatch.c
SRC += lib/macro/macro_payload.c
SRC += lib/key/multi_tap_engine.c
SRC += lib/state/keyboard_mod_state.c

ifeq ($(strip $(POINTING_DEVICE_ENABLE)), yes)
    SRC += lib/pointing/pointing_device_runtime.c
    SRC += lib/pointing/pd_mode_key_runtime.c
    SRC += lib/pointing/pd_mode_state.c
    SRC += lib/pointing/pd_mode_registry.c
    SRC += lib/pointing/pointer_layer_policy.c
    SRC += lib/pointing/pointing_device_mode_handlers.c
    SRC += lib/state/pd_shared_state.c
endif

ifeq ($(strip $(RGB_MATRIX_ENABLE)), yes)
    SRC += lib/rgb/rgb_runtime.c
    SRC += lib/rgb/rgb_automouse.c
endif

# Split role override: build with FORCE_MASTER=yes or FORCE_SLAVE=yes
# to force a specific half's role when both have USB connected.
ifdef FORCE_MASTER
    OPT_DEFS += -DFORCE_MASTER
endif
ifdef FORCE_SLAVE
    OPT_DEFS += -DFORCE_SLAVE
endif
