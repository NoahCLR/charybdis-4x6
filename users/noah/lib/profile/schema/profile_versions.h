#pragma once
#include "profile_domain_registry.h"

// The blob validator and persistent shape check share the versions firmware
// writes. Older formats are rejected, never migrated by firmware. Schema 4,
// store format 6 and Profile Wire 2 open the new base (D-F28): every length
// and offset inside a profile is 32 bits wide.
#define NOAH_PROFILE_SCHEMA_MAJOR 4u
// A 128 KiB profile slot less the first kilobyte its header and commit
// marker take on flash (D-F29).
#define NOAH_PROFILE_PAYLOAD_MAX 130048u
#define NOAH_PROFILE_DOMAIN_COUNT NOAH_PROFILE_DOMAIN_REGISTRY_COUNT
#define NOAH_PROFILE_PD_COUNT 32u
#define NOAH_PROFILE_PD_VERSION NOAH_PROFILE_DOMAIN_VERSION_PD
#define NOAH_PROFILE_RGB_VERSION NOAH_PROFILE_DOMAIN_VERSION_RGB
#define NOAH_PROFILE_SETTINGS_VERSION NOAH_PROFILE_DOMAIN_VERSION_SETTINGS
#define NOAH_PROFILE_LOGICAL_STORE_VERSION 6u
// The one supported tap depth (D-F14): a behaviour row holds at most this many
// steps, the whole table rows × depth, and RGB depth - 1 extra tap-branch
// colours. Formats are counted, so a later firmware can raise it; a profile
// alone cannot.
#ifdef KEY_BEHAVIOR_MAX_TAP_COUNT
#    define NOAH_PROFILE_TAP_DEPTH KEY_BEHAVIOR_MAX_TAP_COUNT
#else
#    define NOAH_PROFILE_TAP_DEPTH 5u
#endif
#define NOAH_PROFILE_BEHAVIOR_ROWS 128u
