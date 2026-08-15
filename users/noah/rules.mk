# Shared Noah userspace runtime and build settings.
#
# Loaded automatically for keymaps named `noah`. This userspace now owns the
# full ruleset for the current Charybdis keymap, including feature toggles and
# reusable runtime sources.

NOAH_USERSPACE_ROOT := $(if $(QMK_USERSPACE),$(QMK_USERSPACE),$(abspath $(USER_PATH)/../..))
NOAH_PROFILE_VALIDATION_RESULT := $(shell log="$$(mktemp "$${TMPDIR:-/tmp}/noah-profile-validation.XXXXXX")"; cd "$(NOAH_USERSPACE_ROOT)" && sh tests/host/run_real_profile_validation_tests.sh "$(KEYMAP_PATH)" >"$$log" 2>&1; status=$$?; if [ $$status -ne 0 ]; then cat "$$log" >&2; echo failed; fi; rm -f "$$log")

ifeq ($(strip $(NOAH_PROFILE_VALIDATION_RESULT)),failed)
    $(error Noah authored profile validation failed; run `sh tests/host/run_real_profile_validation_tests.sh "$(KEYMAP_PATH)"`)
endif

include $(USER_PATH)/source_manifest.mk

# VIA support: enables runtime key remapping via the VIA desktop app.
VIA_ENABLE = yes

# Key combos: press multiple keys simultaneously to trigger an action.
COMBO_ENABLE = yes

# Link-time optimization: reduces binary size.
LTO_ENABLE = yes

# The reviewed target paths need more than the ChibiOS platform default while
# retaining a 25% (and at least 512-byte) process-stack reserve.
USE_PROCESS_STACKSIZE = 0xA00

# The reviewed-path stack gate uses final linked disassembly. Disabling GCC
# shrink wrapping keeps each analyzed frame allocation in the entry prologue.
ifeq ($(strip $(NOAH_STACK_BUDGET_ENABLE)), yes)
    CFLAGS += -fno-shrink-wrap
    CXXFLAGS += -fno-shrink-wrap
    EXTRALDFLAGS += -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref
endif

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
