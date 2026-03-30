# VIA support: enables runtime key remapping via the VIA desktop app.
VIA_ENABLE = yes

# Key combos: press multiple keys simultaneously to trigger an action.
COMBO_ENABLE = yes

# Link-time optimization: reduces binary size.
LTO_ENABLE = yes

# Build runtime as real modules instead of keeping the full implementation
# inside keymap.c.
SRC += lib/userspace_runtime.c
SRC += lib/key_runtime.c
SRC += lib/pointing_device_runtime.c
SRC += lib/rgb_runtime.c
SRC += lib/pointing_device_modes.c
SRC += lib/pointing_device_mode_handlers.c
SRC += lib/split_sync.c
SRC += lib/rgb_automouse.c

# Split role override: build with FORCE_MASTER=yes or FORCE_SLAVE=yes
# to force a specific half's role when both have USB connected.
ifdef FORCE_MASTER
    OPT_DEFS += -DFORCE_MASTER
endif
ifdef FORCE_SLAVE
    OPT_DEFS += -DFORCE_SLAVE
endif
