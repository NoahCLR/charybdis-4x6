# Optional paired-firmware experiment. Leave QMK's transport default alone
# unless explicitly selected; both halves must use the same baud rate.
ifneq ($(strip $(NOAH_SPLIT_BAUD)),)
    ifeq ($(filter $(strip $(NOAH_SPLIT_BAUD)),230400 460800),)
        $(error NOAH_SPLIT_BAUD must be 230400 or 460800)
    endif
    ifneq ($(words $(NOAH_SPLIT_BAUD)),1)
        $(error NOAH_SPLIT_BAUD must be a single baud rate)
    endif
    OPT_DEFS += -DSERIAL_USART_SPEED=$(strip $(NOAH_SPLIT_BAUD))
endif

# Enabled explicitly for A/B until physical timing and RGB acceptance pass.
ifneq ($(strip $(NOAH_SPLIT_ACTIVITY_COALESCE)),)
    ifeq ($(strip $(NOAH_SPLIT_ACTIVITY_COALESCE)),yes)
        OPT_DEFS += -DNOAH_SPLIT_ACTIVITY_COALESCE_ENABLE
    else ifneq ($(strip $(NOAH_SPLIT_ACTIVITY_COALESCE)),no)
        $(error NOAH_SPLIT_ACTIVITY_COALESCE must be yes or no)
    endif
endif

ifneq ($(strip $(NOAH_SPLIT_DIAGNOSTICS)),)
    ifeq ($(strip $(NOAH_SPLIT_DIAGNOSTICS)),yes)
        OPT_DEFS += -DSPLIT_TRANSACTION_DIAGNOSTICS
    else ifneq ($(strip $(NOAH_SPLIT_DIAGNOSTICS)),no)
        $(error NOAH_SPLIT_DIAGNOSTICS must be yes or no)
    endif
endif
