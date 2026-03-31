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

SRC += noah.c
SRC += lib/key/key_runtime.c
SRC += lib/key/action_router.c
SRC += lib/key/multi_tap_engine.c

ifeq ($(strip $(POINTING_DEVICE_ENABLE)), yes)
    SRC += lib/pointing/pointing_device_runtime.c
    SRC += lib/pointing/pd_mode_key_runtime.c
    SRC += lib/pointing/pointer_layer_policy.c
    SRC += lib/pointing/pointing_device_modes.c
    SRC += lib/pointing/pointing_device_mode_handlers.c
    SRC += lib/pointing/split_sync.c
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
