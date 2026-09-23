#pragma once

// The bridge build retains the deployed six-mode schema and EEPROM geometry.
#ifdef NOAH_PD_PROFILE_ENABLE
#    define NOAH_PROFILE_SCHEMA_MAJOR 2u
#    define NOAH_PROFILE_PAYLOAD_MAX 5088u
#    define NOAH_PROFILE_DOMAIN_COUNT 5u
#    define NOAH_PROFILE_PD_COUNT 8u
#    define NOAH_PROFILE_RGB_VERSION 2u
// Settings v3 names the 64 VIA macros where v2 carried 16 user macros. v2
// stays readable so a stored profile survives the firmware that writes v3.
#    define NOAH_PROFILE_SETTINGS_VERSION 3u
#    define NOAH_PROFILE_SETTINGS_VERSION_ACCEPTED(version) ((version) == 2u || (version) == 3u)
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
