# The split link runs at QMK's default speed. A faster link was tried and
# rejected: at 460,800 baud it garbled split messages (D-L43), so there is no
# speed selector, and an old build command that still sets one fails.
ifneq ($(strip $(NOAH_SPLIT_BAUD)),)
    $(error NOAH_SPLIT_BAUD was removed: the split link stays at QMK's default speed (D-L43))
endif

# Activity coalescing is on by default and needs the fork's activity hook
# (sol at 6889960271 or later). NOAH_SPLIT_ACTIVITY_COALESCE=no builds the
# uncoalesced comparison firmware.
NOAH_SPLIT_ACTIVITY_COALESCE_SELECTED := $(or $(strip $(NOAH_SPLIT_ACTIVITY_COALESCE)),yes)
ifeq ($(NOAH_SPLIT_ACTIVITY_COALESCE_SELECTED),yes)
    OPT_DEFS += -DNOAH_SPLIT_ACTIVITY_COALESCE_ENABLE
else ifneq ($(NOAH_SPLIT_ACTIVITY_COALESCE_SELECTED),no)
    $(error NOAH_SPLIT_ACTIVITY_COALESCE must be yes or no)
endif

ifneq ($(strip $(NOAH_SPLIT_DIAGNOSTICS)),)
    ifeq ($(strip $(NOAH_SPLIT_DIAGNOSTICS)),yes)
        OPT_DEFS += -DSPLIT_TRANSACTION_DIAGNOSTICS
    else ifneq ($(strip $(NOAH_SPLIT_DIAGNOSTICS)),no)
        $(error NOAH_SPLIT_DIAGNOSTICS must be yes or no)
    endif
endif
