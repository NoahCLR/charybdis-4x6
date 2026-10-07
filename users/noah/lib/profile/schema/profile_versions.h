#pragma once

// The blob validator and persistent shape check share the versions firmware
// writes. Older formats are rejected, never migrated by firmware.
#define NOAH_PROFILE_SCHEMA_MAJOR 2u
#define NOAH_PROFILE_PAYLOAD_MAX 5088u
#define NOAH_PROFILE_DOMAIN_COUNT 5u
#define NOAH_PROFILE_PD_COUNT 32u
#define NOAH_PROFILE_PD_VERSION 2u
#define NOAH_PROFILE_RGB_VERSION 3u
#define NOAH_PROFILE_SETTINGS_VERSION 5u
#define NOAH_PROFILE_LOGICAL_STORE_VERSION 3u
#define NOAH_PROFILE_PD_SETTINGS_VERSION_ACCEPTED(version) ((version) == NOAH_PROFILE_SETTINGS_VERSION)
#define NOAH_PROFILE_SETTINGS_VERSION_ACCEPTED(version) NOAH_PROFILE_PD_SETTINGS_VERSION_ACCEPTED(version)
#define NOAH_PROFILE_COMBO_VERSION_ACCEPTED(version) ((version) == 2u)
#define NOAH_PROFILE_PD_RGB_VERSION_ACCEPTED(version) ((version) == NOAH_PROFILE_RGB_VERSION)
#define NOAH_PROFILE_PD_DOMAIN_VERSION_ACCEPTED(version) ((version) == NOAH_PROFILE_PD_VERSION)
