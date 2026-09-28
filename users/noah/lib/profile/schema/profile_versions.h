#pragma once

// Settings versions a schema-2 (PD) profile may carry. It is the one list:
// the validator accepts them and the store's shape check stores them, so the
// two cannot drift (the store once refused v4 after the validator passed it).
#define NOAH_PROFILE_PD_SETTINGS_VERSION_ACCEPTED(version) ((version) >= 2u && (version) <= 5u)

// Schema-1 constants remain for codec compatibility tests. Firmware builds
// always define NOAH_PD_PROFILE_ENABLE and cannot select the old geometry.
#ifdef NOAH_PD_PROFILE_ENABLE
#    define NOAH_PROFILE_SCHEMA_MAJOR 2u
#    define NOAH_PROFILE_PAYLOAD_MAX 5088u
#    define NOAH_PROFILE_DOMAIN_COUNT 5u
#    define NOAH_PROFILE_PD_COUNT 8u
#    define NOAH_PROFILE_RGB_VERSION 2u
// Settings v3 names the 64 VIA macros where v2 carried 16 user macros; v4
// guarantees every name 20 ASCII characters; v5 also names the 64 custom keys.
// v2–v4 stay readable so a stored profile survives the firmware that writes v5.
#    define NOAH_PROFILE_SETTINGS_VERSION 5u
#    define NOAH_PROFILE_SETTINGS_VERSION_ACCEPTED(version) NOAH_PROFILE_PD_SETTINGS_VERSION_ACCEPTED(version)
#    define NOAH_PROFILE_LOGICAL_STORE_VERSION 3u
#else
#    define NOAH_PROFILE_SCHEMA_MAJOR 1u
#    define NOAH_PROFILE_PAYLOAD_MAX 4064u
#    define NOAH_PROFILE_DOMAIN_COUNT 4u
#    define NOAH_PROFILE_PD_COUNT 6u
#    define NOAH_PROFILE_RGB_VERSION 1u
#    define NOAH_PROFILE_SETTINGS_VERSION 1u
#    define NOAH_PROFILE_SETTINGS_VERSION_ACCEPTED(version) ((version) == 1u)
#    define NOAH_PROFILE_LOGICAL_STORE_VERSION 2u
#endif
