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
    OPT_DEFS += -DNOAH_STACK_BUDGET_ENABLE
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
# to force a specific half's role when both have USB connected. On this
# MASTER_RIGHT board, right is FORCE_MASTER and left is FORCE_SLAVE. These
# flags describe transport role, not a generic durable physical-origin source.
ifdef FORCE_MASTER
    OPT_DEFS += -DFORCE_MASTER
endif
ifdef FORCE_SLAVE
    OPT_DEFS += -DFORCE_SLAVE
endif

# Stable physical-half provisioning for side-specific firmware artifacts.
# This is deliberately independent of FORCE_MASTER/FORCE_SLAVE: those flags
# select the current transport role, while this value is durable profile
# origin and QMK handedness embedded in flash.
ifneq ($(strip $(NOAH_PHYSICAL_HALF)),)
    ifeq ($(strip $(NOAH_PHYSICAL_HALF)),left)
        OPT_DEFS += -DNOAH_PHYSICAL_HALF_LEFT
    else ifeq ($(strip $(NOAH_PHYSICAL_HALF)),right)
        OPT_DEFS += -DNOAH_PHYSICAL_HALF_RIGHT
    else
        $(error NOAH_PHYSICAL_HALF must be `left` or `right`)
    endif
endif

# The live-profile owner is part of ordinary firmware. The live editing app
# talks to it, so it is no longer a deliberate engineering artifact.
#
# It requires a provisioned physical half, because durable profile origin
# identity is side-specific. The generic half-less convenience build therefore
# cannot carry it and says so rather than failing; flash the side-specific pair
# to get the owner.
#
# NOAH_LIVE_PROFILE_OWNER=no builds an owner-free image from the same source.
# That is the only remaining lever for comparing ordinary against live-profile
# behaviour without changing branches, which matters while the pointing cadence
# regression in docs/architecture/pointing-cadence-known-issue.md is unresolved.
NOAH_LIVE_PROFILE_OWNER ?= yes
ifneq ($(strip $(NOAH_LIVE_PROFILE_OWNER)),no)
    ifneq ($(strip $(NOAH_LIVE_PROFILE_OWNER)),yes)
        $(error NOAH_LIVE_PROFILE_OWNER must be `yes` or `no`)
    endif
    ifneq ($(strip $(NOAH_PHYSICAL_HALF)),)
        ifneq ($(strip $(VIA_ENABLE)),yes)
            $(error NOAH_LIVE_PROFILE_OWNER=yes requires VIA_ENABLE=yes)
        endif
        OPT_DEFS += -DNOAH_LIVE_PROFILE_OWNER_ENABLE
        SRC += $(NOAH_LIVE_PROFILE_OWNER_SOURCES)
    else
        $(info noah: live-profile owner omitted; NOAH_PHYSICAL_HALF is not provisioned)
    endif
endif

# Candidate routing and advertised write capabilities are one stricter switch
# layered on the complete owner. Keeping the define behind this build-time
# dependency prevents a firmware image from advertising mutation without the
# owner that receives it, or from accepting hidden writes while advertising a
# read-only channel.
# The mutation route is the write path the live editing app needs, so it
# follows the owner rather than being separately opted into. It cannot outlive
# the owner: without one there is nothing to route a candidate into.
NOAH_LIVE_PROFILE_MUTATION ?= yes
ifneq ($(strip $(NOAH_LIVE_PROFILE_MUTATION)),no)
    ifneq ($(strip $(NOAH_LIVE_PROFILE_MUTATION)),yes)
        $(error NOAH_LIVE_PROFILE_MUTATION must be `yes` or `no`)
    endif
    ifeq ($(strip $(NOAH_LIVE_PROFILE_OWNER)),no)
        $(error NOAH_LIVE_PROFILE_MUTATION=yes requires NOAH_LIVE_PROFILE_OWNER=yes)
    endif
    ifneq ($(strip $(NOAH_PHYSICAL_HALF)),)
        OPT_DEFS += -DNOAH_LIVE_PROFILE_MUTATION_ENABLE
    endif
endif

# Temporary, read-only on-device cadence recorder used to compare ordinary and
# live-profile firmware from the same source revision. The recorder is absent
# from normal artifacts and requires VIA for bounded post-capture readback.
ifneq ($(strip $(NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS)),)
    ifneq ($(strip $(NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS)),yes)
        $(error NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS must be `yes` when specified)
    endif
    ifneq ($(strip $(VIA_ENABLE)),yes)
        $(error NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS=yes requires VIA_ENABLE=yes)
    endif
    OPT_DEFS += -DNOAH_PROFILE_PERFORMANCE_DIAGNOSTICS_ENABLE
endif
