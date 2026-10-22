# The image every pair is built and every host test runs in (D-F05): QMK's
# official qmk_cli image, with two changes.
#
# QMK builds its toolchain separately for each host. The linux/arm64 variant's
# ARM target libraries (newlib, libgcc, startup files, linker scripts) differ
# from the linux/amd64 variant's, so a Mac building natively linked different
# code from CI. Here both variants carry the amd64 variant's target files; each
# keeps its own native compiler programs, which generate the same code.
#
# The host suite also calls ripgrep (the source guards) and Node (the
# diagnostics tools' tests). They are the projects' own release binaries,
# pinned by SHA-256, so the image builds the same from this file on any day,
# needs no package manager, and adds two files that nothing in a pair build
# reads. To move to a newer QMK image, change only QMK_CLI. To change a tool,
# set its version and both checksums from the project's published sums:
#   https://nodejs.org/dist/v<version>/SHASUMS256.txt (linux-x64, linux-arm64 .tar.xz)
#   https://github.com/BurntSushi/ripgrep/releases/tag/<version> (the .sha256
#   beside x86_64- and aarch64-unknown-linux-musl .tar.gz, statically linked)
#
# Build and publish it with tools/make-build-image.sh, never by hand.

ARG QMK_CLI=ghcr.io/qmk/qmk_cli@sha256:b7d7fa8fb4432b569931de5ad59098cb788f440ed61a62c5126746b71aee0f4a
ARG NODE_VERSION=24.22.1
ARG NODE_SHA256_AMD64=6c172a9ab6718f7b801fb5a7bd486af2ee5e32758086a04b84fcce2aa77095e6
ARG NODE_SHA256_ARM64=dbabf7a1cbf947e6059ffad2c27b80041db97dea1e553b62afdee7f13fca336e
ARG RIPGREP_VERSION=15.2.0
ARG RIPGREP_SHA256_AMD64=33e15bcf1624b25cdd2a55813a47a2f95dbe126268203e76aa6a585d1e7b149c
ARG RIPGREP_SHA256_ARM64=800b1e7206afe799dfb5a6901f23147cfaabe0e52210538100f61e86e1740915

FROM --platform=linux/amd64 ${QMK_CLI} AS target

# Downloads run once on the building machine, for each target platform.
FROM --platform=$BUILDPLATFORM ${QMK_CLI} AS tools
ARG TARGETARCH NODE_VERSION NODE_SHA256_AMD64 NODE_SHA256_ARM64 RIPGREP_VERSION RIPGREP_SHA256_AMD64 RIPGREP_SHA256_ARM64
RUN set -eu; \
    case "$TARGETARCH" in \
        amd64) node=x64; node_sum=$NODE_SHA256_AMD64; rg=x86_64; rg_sum=$RIPGREP_SHA256_AMD64 ;; \
        arm64) node=arm64; node_sum=$NODE_SHA256_ARM64; rg=aarch64; rg_sum=$RIPGREP_SHA256_ARM64 ;; \
        *) echo "no tools for $TARGETARCH" >&2; exit 1 ;; \
    esac; \
    node_dir="node-v$NODE_VERSION-linux-$node"; rg_dir="ripgrep-$RIPGREP_VERSION-$rg-unknown-linux-musl"; \
    cd /tmp; \
    curl -fsSLo node.tar.xz "https://nodejs.org/dist/v$NODE_VERSION/$node_dir.tar.xz"; \
    curl -fsSLo rg.tar.gz "https://github.com/BurntSushi/ripgrep/releases/download/$RIPGREP_VERSION/$rg_dir.tar.gz"; \
    printf '%s  node.tar.xz\n%s  rg.tar.gz\n' "$node_sum" "$rg_sum" | sha256sum -c -; \
    mkdir /out; \
    tar -xJf node.tar.xz -C /out --strip-components=2 "$node_dir/bin/node"; \
    tar -xzf rg.tar.gz -C /out --strip-components=1 "$rg_dir/rg"

FROM ${QMK_CLI}
# Only ARM target code and headers: none of these hold a host program.
COPY --from=target /opt/qmk/arm-none-eabi/lib /opt/qmk/arm-none-eabi/lib
COPY --from=target /opt/qmk/arm-none-eabi/include /opt/qmk/arm-none-eabi/include
COPY --from=target /opt/qmk/arm-none-eabi/sys-include /opt/qmk/arm-none-eabi/sys-include
COPY --from=target /opt/qmk/lib/gcc/arm-none-eabi /opt/qmk/lib/gcc/arm-none-eabi
COPY --from=target /opt/qmk/newlib-nano /opt/qmk/newlib-nano
COPY --from=tools --chown=0:0 /out/node /out/rg /usr/local/bin/
