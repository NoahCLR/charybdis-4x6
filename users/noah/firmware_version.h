#pragma once

// The firmware version: the firmware release this build includes (D-F77).
// The Profile Wire capability page reports it, and nothing else depends on
// it: a release on its own never invalidates a saved profile or a backup.
//
// `release` rewrites the NOAH_FIRMWARE_VERSION line when it prepares a
// firmware release; never change it by hand. Builds from dev and task
// branches report the last release they include.

// vYYYY.MM.DD[.N]: year in bits 31-16, month in bits 15-8, then N (0-7, 0
// for no suffix) above a five-bit day.
#define NOAH_FIRMWARE_RELEASE(year, month, day, n) (((0UL + (year)) << 16) | ((0UL + (month)) << 8) | ((0UL + (n)) << 5) | (0UL + (day)))

// Host tests build other releases with -DNOAH_FIRMWARE_VERSION to show the
// action-ABI digest does not depend on it; firmware builds never set it.
#ifndef NOAH_FIRMWARE_VERSION
#define NOAH_FIRMWARE_VERSION NOAH_FIRMWARE_RELEASE(2026, 11, 20, 0)
#endif
