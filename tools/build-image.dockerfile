# The image every pair is built in (D-F05): QMK's official qmk_cli image, with
# one change so both of its platforms build identical firmware.
#
# QMK builds its toolchain separately for each host. The linux/arm64 variant's
# ARM target libraries (newlib, libgcc, startup files, linker scripts) differ
# from the linux/amd64 variant's, so a Mac building natively linked different
# code from CI. Here both variants carry the amd64 variant's target files; each
# keeps its own native compiler programs, which generate the same code.
#
# Build and publish it with tools/make-build-image.sh, never by hand.

ARG QMK_CLI=ghcr.io/qmk/qmk_cli@sha256:b7d7fa8fb4432b569931de5ad59098cb788f440ed61a62c5126746b71aee0f4a

FROM --platform=linux/amd64 ${QMK_CLI} AS target

FROM ${QMK_CLI}
# Only ARM target code and headers: none of these hold a host program.
COPY --from=target /opt/qmk/arm-none-eabi/lib /opt/qmk/arm-none-eabi/lib
COPY --from=target /opt/qmk/arm-none-eabi/include /opt/qmk/arm-none-eabi/include
COPY --from=target /opt/qmk/arm-none-eabi/sys-include /opt/qmk/arm-none-eabi/sys-include
COPY --from=target /opt/qmk/lib/gcc/arm-none-eabi /opt/qmk/lib/gcc/arm-none-eabi
COPY --from=target /opt/qmk/newlib-nano /opt/qmk/newlib-nano
