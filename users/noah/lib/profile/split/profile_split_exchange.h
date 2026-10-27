// ──────────────────────────────────────────────────────────────────────────
// One Request To The Other Half
// ──────────────────────────────────────────────────────────────────────────
//
// The seam both of the profile's request/reply protocols send through: the
// durable reconciler (PUT_PROFILE_SPLIT_SYNC) and the working copy's sync
// (PUT_PROFILE_WORKING_SYNC). The production adapters are QMK's split RPC
// (compat/qmk_profile_split_transport.c); host tests run the in-memory link in
// tests/host/split_fault_link.c.
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Sends request_length bytes and reads the reply into response_length bytes,
// the reply buffer the protocol's request asks for.
// - True when a reply came back. It may still be stale (QMK's buffer when the
//   other half ran no callback), zeros (a request the other half refused) or
//   garbled, so the caller checks its own framing (CRC and correlation).
// - False when no reply came back. The request may still have reached the
//   other half, so the caller sends the same request again (split framing:
//   byte for byte; working-copy sync: the same kind and sequence).
// Calling context: the scan's durable-I/O step only, at most one exchange per
// step, blocking for one QMK RPC. Never from an RPC callback.
typedef bool (*noah_profile_split_exchange_fn)(void *context, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length);
