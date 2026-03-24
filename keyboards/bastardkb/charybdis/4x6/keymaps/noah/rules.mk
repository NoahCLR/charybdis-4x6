# VIA support: enables runtime key remapping via the VIA desktop app.
VIA_ENABLE = yes

# Link-time optimization: reduces binary size.
LTO_ENABLE = yes

# Split role override: build with FORCE_MASTER=yes or FORCE_SLAVE=yes
# to force a specific half's role when both have USB connected.
ifdef FORCE_MASTER
    OPT_DEFS += -DFORCE_MASTER
endif
ifdef FORCE_SLAVE
    OPT_DEFS += -DFORCE_SLAVE
endif
