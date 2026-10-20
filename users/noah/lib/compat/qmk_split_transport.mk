# Both halves use 460,800 baud with frame CRC on by default (D-L43).
# Keep one fixed speed: an old command using the removed selector must fail.
OPT_DEFS += -DSERIAL_USART_SPEED=460800
ifneq ($(strip $(NOAH_SPLIT_BAUD)),)
    $(error NOAH_SPLIT_BAUD was removed: the split link is fixed at 460800 baud (D-L43))
endif

# Activity coalescing is on by default and needs the fork's activity hook
# (noah-userspace-contracts at 6889960271 or later). NOAH_SPLIT_ACTIVITY_COALESCE=no builds the
# uncoalesced comparison firmware.
NOAH_SPLIT_ACTIVITY_COALESCE_SELECTED := $(or $(strip $(NOAH_SPLIT_ACTIVITY_COALESCE)),yes)
ifeq ($(NOAH_SPLIT_ACTIVITY_COALESCE_SELECTED),yes)
    OPT_DEFS += -DNOAH_SPLIT_ACTIVITY_COALESCE_ENABLE
else ifneq ($(NOAH_SPLIT_ACTIVITY_COALESCE_SELECTED),no)
    $(error NOAH_SPLIT_ACTIVITY_COALESCE must be yes or no)
endif

# Every split data frame carries a CRC8 by default, which needs the fork's
# frame CRC (noah-userspace-contracts at f4f77a2aaf or later, QMK_SPLIT_TRANSPORT_CRC_VERSION 1). Both halves must match: a
# mixed pair fails at the handshake. NOAH_SPLIT_CRC=no builds the comparison
# firmware without it.
NOAH_SPLIT_CRC_SELECTED := $(or $(strip $(NOAH_SPLIT_CRC)),yes)
ifeq ($(NOAH_SPLIT_CRC_SELECTED),yes)
    OPT_DEFS += -DSPLIT_TRANSPORT_CRC
else ifneq ($(NOAH_SPLIT_CRC_SELECTED),no)
    $(error NOAH_SPLIT_CRC must be yes or no)
endif

ifneq ($(strip $(NOAH_SPLIT_DIAGNOSTICS)),)
    ifeq ($(strip $(NOAH_SPLIT_DIAGNOSTICS)),yes)
        OPT_DEFS += -DSPLIT_TRANSACTION_DIAGNOSTICS
    else ifneq ($(strip $(NOAH_SPLIT_DIAGNOSTICS)),no)
        $(error NOAH_SPLIT_DIAGNOSTICS must be yes or no)
    endif
endif
