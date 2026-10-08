#pragma once
#include "profile_domain_registry.h"

// The blob validator and persistent shape check share the versions firmware
// writes. Older formats are rejected, never migrated by firmware.
#define NOAH_PROFILE_SCHEMA_MAJOR 2u
#define NOAH_PROFILE_PAYLOAD_MAX 5088u
#define NOAH_PROFILE_DOMAIN_COUNT NOAH_PROFILE_DOMAIN_REGISTRY_COUNT
#define NOAH_PROFILE_PD_COUNT 32u
#define NOAH_PROFILE_PD_VERSION NOAH_PROFILE_DOMAIN_VERSION_PD
#define NOAH_PROFILE_RGB_VERSION NOAH_PROFILE_DOMAIN_VERSION_RGB
#define NOAH_PROFILE_SETTINGS_VERSION NOAH_PROFILE_DOMAIN_VERSION_SETTINGS
#define NOAH_PROFILE_LOGICAL_STORE_VERSION 3u
