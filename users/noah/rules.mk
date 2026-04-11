# Shared Noah userspace runtime and build settings.
#
# Loaded automatically for keymaps named `noah`. This userspace now owns the
# full ruleset for the current Charybdis keymap, including feature toggles and
# reusable runtime sources.

include $(USER_PATH)/source_manifest.mk

# VIA support: enables runtime key remapping via the VIA desktop app.
VIA_ENABLE = yes

# Key combos: press multiple keys simultaneously to trigger an action.
COMBO_ENABLE = yes

# Link-time optimization: reduces binary size.
LTO_ENABLE = yes

SRC += $(NOAH_COMMON_SOURCES)

ifeq ($(strip $(POINTING_DEVICE_ENABLE)), yes)
    SRC += $(NOAH_POINTING_SOURCES)
endif

ifeq ($(strip $(RGB_MATRIX_ENABLE)), yes)
    SRC += $(NOAH_AUTOMOUSE_SOURCES)
    SRC += $(NOAH_RGB_KEYMAP_SOURCES)
endif

# Split role override: build with FORCE_MASTER=yes or FORCE_SLAVE=yes
# to force a specific half's role when both have USB connected.
ifdef FORCE_MASTER
    OPT_DEFS += -DFORCE_MASTER
endif
ifdef FORCE_SLAVE
    OPT_DEFS += -DFORCE_SLAVE
endif
