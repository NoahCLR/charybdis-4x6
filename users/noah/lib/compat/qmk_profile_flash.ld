/*
 * Profile slot region guard (D-F29), an implicit linker script added to the
 * link by users/noah/rules.mk. The two 128 KiB profile slots start 1 MiB
 * into flash (XIP 0x10100000, NOAH_PROFILE_STORAGE_FLASH_OFFSET); the
 * firmware image must end before them.
 */
ASSERT(__flash_binary_end <= 0x10100000, "the firmware image reaches the profile slot region (D-F29)");
