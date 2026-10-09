// Firmware's Profile Wire contract, stated from its own code: the capability
// pages this keyboard answers once its live-profile owner has opened the
// compiled defaults. The values come from the same header the VIA channel uses
// (qmk_via_profile_capabilities.h), the digests and domain mask from the same
// compiled-defaults code the owner runs, and the bytes from the same encoder.
// run_contract_probe.sh builds it with the production feature set and adds the
// BK pin and fixture hashes; the client's agreement check decodes the pages with
// its own runtime decoder. Prints one JSON object on stdout.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/profile/protocol/profile_wire_v1.h"
#include "users/noah/lib/profile/protocol/profile_candidate_v1.h"
#include "users/noah/lib/profile/storage/profile_storage_layout.h"
#include "users/noah/lib/profile/schema/profile_compiled_defaults_v1.h"
#include "users/noah/lib/profile/schema/profile_validator_v1.h"
#include "users/noah/lib/compat/qmk_via_profile_capabilities.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"

// The PD-mode table exactly as the pointing registry defines its rows
// (pd_mode_registry.c: mode flag, keycode, lock action), and its two lookups,
// without the registry's handlers: the action vocabulary and its digest read
// only these fields.
#define NOAH_PD_MODE_PROBE_ROW(name, mode_keycode) {.mode_flag = PD_MODE_##name, .keycode = mode_keycode, .lock_action = mode_keycode##_LOCK},
const pd_mode_def_t pd_modes[PD_MODE_COUNT] = {NOAH_PD_MODE_LIST(NOAH_PD_MODE_PROBE_ROW)};
#undef NOAH_PD_MODE_PROBE_ROW

bool is_pd_mode_lock_action(uint16_t action) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].lock_action == action) return true;
    }
    return false;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].keycode != KC_NO && pd_modes[i].keycode == keycode) return pd_modes[i].mode_flag;
    }
    return 0;
}

enum {
    FRAME_COMMAND    = 0u,
    FRAME_CHANNEL    = 1u,
    FRAME_VALUE      = 2u,
    FRAME_REQUEST_ID = 3u,
    FRAME_PAGE       = 4u,
    FRAME_STATUS     = 5u,
    FRAME_PAYLOAD    = 7u,
};

static int print_page(const noah_profile_wire_v1_read_service_t *service, uint8_t page) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];

    memset(frame, 0, sizeof(frame));
    frame[FRAME_COMMAND]    = NOAH_PROFILE_WIRE_V1_COMMAND_GET;
    frame[FRAME_CHANNEL]    = NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL;
    frame[FRAME_VALUE]      = NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY;
    frame[FRAME_REQUEST_ID] = 1u;
    frame[FRAME_PAGE]       = page;
    if (!noah_profile_wire_v1_handle_get(service, frame, sizeof(frame)) || frame[FRAME_STATUS] != NOAH_PROFILE_WIRE_V1_STATUS_OK) {
        fprintf(stderr, "contract probe: capability page %u was not answered\n", (unsigned)page);
        return 1;
    }
    printf("\"");
    for (uint8_t i = 0u; i < NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE; i++) {
        printf("%02x", (unsigned)frame[FRAME_PAYLOAD + i]);
    }
    printf("\"");
    return 0;
}

int main(void) {
    noah_profile_compiled_v1_t                compiled;
    noah_profile_compiled_v1_error_t          error;
    noah_profile_validator_v1_compatibility_t compatibility;
    noah_profile_wire_v1_read_service_t       service;

    if (noah_profile_compiled_v1_open(&compiled, &error) != NOAH_PROFILE_COMPILED_V1_OK) {
        fprintf(stderr, "contract probe: the compiled defaults do not open (surface %u, row %u)\n", (unsigned)error.surface, (unsigned)error.row);
        return 1;
    }
    if (!noah_profile_compiled_v1_compatibility(&compiled, &compatibility)) {
        fprintf(stderr, "contract probe: the compiled defaults have no compatibility declaration\n");
        return 1;
    }

    // What the channel reports after the owner's first status (see
    // noah_profile_channel_refresh_owner_capabilities).
    memset(&service, 0, sizeof(service));
    service.capabilities = (noah_profile_wire_v1_capabilities_t)NOAH_PROFILE_CHANNEL_BASE_CAPABILITIES;
    noah_profile_channel_apply_owner_capabilities(&service.capabilities, compiled.metadata.action_abi_digest, compiled.metadata.digest, compatibility.allowed_domain_mask);

    printf("{\"format\":1,\"actionAbiDigest\":\"0x%08lx\",\"compiledDefaultDigest\":\"0x%08lx\",\"supportedDomainMask\":%u,\"capabilityPages\":[", (unsigned long)compiled.metadata.action_abi_digest, (unsigned long)compiled.metadata.digest, (unsigned)compatibility.allowed_domain_mask);
    if (print_page(&service, 0u) != 0) {
        return 1;
    }
    printf(",");
    if (print_page(&service, 1u) != 0) {
        return 1;
    }
    printf(",");
    if (print_page(&service, 2u) != 0) {
        return 1;
    }
    printf("]}\n");
    return 0;
}
