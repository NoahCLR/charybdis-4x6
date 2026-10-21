// Included by the owner harness: actual two-half Save and physical NOR media.
static void assert_no_automatic_admission_work(noah_profile_owner_t *owner, uint32_t now) {
    uint32_t checks = noah_profile_owner_test_profile_held_count();

    for (uint32_t scan = 0u; scan < 4096u; scan++) {
        (void)noah_profile_owner_scan(owner, true, now);
    }
    assert(noah_profile_owner_test_profile_held_count() == checks);
}

static void test_ineligible_auto_saves_skip_admission_work(void) {
    static noah_profile_owner_t left, right;
    static memory_t lm, rm;
    split_link_t ll = {.peer = &right}, rl = {.peer = &left};
    working_link_t lw = {.peer = &right}, rw = {.peer = &left};
    uint32_t now = 100000u;
    uint8_t keymap[KEYMAP_LENGTH] = {0};
    noah_profile_working_splice_t create = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .insert = sizeof(keymap), .flags = NOAH_PROFILE_WORKING_SPLICE_CREATE};

    erased_memory(&lm);
    erased_memory(&rm);
    boot_live_pair(&left, &lm, &right, &rm, &ll, &rl, &lw, &rw, 99u, &now);
    scan_until_converged(&left, &right, &now);
    for (uint8_t mode = 0u; mode < NOAH_PROFILE_EDIT_MODE_COUNT; mode++) {
        left.working_sync.mode.mode = mode;
        assert_no_automatic_admission_work(&left, now);
    }
    // Finishing an adjustment's bookkeeping must still happen on the
    // no-work path.
    left.auto_save.edit_transaction = 7u;
    assert_no_automatic_admission_work(&left, now);
    assert(left.auto_save.edit_transaction == 0u);

    left.working_sync.mode.mode = NOAH_PROFILE_EDIT_MODE_LIVE_REVIEW;
    assert(live_edit(&left, &right, 1u, &create, keymap, sizeof(keymap), &now) == NOAH_PROFILE_WORKING_OK);
    scan_until_converged(&left, &right, &now);
    assert(left.working.active && !left.auto_save.apply_adjustments_only);
    assert_no_automatic_admission_work(&left, now);
    left.working_sync.mode.mode = NOAH_PROFILE_EDIT_MODE_APPLY;
    assert_no_automatic_admission_work(&left, now);
    left.working_sync.mode.mode = NOAH_PROFILE_EDIT_MODE_LIVE;
    left.auto_save.attempted = true;
    assert_no_automatic_admission_work(&left, now);
    left.auto_save.attempted = false;
    left.auto_save.quiet_since = now;
    assert_no_automatic_admission_work(&left, now);
    puts("automatic admission snapshots are skipped without an eligible save or adjustment");
}

static bool auto_quiet = true;
static bool auto_keyboard_quiet(void *context) {
    (void)context;
    return auto_quiet;
}

static void auto_settings(uint8_t bytes[NOAH_SETTINGS_MIN_SIZE]) {
    memset(bytes, 0, NOAH_SETTINGS_MIN_SIZE);
    const uint8_t header[] = {NOAH_SETTINGS_VERSION, 16u, 31u, 128u, 128u, 0u, 128u, 0u};
    memcpy(bytes, header, sizeof(header));
    write_u32(bytes + NOAH_SETTINGS_SCALARS_OFFSET + NOAH_SETTING_AUTO_MOUSE_TIMEOUT * 4u, 1000u);
    write_u32(bytes + NOAH_SETTINGS_SCALARS_OFFSET + NOAH_SETTING_FEEDBACK_PERIOD * 4u, 100u);
    write_u32(bytes + NOAH_SETTINGS_SCALARS_OFFSET + NOAH_SETTING_DEFAULT_DPI * 4u, 400u);
    write_u32(bytes + NOAH_SETTINGS_SCALARS_OFFSET + NOAH_SETTING_SNIPING_DPI * 4u, 200u);
    write_u32(bytes + NOAH_SETTINGS_SCALARS_OFFSET + NOAH_SETTING_DEFAULT_LAYERS * 4u, 1u);
}

// The authored defaults a profile without a settings domain inherits: the
// test settings with a default DPI (600) that no test saves by itself.
static void auto_defaults(uint8_t bytes[NOAH_SETTINGS_MIN_SIZE]) {
    auto_settings(bytes);
    write_u32(bytes + NOAH_SETTINGS_SCALARS_OFFSET + NOAH_SETTING_DEFAULT_DPI * 4u, 600u);
    write_u32(bytes + NOAH_SETTINGS_SCALARS_OFFSET + NOAH_SETTING_RGB_COLOR * 4u, 0x102030u);
}
static uint16_t auto_defaults_length(void) {
    return NOAH_SETTINGS_MIN_SIZE;
}
static uint8_t auto_defaults_byte(uint16_t offset) {
    uint8_t bytes[NOAH_SETTINGS_MIN_SIZE];
    auto_defaults(bytes);
    return offset < sizeof(bytes) ? bytes[offset] : 0u;
}
static void auto_configure(noah_profile_owner_t *left) {
    left->config.keyboard_quiet = auto_keyboard_quiet;
    left->config.settings_defaults_length = auto_defaults_length;
    left->config.settings_defaults_byte = auto_defaults_byte;
}

static void auto_scans(noah_profile_owner_t *left, noah_profile_owner_t *right, uint32_t *now, uint32_t duration) {
    uint32_t start = *now;
    while (*now - start < duration) scan_pair(left, right, now);
}

static uint32_t active_scalar(noah_profile_owner_t *owner, uint8_t setting) {
    uint8_t bytes[4];
    // The only domain in these tests is settings, at payload offset 14.
    assert(noah_profile_owner_working_read(owner, 14u + NOAH_SETTINGS_SCALARS_OFFSET + setting * 4u, bytes, 4u));
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8u | (uint32_t)bytes[2] << 16u | (uint32_t)bytes[3] << 24u;
}

static void test_auto_save_modes_and_adjustments(void) {
    static noah_profile_owner_t left, right;
    static memory_t lm, rm;
    uint8_t settings[NOAH_SETTINGS_MIN_SIZE];
    noah_profile_working_splice_t create = {.domain_id = NOAH_PROFILE_DOMAIN_V1_SETTINGS, .insert = sizeof(settings), .flags = NOAH_PROFILE_WORKING_SPLICE_CREATE};
    noah_profile_working_splice_t dpi = {.domain_id = NOAH_PROFILE_DOMAIN_V1_SETTINGS, .offset = NOAH_SETTINGS_SCALARS_OFFSET + NOAH_SETTING_DEFAULT_DPI * 4u, .remove = 4u, .insert = 4u};
    uint8_t value[4];
    for (uint8_t mode = 0u; mode < NOAH_PROFILE_EDIT_MODE_COUNT; mode++) {
        split_link_t ll = {.peer = &right}, rl = {.peer = &left};
        working_link_t lw = {.peer = &right}, rw = {.peer = &left};
        uint32_t now = UINT32_MAX - 5000u; // timer wrap is ordinary
        erased_memory(&lm); erased_memory(&rm); auto_settings(settings);
        boot_live_pair(&left, &lm, &right, &rm, &ll, &rl, &lw, &rw, 99u, &now);
        scan_until_converged(&left, &right, &now);
        auto_quiet = false;
        left.config.keyboard_quiet = auto_keyboard_quiet;
        left.working_sync.mode.mode = right.working_sync.mode.mode = mode;
        assert(live_edit(&left, &right, 1u, &create, settings, sizeof(settings), &now) == NOAH_PROFILE_WORKING_OK);
        scan_until_converged(&left, &right, &now);
        uint32_t writes = lm.writes + rm.writes;
        auto_quiet = true;
        auto_scans(&left, &right, &now, 2900u);
        assert(lm.writes + rm.writes == writes && left.save.phase == NOAH_PROFILE_OWNER_SAVE_IDLE);
        // A held key restarts the whole quiet interval, even past the deadline.
        auto_quiet = false;
        auto_scans(&left, &right, &now, 10000u);
        assert(lm.writes + rm.writes == writes && left.save.phase == NOAH_PROFILE_OWNER_SAVE_IDLE);
        auto_quiet = true;
        auto_scans(&left, &right, &now, 2900u);
        assert(lm.writes + rm.writes == writes);
        auto_scans(&left, &right, &now, 60000u);
        if (mode == NOAH_PROFILE_EDIT_MODE_LIVE) {
            assert(left.save.keyboard_saves == 1u && !left.working.active && !right.working.active);
            assert(left.save.keyboard_generation == 1u && left.save.last_automatic);
        } else {
            assert(left.save.keyboard_saves == 0u && left.working.active);
            // Apply's wire edits and Review's edits never save themselves.
            assert(noah_profile_owner_save(&left, 30u, left.working.session, left.working.revision) == NOAH_PROFILE_WORKING_QUEUED);
            for (uint32_t guard = 0u; guard < 20000u && left.save.phase; guard++) scan_pair(&left, &right, &now);
            assert(left.save.last_result == NOAH_PROFILE_WORKING_OK);
        }
        scan_until_converged(&left, &right, &now);
        uint32_t saves = left.save.keyboard_saves;
        // Toggle keys cannot enter this path.
        assert(!noah_profile_owner_adjust(&left, NOAH_SETTING_COMBOS_ENABLED, 0u));
        assert(!noah_profile_owner_adjust(&left, NOAH_SETTING_BEHAVIORS_ENABLED, 0u));
        auto_scans(&left, &right, &now, 10000u);
        assert(left.save.keyboard_saves == saves);
        // Four adjustment scalars are a single compound working-copy edit.
        assert(noah_profile_owner_adjust(&left, NOAH_SETTING_DEFAULT_DPI, 600u));
        assert(noah_profile_owner_adjust(&left, NOAH_SETTING_DEFAULT_DPI, 800u));
        assert(noah_profile_owner_adjust(&left, NOAH_SETTING_SNIPING_DPI, 300u));
        assert(noah_profile_owner_adjust(&left, NOAH_SETTING_RGB_MODE, 0x07010101u));
        assert(noah_profile_owner_adjust(&left, NOAH_SETTING_RGB_COLOR, 0x204060u));
        auto_scans(&left, &right, &now, 2000u);
        assert(left.working.active && active_scalar(&left, NOAH_SETTING_DEFAULT_DPI) == 800u);
        assert(active_scalar(&right, NOAH_SETTING_SNIPING_DPI) == 300u);
        assert(active_scalar(&right, NOAH_SETTING_RGB_COLOR) == 0x204060u);
        uint32_t adjustment_revision = left.working.revision;
        if (mode == NOAH_PROFILE_EDIT_MODE_APPLY) {
            uint8_t invalid[4]; write_u32(invalid, 1u); // DPI is at least 400
            assert(live_edit(&left, &right, 34u, &dpi, invalid, sizeof(invalid), &now) == NOAH_PROFILE_WORKING_VALIDATION_REJECTED);
            assert(left.working.revision == adjustment_revision);
        }
        if (mode == NOAH_PROFILE_EDIT_MODE_LIVE_REVIEW) {
            auto_scans(&left, &right, &now, 30000u);
            assert(left.working.active && left.save.keyboard_saves == saves);
            assert(noah_profile_owner_revert(&left, 35u, left.working.session, left.working.revision) == NOAH_PROFILE_WORKING_QUEUED);
            scan_until_converged(&left, &right, &now);
            continue;
        }
        for (uint32_t guard = 0u; guard < 10000u && !left.save.phase; guard++) scan_pair(&left, &right, &now);
        assert(left.save.phase && left.save.automatic);
        // Adjustment keys pressed while Save reads the frozen RAM are queued
        // and become the next revision after Save finishes.
        assert(noah_profile_owner_adjust(&left, NOAH_SETTING_DEFAULT_DPI, 1000u));
        for (uint32_t guard = 0u; guard < 20000u && left.save.phase; guard++) scan_pair(&left, &right, &now);
        assert(left.save.keyboard_saves == saves + 1u && left.save.keyboard_revision == adjustment_revision);
        auto_scans(&left, &right, &now, 2000u);
        assert(left.working.active && active_scalar(&right, NOAH_SETTING_DEFAULT_DPI) == 1000u);
        assert(left.working.revision == adjustment_revision + 1u);
        auto_scans(&left, &right, &now, 60000u);
        assert(left.save.keyboard_saves == saves + 2u && !left.working.active);
        auto_scans(&left, &right, &now, 30000u);
        assert(left.save.keyboard_saves == saves + 2u); // no repeat without an edit
        if (mode == NOAH_PROFILE_EDIT_MODE_APPLY) {
            // Mixing an explicit host edit into adjustments must never save
            // that host edit merely because the keyboard was adjusted too.
            assert(noah_profile_owner_adjust(&left, NOAH_SETTING_DEFAULT_DPI, 1200u));
            auto_scans(&left, &right, &now, 1000u);
            write_u32(value, 1400u);
            assert(live_edit(&left, &right, 50u, &dpi, value, 4u, &now) == NOAH_PROFILE_WORKING_OK);
            auto_scans(&left, &right, &now, 10000u);
            assert(left.working.active && left.save.keyboard_saves == saves + 2u);
        }
    }
    puts("automatic saves: quiet period, held keys, all modes, toggles, compound adjustments, mid-save edits and wrap pass");
}

static void test_auto_save_hour_bound(void) {
    static noah_profile_owner_t left, right;
    static memory_t lm, rm;
    split_link_t ll = {.peer = &right}, rl = {.peer = &left};
    working_link_t lw = {.peer = &right}, rw = {.peer = &left};
    uint8_t settings[NOAH_SETTINGS_MIN_SIZE];
    noah_profile_working_splice_t create = {.domain_id = NOAH_PROFILE_DOMAIN_V1_SETTINGS, .insert = sizeof(settings), .flags = NOAH_PROFILE_WORKING_SPLICE_CREATE};
    uint32_t now = 0u;
    erased_memory(&lm); erased_memory(&rm); auto_settings(settings);
    boot_live_pair(&left, &lm, &right, &rm, &ll, &rl, &lw, &rw, 101u, &now);
    scan_until_converged(&left, &right, &now);
    left.config.keyboard_quiet = auto_keyboard_quiet;
    left.working_sync.mode.mode = right.working_sync.mode.mode = NOAH_PROFILE_EDIT_MODE_LIVE;
    assert(live_edit(&left, &right, 1u, &create, settings, sizeof(settings), &now) == NOAH_PROFILE_WORKING_OK);
    uint32_t start = now;
    // An adjustment every second for an hour. Nothing ever reaches flash.
    for (uint32_t edit = 0u; edit < 3600u; edit++) {
        assert(noah_profile_owner_adjust(&left, NOAH_SETTING_DEFAULT_DPI, edit % 2u ? 600u : 800u));
        auto_scans(&left, &right, &now, 1000u);
    }
    assert(now - start >= 3600000u && !left.save.keyboard_saves && !lm.writes && !rm.writes);
    auto_scans(&left, &right, &now, 60000u);
    assert(left.save.keyboard_saves == 1u);
    // Repeated edit/quiet cycles cannot exceed floor(hour/quiet period),
    // regardless of scan speed or profile size (save time only lowers it).
    start = now;
    uint32_t requested = 0u;
    while (now - start < 3600000u) {
        if (!left.working.active && !left.save.phase && !left.auto_save.pending && both_halves_converged(&left)) {
            assert(noah_profile_owner_adjust(&left, NOAH_SETTING_DEFAULT_DPI, requested++ % 2u ? 600u : 800u));
        }
        scan_pair(&left, &right, &now);
    }
    assert(left.save.keyboard_saves - 1u <= 3600000u / NOAH_PROFILE_OWNER_AUTO_SAVE_QUIET_MS);
    puts("continuous editing: zero saves/hour; separated quiet periods: at most 1200 saves/hour");
}

typedef struct {
    uint8_t bytes[NOAH_PROFILE_STORAGE_REGION_SIZE];
    bool programmed[NOAH_PROFILE_STORAGE_REGION_SIZE / NOAH_PROFILE_STORAGE_PAGE_SIZE];
    uint32_t operations, cut, clock;
    // From this attempt on (1-based, 0 never) erase and program fail and leave
    // the media as they were: a refusing chip, not a power cut.
    uint32_t attempts, fail_from;
    bool off, halfway;
} auto_flash_t;
static auto_flash_t auto_chip, auto_snapshot;
static noah_profile_flash_store_t auto_flash;
static noah_profile_owner_t auto_owner;
static uint8_t auto_working[NOAH_PROFILE_PAYLOAD_MAX];

static bool auto_erase(void *context, uint32_t offset) {
    auto_flash_t *chip = context;
    assert(!chip->off && offset % NOAH_PROFILE_STORAGE_SECTOR_SIZE == 0u);
    chip->attempts++;
    if (chip->fail_from && chip->attempts >= chip->fail_from) return false;
    chip->operations++;
    chip->clock += 45000u;
    bool cut = chip->operations == chip->cut;
    uint32_t count = cut ? (chip->halfway ? NOAH_PROFILE_STORAGE_SECTOR_SIZE / 2u : 0u) : NOAH_PROFILE_STORAGE_SECTOR_SIZE;
    memset(chip->bytes + offset, 255, count);
    memset(chip->programmed + offset / NOAH_PROFILE_STORAGE_PAGE_SIZE, 0, count / NOAH_PROFILE_STORAGE_PAGE_SIZE);
    chip->off = cut;
    return !cut;
}
static bool auto_program(void *context, uint32_t offset, const uint8_t *bytes) {
    auto_flash_t *chip = context;
    assert(!chip->off && offset % NOAH_PROFILE_STORAGE_PAGE_SIZE == 0u && !chip->programmed[offset / NOAH_PROFILE_STORAGE_PAGE_SIZE]);
    chip->attempts++;
    if (chip->fail_from && chip->attempts >= chip->fail_from) return false;
    chip->programmed[offset / NOAH_PROFILE_STORAGE_PAGE_SIZE] = true;
    chip->operations++;
    chip->clock += 800u;
    bool cut = chip->operations == chip->cut;
    uint32_t count = cut ? (chip->halfway ? NOAH_PROFILE_STORAGE_PAGE_SIZE / 2u : 0u) : NOAH_PROFILE_STORAGE_PAGE_SIZE;
    for (uint32_t i = 0u; i < count; i++) {
        assert((chip->bytes[offset + i] & bytes[i]) == bytes[i]);
        chip->bytes[offset + i] &= bytes[i];
    }
    chip->off = cut;
    return !cut;
}
static void auto_read(void *context, uint32_t offset, uint8_t *bytes, uint32_t count) {
    auto_flash_t *chip = context;
    memcpy(bytes, chip->bytes + offset, count);
}
static uint32_t auto_clock(void *context) { return ((auto_flash_t *)context)->clock; }
static void auto_flash_scan(uint32_t *now) {
    noah_profile_flash_store_grant_begin(&auto_flash, auto_quiet);
    if (!auto_chip.off && noah_profile_flash_store_may_write(&auto_flash)) (void)noah_profile_owner_scan(&auto_owner, true, *now);
    (*now)++;
}
static void auto_flash_boot(uint32_t *now) {
    noah_profile_flash_store_init(&auto_flash, (noah_profile_flash_driver_t){.read = auto_read, .erase_sector = auto_erase, .program_page = auto_program, .now_us = auto_clock, .context = &auto_chip});
    assert(noah_profile_owner_init(&auto_owner, &(noah_profile_owner_config_t){.store_io = noah_profile_flash_store_io(&auto_flash), .origin_half = 0u, .working_bytes = auto_working, .working_capacity = sizeof(auto_working), .working_session = 123u, .keyboard_quiet = auto_keyboard_quiet, .settings_defaults_length = auto_defaults_length, .settings_defaults_byte = auto_defaults_byte}));
    for (uint32_t guard = 0u; guard < 20000u && auto_owner.state != NOAH_PROFILE_OWNER_READY_COMPILED && auto_owner.state != NOAH_PROFILE_OWNER_READY_VALIDATED; guard++) auto_flash_scan(now);
    assert(auto_owner.working_ready);
    for (uint8_t settle = 0u; settle < 4u; settle++) auto_flash_scan(now);
}
static void auto_flash_edit(uint16_t transaction, bool create, uint32_t *now) {
    uint8_t settings[NOAH_SETTINGS_MIN_SIZE];
    uint8_t value[4]; write_u32(value, 800u);
    auto_settings(settings);
    uint32_t length = create ? sizeof(settings) : 4u;
    const uint8_t *bytes = create ? settings : value;
    noah_profile_working_splice_t splice = {.domain_id = NOAH_PROFILE_DOMAIN_V1_SETTINGS, .offset = create ? 0u : NOAH_SETTINGS_SCALARS_OFFSET + NOAH_SETTING_DEFAULT_DPI * 4u, .remove = create ? 0u : 4u, .insert = length, .flags = create ? NOAH_PROFILE_WORKING_SPLICE_CREATE : 0u};
    assert(noah_profile_owner_edit_begin(&auto_owner, transaction, auto_owner.working.session, auto_owner.working.revision, 1u, length, 0u) == NOAH_PROFILE_WORKING_OK);
    assert(noah_profile_owner_edit_splice(&auto_owner, transaction, 0u, &splice) == NOAH_PROFILE_WORKING_OK);
    for (uint32_t offset = 0u; offset < length; offset += 22u) assert(noah_profile_owner_edit_data(&auto_owner, transaction, offset, bytes + offset, length - offset < 22u ? length - offset : 22u) == NOAH_PROFILE_WORKING_OK);
    assert(noah_profile_owner_edit_commit(&auto_owner, transaction, crc_of(bytes, length)) == NOAH_PROFILE_WORKING_QUEUED);
    for (uint32_t guard = 0u; guard < 20000u && noah_profile_working_copy_busy(&auto_owner.working); guard++) auto_flash_scan(now);
    assert(auto_owner.working.active);
}
static void test_auto_save_power_cuts(void) {
    uint32_t now = 0u;
    memset(&auto_chip, 0, sizeof(auto_chip)); memset(auto_chip.bytes, 255, sizeof(auto_chip.bytes));
    auto_flash_boot(&now);
    auto_flash_edit(1u, true, &now);
    assert(noah_profile_owner_save(&auto_owner, 2u, auto_owner.working.session, auto_owner.working.revision) == NOAH_PROFILE_WORKING_QUEUED);
    for (uint32_t guard = 0u; guard < 20000u && auto_owner.save.phase; guard++) auto_flash_scan(&now);
    assert(auto_owner.save.last_result == NOAH_PROFILE_WORKING_OK && auto_owner.store.committed.generation == 1u);
    auto_chip.operations = 0u;
    auto_snapshot = auto_chip;
    auto_owner.working_sync.mode.mode = NOAH_PROFILE_EDIT_MODE_LIVE;
    auto_flash_edit(3u, false, &now);
    for (uint32_t guard = 0u; guard < 20000u && !auto_owner.save.keyboard_saves; guard++) auto_flash_scan(&now);
    assert(auto_owner.save.keyboard_saves == 1u && auto_owner.store.committed.generation == 2u);
    uint32_t operations = auto_chip.operations;
    assert(operations >= 8u); // header, payload, markers and erases all traversed
    for (uint32_t cut = 1u; cut <= operations; cut++) {
        for (uint8_t half = 0u; half < 2u; half++) {
            auto_chip = auto_snapshot; auto_chip.cut = cut; auto_chip.halfway = half;
            auto_flash_boot(&now); auto_owner.working_sync.mode.mode = NOAH_PROFILE_EDIT_MODE_LIVE;
            auto_flash_edit(3u, false, &now);
            for (uint32_t guard = 0u; guard < 20000u && !auto_chip.off; guard++) auto_flash_scan(&now);
            assert(auto_chip.off);
            auto_chip.off = false; auto_chip.cut = 0u;
            auto_flash_boot(&now);
            // A torn last marker may already make generation 2 durable;
            // otherwise generation 1 must remain valid, never defaults.
            assert(auto_owner.store.committed.generation == 1u || auto_owner.store.committed.generation == 2u);
            assert(active_scalar(&auto_owner, NOAH_SETTING_DEFAULT_DPI) == (auto_owner.store.committed.generation == 1u ? 400u : 800u));
        }
    }
    printf("automatic save: power cut before/halfway through all %u physical erase/program operations preserves a complete profile\n", operations);
}

// Reads the active settings domain (the only domain in these tests, at
// payload offset 14) and compares it with `expected`.
static void assert_active_settings(noah_profile_owner_t *owner, const uint8_t expected[NOAH_SETTINGS_MIN_SIZE]) {
    uint8_t bytes[NOAH_SETTINGS_MIN_SIZE];
    for (uint32_t offset = 0u; offset < sizeof(bytes); offset += 20u) {
        uint32_t count = sizeof(bytes) - offset < 20u ? sizeof(bytes) - offset : 20u;
        assert(noah_profile_owner_working_read(owner, 14u + offset, bytes + offset, count));
    }
    assert(memcmp(bytes, expected, sizeof(bytes)) == 0);
}

static void auto_run_until(noah_profile_owner_t *left, noah_profile_owner_t *right, uint32_t *now, uint32_t duration) {
    auto_scans(left, right, now, duration);
    scan_until_converged(left, right, now);
}

static void test_an_apply_and_an_adjustment_land_in_the_order_made(void) {
    static noah_profile_owner_t left, right;
    static memory_t lm, rm;
    split_link_t ll = {.peer = &right}, rl = {.peer = &left};
    working_link_t lw = {.peer = &right}, rw = {.peer = &left};
    uint8_t settings[NOAH_SETTINGS_MIN_SIZE], expected[NOAH_SETTINGS_MIN_SIZE];
    noah_profile_working_splice_t create = {.domain_id = NOAH_PROFILE_DOMAIN_V1_SETTINGS, .insert = sizeof(settings), .flags = NOAH_PROFILE_WORKING_SPLICE_CREATE};
    noah_profile_owner_working_status_t status;
    uint32_t now = 1000u;

    erased_memory(&lm); erased_memory(&rm); auto_settings(settings);
    boot_live_pair(&left, &lm, &right, &rm, &ll, &rl, &lw, &rw, 111u, &now);
    scan_until_converged(&left, &right, &now);
    auto_quiet = true;
    auto_configure(&left);
    left.working_sync.mode.mode = right.working_sync.mode.mode = NOAH_PROFILE_EDIT_MODE_APPLY;
    // Saved: default DPI 400 (an explicit Save; Apply never saves a client edit itself).
    assert(live_edit(&left, &right, 1u, &create, settings, sizeof(settings), &now) == NOAH_PROFILE_WORKING_OK);
    scan_until_converged(&left, &right, &now);
    assert(noah_profile_owner_save(&left, 2u, left.working.session, left.working.revision) == NOAH_PROFILE_WORKING_QUEUED);
    for (uint32_t guard = 0u; guard < 20000u && left.save.phase; guard++) scan_pair(&left, &right, &now);
    assert(left.save.last_result == NOAH_PROFILE_WORKING_OK && left.committed_descriptor.generation == 1u);
    scan_until_converged(&left, &right, &now);

    // A DPI key queues 800; a new Apply waits until it publishes (the
    // admission matrix). It publishes, still unsaved; the Apply is then
    // admitted, and it is the later act: it replaces the adjustment, which
    // must not come back as a save of its own.
    assert(noah_profile_owner_adjust(&left, NOAH_SETTING_DEFAULT_DPI, 800u));
    scan_until_converged(&left, &right, &now);
    assert(!left.auto_save.pending && left.working.active && active_scalar(&right, NOAH_SETTING_DEFAULT_DPI) == 800u);
    assert(noah_profile_owner_working_status(&left, &status) && !status.adjustment_pending);
    stage_valid_candidate_pair(&left, &right, 9u, &now);
    commit_valid_candidate_pair(&left, &right, 9u, 2u, &now);
    scan_until_converged(&left, &right, &now);
    auto_run_until(&left, &right, &now, 60000u);
    assert(!left.working.active && !right.working.active && left.save.keyboard_saves == 0u && left.committed_descriptor.generation == 2u && right.committed_descriptor.generation == 2u);

    // An adjustment arriving while an admitted Apply runs lets it finish, then
    // publishes onto the applied profile (here without settings: the inherited
    // defaults gain it) and is saved by the keyboard in Apply mode.
    stage_valid_candidate_pair(&left, &right, 10u, &now);
    assert(noah_profile_owner_adjust(&left, NOAH_SETTING_SNIPING_DPI, 300u));
    commit_valid_candidate_pair(&left, &right, 10u, 3u, &now);
    assert(left.auto_save.pending);
    scan_until_converged(&left, &right, &now);
    assert(left.working.active && active_scalar(&left, NOAH_SETTING_SNIPING_DPI) == 300u && active_scalar(&right, NOAH_SETTING_SNIPING_DPI) == 300u);
    auto_defaults(expected);
    write_u32(expected + NOAH_SETTINGS_SCALARS_OFFSET + NOAH_SETTING_SNIPING_DPI * 4u, 300u);
    assert_active_settings(&left, expected);
    assert_active_settings(&right, expected);
    auto_run_until(&left, &right, &now, 10000u);
    assert(left.save.keyboard_saves == 1u && !left.working.active && left.committed_descriptor.generation == 4u && right.committed_descriptor.generation == 4u);
    assert_active_settings(&left, expected);
    puts("an Apply admitted once a queued adjustment publishes replaces it; one admitted already finishes, then the adjustment follows it");
}

static void test_adjustments_without_a_settings_domain(void) {
    static noah_profile_owner_t left, right;
    static memory_t lm, rm;
    uint8_t settings[NOAH_SETTINGS_MIN_SIZE], expected[NOAH_SETTINGS_MIN_SIZE];
    noah_profile_working_splice_t create = {.domain_id = NOAH_PROFILE_DOMAIN_V1_SETTINGS, .insert = sizeof(settings), .flags = NOAH_PROFILE_WORKING_SPLICE_CREATE};
    noah_profile_working_splice_t drop = {.domain_id = NOAH_PROFILE_DOMAIN_V1_SETTINGS, .remove = sizeof(settings), .flags = NOAH_PROFILE_WORKING_SPLICE_DROP};

    for (uint8_t mode = 0u; mode < NOAH_PROFILE_EDIT_MODE_COUNT; mode++) {
        for (uint8_t dropped = 0u; dropped < 2u; dropped++) {
            split_link_t ll = {.peer = &right}, rl = {.peer = &left};
            working_link_t lw = {.peer = &right}, rw = {.peer = &left};
            uint32_t now = 500u;
            erased_memory(&lm); erased_memory(&rm); auto_settings(settings);
            boot_live_pair(&left, &lm, &right, &rm, &ll, &rl, &lw, &rw, 120u + mode, &now);
            scan_until_converged(&left, &right, &now);
            auto_quiet = true;
            auto_configure(&left);
            left.working_sync.mode.mode = right.working_sync.mode.mode = NOAH_PROFILE_EDIT_MODE_LIVE;
            if (dropped) {
                // Saved with settings, then the domain is dropped and saved:
                // a valid profile that inherits the compiled settings.
                assert(live_edit(&left, &right, 1u, &create, settings, sizeof(settings), &now) == NOAH_PROFILE_WORKING_OK);
                auto_run_until(&left, &right, &now, 10000u);
                assert(left.save.keyboard_saves == 1u && !left.working.active);
                assert(live_edit(&left, &right, 2u, &drop, settings, 0u, &now) == NOAH_PROFILE_WORKING_OK);
                auto_run_until(&left, &right, &now, 10000u);
                assert(left.save.keyboard_saves == 2u && !left.working.active);
            }
            left.working_sync.mode.mode = right.working_sync.mode.mode = mode;
            uint32_t saves = left.save.keyboard_saves, generation = left.committed_descriptor.generation, writes = lm.writes + rm.writes;
            // Two adjustments coalesce; every other scalar stays the default.
            assert(noah_profile_owner_adjust(&left, NOAH_SETTING_DEFAULT_DPI, 800u));
            assert(noah_profile_owner_adjust(&left, NOAH_SETTING_DEFAULT_DPI, 1000u));
            assert(noah_profile_owner_adjust(&left, NOAH_SETTING_RGB_MODE, 0x07010101u));
            auto_run_until(&left, &right, &now, 2000u);
            auto_defaults(expected);
            write_u32(expected + NOAH_SETTINGS_SCALARS_OFFSET + NOAH_SETTING_DEFAULT_DPI * 4u, 1000u);
            write_u32(expected + NOAH_SETTINGS_SCALARS_OFFSET + NOAH_SETTING_RGB_MODE * 4u, 0x07010101u);
            assert(!left.auto_save.pending && left.working.active && left.working.last_result == NOAH_PROFILE_WORKING_OK);
            assert_active_settings(&left, expected);
            assert_active_settings(&right, expected);
            assert(lm.writes + rm.writes == writes); // an edit alone writes no flash
            auto_run_until(&left, &right, &now, 10000u);
            if (mode == NOAH_PROFILE_EDIT_MODE_LIVE_REVIEW) {
                assert(left.working.active && left.save.keyboard_saves == saves);
                continue;
            }
            // Live and Apply save an adjustment-only copy; the revision advanced.
            assert(!left.working.active && left.save.keyboard_saves == saves + 1u && left.committed_descriptor.generation == generation + 1u && right.committed_descriptor.generation == generation + 1u);
            // A replug boots the saved profile with the adjusted values and every other default.
            boot_live_pair(&left, &lm, &right, &rm, &ll, &rl, &lw, &rw, 130u, &now);
            scan_until_converged(&left, &right, &now);
            assert_active_settings(&left, expected);
            assert_active_settings(&right, expected);
        }
    }
    puts("adjustments on a profile without a settings domain create it from the defaults, keep every other value, and save");
}

// Runs the keyboard's automatic attempt to its end, on the physical media.
static void auto_flash_run_attempt(uint32_t *now) {
    for (uint32_t guard = 0u; guard < 40000u && !(auto_owner.auto_save.attempted && auto_owner.save.phase == NOAH_PROFILE_OWNER_SAVE_IDLE); guard++) auto_flash_scan(now);
    assert(auto_owner.auto_save.attempted && auto_owner.save.phase == NOAH_PROFILE_OWNER_SAVE_IDLE);
}

static void test_a_failed_automatic_save_is_reported_and_not_repeated(void) {
    for (uint8_t explicit_retry = 0u; explicit_retry < 2u; explicit_retry++) {
        uint32_t now = 0u;
        memset(&auto_chip, 0, sizeof(auto_chip)); memset(auto_chip.bytes, 255, sizeof(auto_chip.bytes));
        auto_flash_boot(&now);
        auto_flash_edit(1u, true, &now);
        assert(noah_profile_owner_save(&auto_owner, 2u, auto_owner.working.session, auto_owner.working.revision) == NOAH_PROFILE_WORKING_QUEUED);
        for (uint32_t guard = 0u; guard < 20000u && auto_owner.save.phase; guard++) auto_flash_scan(&now);
        assert(auto_owner.save.last_result == NOAH_PROFILE_WORKING_OK && auto_owner.store.committed.generation == 1u);
        auto_owner.working_sync.mode.mode = NOAH_PROFILE_EDIT_MODE_LIVE;

        // The chip refuses every write from now on.
        auto_chip.fail_from = auto_chip.attempts + 1u;
        auto_flash_edit(3u, false, &now);
        uint32_t revision = auto_owner.working.revision;
        auto_flash_run_attempt(&now);
        assert(auto_owner.save.last_automatic && auto_owner.save.last_result != NOAH_PROFILE_WORKING_OK && auto_owner.save.last_error != 0u);
        assert(auto_owner.save.keyboard_saves == 0u && auto_owner.store.committed.generation == 1u);
        assert(auto_owner.working.active && auto_owner.working.revision == revision);
        // Reported through the ordinary Save status, and the edit stays unsaved.
        noah_profile_owner_working_status_t status;
        assert(noah_profile_owner_working_status(&auto_owner, &status) && status.save.last_result == auto_owner.save.last_result && status.local.active);
        // No further write is even attempted for the failed revision.
        uint32_t attempts = auto_chip.attempts;
        for (uint32_t scan = 0u; scan < 60000u; scan++) auto_flash_scan(&now);
        assert(auto_chip.attempts == attempts && auto_owner.save.keyboard_saves == 0u && auto_owner.save.phase == NOAH_PROFILE_OWNER_SAVE_IDLE);

        auto_chip.fail_from = 0u;
        if (explicit_retry) {
            // An explicit Save is the retry...
            assert(noah_profile_owner_save(&auto_owner, 4u, auto_owner.working.session, auto_owner.working.revision) == NOAH_PROFILE_WORKING_QUEUED);
            for (uint32_t guard = 0u; guard < 20000u && auto_owner.save.phase; guard++) auto_flash_scan(&now);
            assert(auto_owner.save.last_result == NOAH_PROFILE_WORKING_OK && !auto_owner.save.last_automatic && auto_owner.save.keyboard_saves == 0u);
        } else {
            // ...and so is another edit, saved by the keyboard itself.
            uint8_t value[4]; write_u32(value, 1000u);
            noah_profile_working_splice_t dpi = {.domain_id = NOAH_PROFILE_DOMAIN_V1_SETTINGS, .offset = NOAH_SETTINGS_SCALARS_OFFSET + NOAH_SETTING_DEFAULT_DPI * 4u, .remove = 4u, .insert = 4u};
            assert(noah_profile_owner_edit_begin(&auto_owner, 5u, auto_owner.working.session, auto_owner.working.revision, 1u, 4u, 0u) == NOAH_PROFILE_WORKING_OK);
            assert(noah_profile_owner_edit_splice(&auto_owner, 5u, 0u, &dpi) == NOAH_PROFILE_WORKING_OK);
            assert(noah_profile_owner_edit_data(&auto_owner, 5u, 0u, value, 4u) == NOAH_PROFILE_WORKING_OK);
            assert(noah_profile_owner_edit_commit(&auto_owner, 5u, crc_of(value, 4u)) == NOAH_PROFILE_WORKING_QUEUED);
            for (uint32_t guard = 0u; guard < 60000u && !auto_owner.save.keyboard_saves; guard++) auto_flash_scan(&now);
            assert(auto_owner.save.keyboard_saves == 1u && auto_owner.save.last_automatic && auto_owner.save.last_result == NOAH_PROFILE_WORKING_OK);
        }
        for (uint32_t guard = 0u; guard < 20000u && (auto_owner.save.phase || auto_owner.working.active); guard++) auto_flash_scan(&now);
        assert(!auto_owner.working.active && auto_owner.store.committed.generation == 2u);
        assert(active_scalar(&auto_owner, NOAH_SETTING_DEFAULT_DPI) == (explicit_retry ? 800u : 1000u));
    }
    puts("a failed automatic save is reported, not repeated for its revision, and an explicit Save or a new edit retries it");
}

// An edit and its undo, or a value set back, leave a working copy byte for
// byte the saved profile ("Unsaved edits match what is saved", keyboard
// check, 2026-10-19). Saving it writes nothing: the keyboard runs its saved
// profile again with the same revision on both halves, whether the host
// saves (Live + review) or the keyboard does by itself (Live).
static void test_saving_the_saved_bytes_writes_nothing(void) {
    static noah_profile_owner_t left, right;
    static memory_t lm, rm;
    uint8_t settings[NOAH_SETTINGS_MIN_SIZE];
    noah_profile_working_splice_t create = {.domain_id = NOAH_PROFILE_DOMAIN_V1_SETTINGS, .insert = sizeof(settings), .flags = NOAH_PROFILE_WORKING_SPLICE_CREATE};
    noah_profile_working_splice_t dpi = {.domain_id = NOAH_PROFILE_DOMAIN_V1_SETTINGS, .offset = NOAH_SETTINGS_SCALARS_OFFSET + NOAH_SETTING_DEFAULT_DPI * 4u, .remove = 4u, .insert = 4u};
    const uint8_t modes[] = {NOAH_PROFILE_EDIT_MODE_LIVE_REVIEW, NOAH_PROFILE_EDIT_MODE_LIVE};
    uint8_t value[4];

    for (uint8_t m = 0u; m < sizeof(modes); m++) {
        split_link_t ll = {.peer = &right}, rl = {.peer = &left};
        working_link_t lw = {.peer = &right}, rw = {.peer = &left};
        uint32_t now = 1000u;
        erased_memory(&lm); erased_memory(&rm); auto_settings(settings);
        boot_live_pair(&left, &lm, &right, &rm, &ll, &rl, &lw, &rw, 77u, &now);
        scan_until_converged(&left, &right, &now);
        auto_quiet = false;
        left.config.keyboard_quiet = auto_keyboard_quiet;
        left.working_sync.mode.mode = right.working_sync.mode.mode = modes[m];
        assert(live_edit(&left, &right, 1u, &create, settings, sizeof(settings), &now) == NOAH_PROFILE_WORKING_OK);
        scan_until_converged(&left, &right, &now);
        assert(noah_profile_owner_save(&left, 20u, left.working.session, left.working.revision) == NOAH_PROFILE_WORKING_QUEUED);
        for (uint32_t guard = 0u; guard < 20000u && left.save.phase; guard++) scan_pair(&left, &right, &now);
        assert(left.save.last_result == NOAH_PROFILE_WORKING_OK && left.committed_descriptor.generation == 1u);
        scan_until_converged(&left, &right, &now);
        // DPI to 800, then back to the saved 400.
        write_u32(value, 800u);
        assert(live_edit(&left, &right, 2u, &dpi, value, sizeof(value), &now) == NOAH_PROFILE_WORKING_OK);
        scan_until_converged(&left, &right, &now);
        write_u32(value, 400u);
        assert(live_edit(&left, &right, 3u, &dpi, value, sizeof(value), &now) == NOAH_PROFILE_WORKING_OK);
        scan_until_converged(&left, &right, &now);
        assert(left.working.active && right.working.active && active_scalar(&left, NOAH_SETTING_DEFAULT_DPI) == 400u);
        uint32_t revision = left.working.revision, writes = lm.writes + rm.writes, saves = left.save.keyboard_saves;
        if (modes[m] == NOAH_PROFILE_EDIT_MODE_LIVE_REVIEW) {
            assert(noah_profile_owner_save(&left, 21u, left.working.session, revision) == NOAH_PROFILE_WORKING_QUEUED);
            for (uint32_t guard = 0u; guard < 20000u && left.save.phase; guard++) scan_pair(&left, &right, &now);
            assert(left.save.last_result == NOAH_PROFILE_WORKING_OK && left.save.last_transaction_id == 21u);
        } else {
            auto_quiet = true;
            auto_scans(&left, &right, &now, 10000u);
            assert(left.save.keyboard_saves == saves + 1u && left.save.last_automatic && left.save.last_result == NOAH_PROFILE_WORKING_OK);
            assert(left.save.keyboard_revision == revision && left.save.keyboard_generation == 1u);
        }
        scan_until_converged(&left, &right, &now);
        assert(lm.writes + rm.writes == writes);
        assert(left.committed_descriptor.generation == 1u && right.committed_descriptor.generation == 1u);
        assert(!left.working.active && !right.working.active);
        assert(left.working.revision == revision && right.working.revision == revision);
        assert(active_scalar(&left, NOAH_SETTING_DEFAULT_DPI) == 400u && active_scalar(&right, NOAH_SETTING_DEFAULT_DPI) == 400u);
        auto_scans(&left, &right, &now, 10000u);
        assert(lm.writes + rm.writes == writes && left.save.keyboard_saves == saves + (modes[m] == NOAH_PROFILE_EDIT_MODE_LIVE ? 1u : 0u));
        // The next edit starts from the saved profile as usual.
        auto_quiet = false;
        write_u32(value, 600u);
        assert(live_edit(&left, &right, 4u, &dpi, value, sizeof(value), &now) == NOAH_PROFILE_WORKING_OK);
        scan_until_converged(&left, &right, &now);
        assert(left.working.active && left.working.revision == revision + 1u && active_scalar(&right, NOAH_SETTING_DEFAULT_DPI) == 600u);
    }
    auto_quiet = true;
    puts("saving a working copy identical to the saved profile writes nothing, in Live + review and in Live");
}
