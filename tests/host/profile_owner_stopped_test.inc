// ── A stopped owner, and a restart (D-F34) ──────────────────────────────────
//
// An owner in a state nothing but a restart leaves says so in its working
// status, and the other half hears it too; a slow save never does. A restart
// is a reset between scans: anywhere in a save it leaves both halves on the
// old profile or the new one, never anything else. The failures are faked
// through the memory, the compiled reader and the peer authority, so no
// particular defect is needed to stop an owner.

static noah_profile_owner_working_status_t working_status_of(const noah_profile_owner_t *owner) {
    noah_profile_owner_working_status_t status;

    assert(noah_profile_owner_working_status(owner, &status));
    return status;
}

static void assert_neither_stopped(const noah_profile_owner_t *left, const noah_profile_owner_t *right) {
    noah_profile_owner_working_status_t usb   = working_status_of(left);
    noah_profile_owner_working_status_t other = working_status_of(right);

    assert(!usb.stopped && !usb.peer_stopped && !other.stopped && !noah_profile_owner_stopped(left) && !noah_profile_owner_stopped(right));
}

// Scans change nothing, and Save, Revert, edits and the edit mode are refused.
static void assert_stays_stopped(noah_profile_owner_t *stopped, noah_profile_owner_t *master, noah_profile_owner_t *peer, uint32_t *now) {
    noah_profile_owner_state_t state = stopped->state;

    assert(noah_profile_owner_stopped(stopped));
    for (unsigned scan = 0u; scan < 400u; scan++) {
        if (peer) {
            scan_pair(master, peer, now);
        } else {
            (void)noah_profile_owner_scan(master, true, (*now)++);
        }
    }
    assert(stopped->state == state && noah_profile_owner_stopped(stopped));
    assert(noah_profile_owner_edit_begin(stopped, 900u, stopped->working.session, stopped->working.revision, 1u, 2u, 0u) == NOAH_PROFILE_WORKING_INVALID_STATE);
    assert(noah_profile_owner_save(stopped, 901u, stopped->working.session, stopped->working.revision) == NOAH_PROFILE_WORKING_INVALID_STATE);
    assert(noah_profile_owner_revert(stopped, 902u, stopped->working.session, stopped->working.revision) == NOAH_PROFILE_WORKING_INVALID_STATE);
    assert(noah_profile_owner_set_edit_mode(stopped, NOAH_PROFILE_EDIT_MODE_LIVE) == NOAH_PROFILE_WORKING_INVALID_STATE);
    if (stopped->working_ready) {
        noah_profile_owner_working_status_t status = working_status_of(stopped);

        assert(status.stopped && status.edits_blocked);
    }
}

static void test_only_states_a_restart_leaves_are_stopped(void) {
    static noah_profile_owner_t       owner;
    const noah_profile_owner_state_t stopped[] = {
        NOAH_PROFILE_OWNER_GENERATION_CONFLICT, NOAH_PROFILE_OWNER_DURABILITY_UNKNOWN,     NOAH_PROFILE_OWNER_COMPILED_ERROR,     NOAH_PROFILE_OWNER_STORAGE_ERROR,
        NOAH_PROFILE_OWNER_INTEGRATION_ERROR,   NOAH_PROFILE_OWNER_POSTCOMMIT_AUTHORITY_LOST, NOAH_PROFILE_OWNER_CONCURRENT_COMMIT,
    };
    const noah_profile_owner_state_t running[] = {
        NOAH_PROFILE_OWNER_UNINITIALIZED,         NOAH_PROFILE_OWNER_VALIDATING_COMPILED,  NOAH_PROFILE_OWNER_DISCOVERING, NOAH_PROFILE_OWNER_ADOPTING_COMMITTED,
        NOAH_PROFILE_OWNER_RECONCILING_COMMITTED, NOAH_PROFILE_OWNER_ACTIVATING_COMMITTED, NOAH_PROFILE_OWNER_READY_COMPILED, NOAH_PROFILE_OWNER_READY_VALIDATED,
    };

    assert(!noah_profile_owner_stopped(NULL));
    for (unsigned index = 0u; index < sizeof(stopped) / sizeof(stopped[0]); index++) {
        memset(&owner, 0, sizeof(owner));
        owner.state = stopped[index];
        assert(noah_profile_owner_stopped(&owner));
        // The scan has nothing to do in it, on the USB half.
        assert(!noah_profile_owner_scan(&owner, true, 1u) && owner.state == stopped[index]);
    }
    for (unsigned index = 0u; index < sizeof(running) / sizeof(running[0]); index++) {
        memset(&owner, 0, sizeof(owner));
        owner.state = running[index];
        assert(!noah_profile_owner_stopped(&owner));
    }
    puts("only the states a restart alone leaves count as stopped");
}

static noah_profile_working_result_t live_replace(noah_profile_owner_t *left, noah_profile_owner_t *right, uint16_t transaction, const uint8_t *bytes, uint32_t length, uint32_t *now) {
    noah_profile_working_result_t result = noah_profile_owner_edit_begin(left, transaction, left->working.session, left->working.revision, 0u, length, NOAH_PROFILE_WORKING_EDIT_REPLACE);

    if (result != NOAH_PROFILE_WORKING_OK) return result;
    for (uint32_t offset = 0u; offset < length; offset += 22u) {
        assert(noah_profile_owner_edit_data(left, transaction, offset, bytes + offset, length - offset < 22u ? length - offset : 22u) == NOAH_PROFILE_WORKING_OK);
    }
    assert(noah_profile_owner_edit_commit(left, transaction, crc_of(bytes, length)) == NOAH_PROFILE_WORKING_QUEUED);
    for (uint32_t guard = 0u; guard < 8192u && noah_profile_working_copy_busy(&left->working); guard++) scan_pair(left, right, now);
    return (noah_profile_working_result_t)left->working.last_result;
}

static void fill_keymap(uint8_t *keymap, uint8_t seed) {
    memset(keymap, 0, KEYMAP_LENGTH);
    for (uint32_t i = 0u; i < KEYMAP_LENGTH; i += 2u) keymap[i] = (uint8_t)(4u + (i + seed) % 20u);
}

// The keyboard check of 2026-10-19 in miniature: a save the USB half cannot
// finish stops its owner. It says so, keeps the unsaved edits readable, and a
// restart brings both halves back; the kept copy then goes back as one
// replacement and saves.
static void test_a_storage_failure_in_a_save_stops_the_usb_half_and_says_so(void) {
    static noah_profile_owner_t left, right;
    static memory_t             lm, rm;
    static uint8_t              keymap[KEYMAP_LENGTH], kept[NOAH_PROFILE_PAYLOAD_MAX], bytes[NOAH_PROFILE_PAYLOAD_MAX];
    split_link_t                ll = {.peer = &right}, rl = {.peer = &left};
    working_link_t              lw = {.peer = &right}, rw = {.peer = &left};
    noah_profile_working_splice_t create = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .insert = KEYMAP_LENGTH, .flags = NOAH_PROFILE_WORKING_SPLICE_CREATE};
    noah_profile_owner_working_status_t status;
    uint32_t now = 1000u, length;

    erased_memory(&lm);
    erased_memory(&rm);
    boot_live_pair(&left, &lm, &right, &rm, &ll, &rl, &lw, &rw, 0x5701u, &now);
    scan_until_converged(&left, &right, &now);
    fill_keymap(keymap, 0u);
    assert(live_edit(&left, &right, 1u, &create, keymap, KEYMAP_LENGTH, &now) == NOAH_PROFILE_WORKING_OK);
    scan_until_converged(&left, &right, &now);
    length = working_status_of(&left).local.length;
    read_active(&left, kept, length);
    assert(noah_profile_owner_save(&left, 40u, left.working.session, left.working.revision) == NOAH_PROFILE_WORKING_QUEUED);
    // A save under way is not a stopped owner, on either half.
    for (uint32_t guard = 0u; guard < 20000u && !noah_profile_split_reconciler_prepared_push_ready(&left.reconciler, NULL); guard++) {
        scan_pair(&left, &right, &now);
        assert_neither_stopped(&left, &right);
    }
    assert(noah_profile_split_reconciler_prepared_push_ready(&left.reconciler, NULL));
    lm.fail_next_write = true;
    for (uint32_t guard = 0u; guard < 4096u && left.state != NOAH_PROFILE_OWNER_STORAGE_ERROR; guard++) scan_pair(&left, &right, &now);
    assert(left.state == NOAH_PROFILE_OWNER_STORAGE_ERROR);
    status = working_status_of(&left);
    assert(status.stopped && !status.peer_stopped && status.local.active && status.edits_blocked);
    assert(!working_status_of(&right).stopped);
    assert_stays_stopped(&left, &left, &right, &now);
    // The unsaved edits are still there to read back.
    read_active(&left, bytes, length);
    assert(memcmp(bytes, kept, length) == 0);

    // A restart reads flash afresh: nothing was saved, so both halves run the
    // compiled defaults, and they work again.
    boot_live_pair(&left, &lm, &right, &rm, &ll, &rl, &lw, &rw, 0x5702u, &now);
    scan_until_converged(&left, &right, &now);
    assert(!left.committed_descriptor.has_profile && !right.committed_descriptor.has_profile && !left.working.active);
    assert_neither_stopped(&left, &right);
    // The kept copy goes back as one replacement and saves on both halves.
    assert(live_replace(&left, &right, 2u, kept, length, &now) == NOAH_PROFILE_WORKING_OK);
    scan_until_converged(&left, &right, &now);
    assert_same_active(&left, &right, kept, length);
    assert(noah_profile_owner_save(&left, 41u, left.working.session, left.working.revision) == NOAH_PROFILE_WORKING_QUEUED);
    for (uint32_t guard = 0u; guard < 20000u && left.save.phase != NOAH_PROFILE_OWNER_SAVE_IDLE; guard++) scan_pair(&left, &right, &now);
    assert(left.save.last_result == NOAH_PROFILE_WORKING_OK);
    scan_until_converged(&left, &right, &now);
    assert(left.committed_descriptor.generation == 1u && right.committed_descriptor.generation == 1u && !left.working.active);
    assert_same_active(&left, &right, kept, length);
    puts("a storage failure in a save stops the USB half, which says so and keeps its edits; a restart brings both back and the kept copy saves");
}

// A half that cannot read its slots at boot stops. On the other half the USB
// half hears it, polls it and sends it nothing else; on the USB half it says
// so itself. A restart with readable flash clears it.
static void test_a_half_that_cannot_read_its_slots_stops_and_is_heard(void) {
    static noah_profile_owner_t left, right;
    static memory_t             lm, rm;
    split_link_t                ll = {.peer = &right}, rl = {.peer = &left};
    working_link_t              lw = {.peer = &right}, rw = {.peer = &left};
    noah_profile_owner_working_status_t status;
    uint32_t now = 1000u, exchanges;

    erased_memory(&lm);
    erased_memory(&rm);
    rm.fail_reads = true;
    boot_live_pair(&left, &lm, &right, &rm, &ll, &rl, &lw, &rw, 0x5703u, &now);
    assert(right.state == NOAH_PROFILE_OWNER_STORAGE_ERROR && working_status_of(&right).stopped);
    for (uint32_t guard = 0u; guard < 400u && !working_status_of(&left).peer_stopped; guard++) scan_pair(&left, &right, &now);
    status = working_status_of(&left);
    assert(status.peer_stopped && !status.stopped && !status.halves_agree && status.edits_blocked);
    assert(noah_profile_owner_edit_begin(&left, 5u, left.working.session, left.working.revision, 1u, 2u, 0u) == NOAH_PROFILE_WORKING_BUSY);
    // Polled once a second (a poll is admitted, then answered), nothing
    // else; while the halves differ it would be ten times as often.
    exchanges = lw.exchanges;
    for (uint32_t scan = 0u; scan < 10000u / NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS; scan++) scan_pair(&left, &right, &now);
    assert(lw.exchanges - exchanges <= 24u && working_status_of(&left).peer_stopped);
    assert(!right.working.active && right.working.revision == 0u);
    assert_stays_stopped(&right, &left, &right, &now);
    rm.fail_reads = false;
    boot_live_pair(&left, &lm, &right, &rm, &ll, &rl, &lw, &rw, 0x5704u, &now);
    scan_until_converged(&left, &right, &now);
    assert_neither_stopped(&left, &right);

    // The same on the USB half: it says so itself.
    lm.fail_reads = true;
    boot_live_pair(&left, &lm, &right, &rm, &ll, &rl, &lw, &rw, 0x5705u, &now);
    assert(left.state == NOAH_PROFILE_OWNER_STORAGE_ERROR);
    status = working_status_of(&left);
    assert(status.stopped && !status.peer_stopped);
    assert_stays_stopped(&left, &left, &right, &now);
    lm.fail_reads = false;
    puts("a half that cannot read its slots stops; the USB half hears the other half say so and only polls it; a restart clears it");
}

// The USB half's commit marker landed but was reported failed: the decision
// may or may not be durable. The owner stops; a restart reads the marker, and
// both halves take the new profile.
static void test_an_unknown_commit_marker_stops_until_a_restart_reads_it(void) {
    static noah_profile_owner_t left, right;
    static memory_t             lm, rm;
    static uint8_t              keymap[KEYMAP_LENGTH], kept[NOAH_PROFILE_PAYLOAD_MAX];
    split_link_t                ll = {.peer = &right}, rl = {.peer = &left};
    working_link_t              lw = {.peer = &right}, rw = {.peer = &left};
    noah_profile_working_splice_t create = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .insert = KEYMAP_LENGTH, .flags = NOAH_PROFILE_WORKING_SPLICE_CREATE};
    uint32_t now = 1000u, length;

    erased_memory(&lm);
    erased_memory(&rm);
    boot_live_pair(&left, &lm, &right, &rm, &ll, &rl, &lw, &rw, 0x5706u, &now);
    scan_until_converged(&left, &right, &now);
    fill_keymap(keymap, 3u);
    assert(live_edit(&left, &right, 1u, &create, keymap, KEYMAP_LENGTH, &now) == NOAH_PROFILE_WORKING_OK);
    scan_until_converged(&left, &right, &now);
    length = working_status_of(&left).local.length;
    read_active(&left, kept, length);
    lm.fail_commit_marker = true;
    assert(noah_profile_owner_save(&left, 40u, left.working.session, left.working.revision) == NOAH_PROFILE_WORKING_QUEUED);
    for (uint32_t guard = 0u; guard < 40000u && left.state != NOAH_PROFILE_OWNER_DURABILITY_UNKNOWN && left.save.phase != NOAH_PROFILE_OWNER_SAVE_IDLE; guard++) scan_pair(&left, &right, &now);
    assert(left.state == NOAH_PROFILE_OWNER_DURABILITY_UNKNOWN && !lm.fail_commit_marker);
    assert(working_status_of(&left).stopped);
    assert_stays_stopped(&left, &left, &right, &now);
    boot_live_pair(&left, &lm, &right, &rm, &ll, &rl, &lw, &rw, 0x5707u, &now);
    for (uint32_t guard = 0u; guard < 40000u && !(left.committed_descriptor.generation == 1u && right.committed_descriptor.generation == 1u && both_halves_converged(&left)); guard++) scan_pair(&left, &right, &now);
    assert(left.committed_descriptor.generation == 1u && right.committed_descriptor.generation == 1u);
    assert_neither_stopped(&left, &right);
    assert_same_active(&left, &right, kept, length);
    puts("an unknown commit marker stops the owner until a restart reads it; both halves then take the new profile");
}

static void boot_single(noah_profile_owner_t *owner, memory_t *memory, uint8_t origin, uint32_t *now) {
    assert(noah_profile_owner_init(owner, &(noah_profile_owner_config_t){.store_io = {.read = memory_read, .write = memory_write, .context = memory}, .origin_half = origin}));
    for (uint32_t guard = 0u; guard < 4096u && owner->state < NOAH_PROFILE_OWNER_READY_COMPILED; guard++) (void)noah_profile_owner_scan(owner, true, (*now)++);
}

// Two saved profiles of one generation, one from each half: at boot neither
// can be chosen, and only a restart onto other flash could change that.
static void test_two_profiles_of_one_generation_stop_the_owner(void) {
    static noah_profile_owner_t first, second, owner;
    static memory_t             a, b;
    uint32_t                    now = 100u;

    erased_memory(&a);
    erased_memory(&b);
    boot_single(&first, &a, 0u, &now);
    commit_blob(&first, 61u, &now);
    boot_single(&second, &b, 1u, &now);
    commit_blob(&second, 62u, &now);
    assert(first.store.committed.slot == NOAH_PROFILE_SLOT_A && second.store.committed.slot == NOAH_PROFILE_SLOT_A);
    memcpy(&a.bytes[NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR], &b.bytes[NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR], NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR - NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR);
    boot_single(&owner, &a, 0u, &now);
    assert(owner.state == NOAH_PROFILE_OWNER_GENERATION_CONFLICT);
    assert_stays_stopped(&owner, &owner, NULL, &now);
    puts("two saved profiles of one generation stop the owner at boot");
}

// The compiled defaults themselves cannot be read: nothing can run.
static void test_unreadable_compiled_defaults_stop_the_owner(void) {
    static noah_profile_owner_t owner;
    static memory_t             memory;
    uint32_t                    now = 100u;

    erased_memory(&memory);
    compiled_read_fails = true;
    // Refused at once, or by the first validation step that reads.
    if (noah_profile_owner_init(&owner, &(noah_profile_owner_config_t){.store_io = {.read = memory_read, .write = memory_write, .context = &memory}})) {
        for (uint32_t guard = 0u; guard < 64u && owner.state == NOAH_PROFILE_OWNER_VALIDATING_COMPILED; guard++) (void)noah_profile_owner_scan(&owner, true, now++);
    }
    compiled_read_fails = false;
    assert(owner.state == NOAH_PROFILE_OWNER_COMPILED_ERROR);
    assert_stays_stopped(&owner, &owner, NULL, &now);
    puts("unreadable compiled defaults stop the owner");
}

// A save that waits a long time for a safe boundary is slow, not stopped.
static void test_a_slow_save_is_never_stopped(void) {
    static noah_profile_owner_t left, right;
    static memory_t             lm, rm;
    static uint8_t              keymap[KEYMAP_LENGTH];
    split_link_t                ll = {.peer = &right}, rl = {.peer = &left};
    working_link_t              lw = {.peer = &right}, rw = {.peer = &left};
    noah_profile_working_splice_t create = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .insert = KEYMAP_LENGTH, .flags = NOAH_PROFILE_WORKING_SPLICE_CREATE};
    uint32_t now = 1000u, started;

    erased_memory(&lm);
    erased_memory(&rm);
    boot_live_pair(&left, &lm, &right, &rm, &ll, &rl, &lw, &rw, 0x5708u, &now);
    scan_until_converged(&left, &right, &now);
    fill_keymap(keymap, 5u);
    assert(live_edit(&left, &right, 1u, &create, keymap, KEYMAP_LENGTH, &now) == NOAH_PROFILE_WORKING_OK);
    scan_until_converged(&left, &right, &now);
    activation_block_reason = NOAH_PROFILE_ACTIVATION_REASON_PERSISTENT_INTENT;
    assert(noah_profile_owner_save(&left, 40u, left.working.session, left.working.revision) == NOAH_PROFILE_WORKING_QUEUED);
    started = now;
    while (now - started < 20000u) {
        scan_pair(&left, &right, &now);
        assert_neither_stopped(&left, &right);
    }
    assert(left.save.phase != NOAH_PROFILE_OWNER_SAVE_IDLE && left.host_barrier.waiting_boundary);
    activation_block_reason = 0u;
    for (uint32_t guard = 0u; guard < 20000u && left.save.phase != NOAH_PROFILE_OWNER_SAVE_IDLE; guard++) {
        scan_pair(&left, &right, &now);
        assert_neither_stopped(&left, &right);
    }
    assert(left.save.last_result == NOAH_PROFILE_WORKING_OK);
    puts("a save waiting 20 s for a safe boundary is never reported stopped");
}

static void init_live_half(noah_profile_owner_t *owner, memory_t *memory, split_link_t *link, working_link_t *working_link, uint8_t origin, uint32_t session) {
    assert(noah_profile_owner_init(owner, &(noah_profile_owner_config_t){
                                              .store_io                  = {.read = memory_read, .write = memory_write, .context = memory},
                                              .split_exchange            = split_exchange,
                                              .split_transport_context   = link,
                                              .origin_half               = origin,
                                              .peer_required             = true,
                                              .working_bytes             = origin ? right_working : left_working,
                                              .working_capacity          = sizeof(left_working),
                                              .working_session           = session,
                                              .working_exchange          = working_exchange,
                                              .working_transport_context = working_link,
                                          }));
}

static void edit_then_save(noah_profile_owner_t *left, noah_profile_owner_t *right, uint32_t *now) {
    noah_profile_working_splice_t key = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .offset = 130u, .remove = 2u, .insert = 2u};
    const uint8_t                 kc[2] = {0x05u, 0x00u};

    assert(live_edit(left, right, 41u, &key, kc, sizeof(kc), now) == NOAH_PROFILE_WORKING_OK);
    scan_until_converged(left, right, now);
    assert(noah_profile_owner_save(left, 42u, left->working.session, left->working.revision) == NOAH_PROFILE_WORKING_QUEUED);
}

static bool pair_settled(noah_profile_owner_t *left, noah_profile_owner_t *right) {
    return left->state == NOAH_PROFILE_OWNER_READY_VALIDATED && right->state == NOAH_PROFILE_OWNER_READY_VALIDATED && left->save.phase == NOAH_PROFILE_OWNER_SAVE_IDLE && left->committed_descriptor.generation == right->committed_descriptor.generation && both_halves_converged(left);
}

// What a half has saved: a whole record of generation 1 or 2, and a saved
// profile that reads as that generation's bytes.
static uint32_t assert_whole_saved(const noah_profile_owner_t *owner, const uint8_t *before, const uint8_t *after, uint32_t length) {
    static uint8_t                    bytes[NOAH_PROFILE_PAYLOAD_MAX];
    noah_effective_profile_snapshot_t saved;
    uint32_t                          generation = owner->committed_descriptor.generation;

    assert(owner->state == NOAH_PROFILE_OWNER_READY_VALIDATED && (generation == 1u || generation == 2u) && owner->store.committed.generation == generation);
    saved = owner->working.active ? owner->working.saved : (noah_effective_profile_snapshot_t){0};
    if (!owner->working.active) {
        assert(noah_effective_profile_provider_copy_active(&owner->provider, &saved) == NOAH_EFFECTIVE_PROFILE_OK);
    }
    assert(saved.identity.generation == generation);
    for (uint32_t offset = 0u; offset < length; offset += 20u) {
        assert(noah_effective_profile_snapshot_read(&saved, offset, bytes + offset, length - offset < 20u ? length - offset : 20u));
    }
    assert(memcmp(bytes, generation == 1u ? before : after, length) == 0);
    return generation;
}

// The restart is a reset between scans, as unplugging is between flash
// operations (marker-last, logical-profile-transaction-v1.md "Power-Loss
// Outcomes"). Restarted at sampled scans across a whole save, both halves
// come back on one profile, the saved one or the new one, whole. The USB
// half restarted alone (the other half did not acknowledge) leaves each half
// a whole saved profile too; the halves agree once the other half restarts
// as well. Until then they may stay apart: a prepared copy the other half
// holds for a sender that restarted before its commit marker is released by
// nothing (an open issue of the split protocol, D-F34), which is why a
// client tells the user to unplug when the other half did not acknowledge.
static void test_a_restart_anywhere_in_a_save_keeps_a_whole_profile(void) {
    static noah_profile_owner_t left, right;
    static memory_t             lm, rm, saved_lm, saved_rm;
    static uint8_t              keymap[KEYMAP_LENGTH], before[NOAH_PROFILE_PAYLOAD_MAX], after[NOAH_PROFILE_PAYLOAD_MAX];
    split_link_t                ll = {.peer = &right}, rl = {.peer = &left};
    working_link_t              lw = {.peer = &right}, rw = {.peer = &left};
    noah_profile_working_splice_t create = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .insert = KEYMAP_LENGTH, .flags = NOAH_PROFILE_WORKING_SPLICE_CREATE};
    uint32_t now = 1000u, length, steps = 0u, session = 0x6000u;
    unsigned samples = 0u, newer = 0u, apart = 0u;

    // Generation 1, saved on both halves.
    erased_memory(&lm);
    erased_memory(&rm);
    boot_live_pair(&left, &lm, &right, &rm, &ll, &rl, &lw, &rw, session++, &now);
    scan_until_converged(&left, &right, &now);
    fill_keymap(keymap, 7u);
    assert(live_edit(&left, &right, 1u, &create, keymap, KEYMAP_LENGTH, &now) == NOAH_PROFILE_WORKING_OK);
    scan_until_converged(&left, &right, &now);
    assert(noah_profile_owner_save(&left, 40u, left.working.session, left.working.revision) == NOAH_PROFILE_WORKING_QUEUED);
    for (uint32_t guard = 0u; guard < 40000u && !pair_settled(&left, &right); guard++) scan_pair(&left, &right, &now);
    assert(pair_settled(&left, &right) && left.committed_descriptor.generation == 1u);
    length = working_status_of(&left).local.length;
    read_active(&left, before, length);
    saved_lm = lm;
    saved_rm = rm;
    // How many scans a save of one more edit takes, start to settled.
    edit_then_save(&left, &right, &now);
    read_active(&left, after, length);
    assert(memcmp(before, after, length) != 0);
    for (; steps < 40000u && !(pair_settled(&left, &right) && left.committed_descriptor.generation == 2u); steps++) scan_pair(&left, &right, &now);
    assert(left.save.last_result == NOAH_PROFILE_WORKING_OK && left.committed_descriptor.generation == 2u);

    for (unsigned usb_only = 0u; usb_only < 2u; usb_only++) {
        for (uint32_t cut = 0u; cut <= steps; cut += steps / 24u + 1u) {
            uint32_t generation;

            lm = saved_lm;
            rm = saved_rm;
            boot_live_pair(&left, &lm, &right, &rm, &ll, &rl, &lw, &rw, session++, &now);
            scan_until_converged(&left, &right, &now);
            edit_then_save(&left, &right, &now);
            for (uint32_t scan = 0u; scan < cut; scan++) {
                scan_pair(&left, &right, &now);
                assert_neither_stopped(&left, &right);
            }
            if (usb_only) {
                init_live_half(&left, &lm, &ll, &lw, 0u, session++);
                // Well past every lease and retry: 100 s.
                for (uint32_t scan = 0u; scan < 100000u / NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS && !pair_settled(&left, &right); scan++) scan_pair(&left, &right, &now);
                assert_neither_stopped(&left, &right);
                (void)assert_whole_saved(&left, before, after, length);
                (void)assert_whole_saved(&right, before, after, length);
                if (!pair_settled(&left, &right)) {
                    apart++;
                    boot_live_pair(&left, &lm, &right, &rm, &ll, &rl, &lw, &rw, session++, &now);
                }
            } else {
                boot_live_pair(&left, &lm, &right, &rm, &ll, &rl, &lw, &rw, session++, &now);
            }
            for (uint32_t guard = 0u; guard < 40000u && !pair_settled(&left, &right); guard++) scan_pair(&left, &right, &now);
            assert(pair_settled(&left, &right));
            generation = assert_whole_saved(&left, before, after, length);
            assert(assert_whole_saved(&right, before, after, length) == generation);
            assert(!left.working.active && !right.working.active);
            assert_neither_stopped(&left, &right);
            assert_same_active(&left, &right, generation == 1u ? before : after, length);
            newer += generation == 2u;
            samples++;
        }
    }
    assert(newer != 0u && newer != samples);
    printf("a restart at %u points of a %u-scan save left one whole profile on both halves (the new one %u times); restarted alone, the USB half left them apart until the other half restarted %u times\n", samples, (unsigned)steps, newer, apart);
}

// The other half's view of the USB half after the decision changes under it:
// a newer generation is lost authority, the same generation from the other
// half a concurrent commit. Both stop the owner.
static void stop_after_the_decision(uint32_t peer_generation, noah_profile_owner_state_t expected) {
    static noah_profile_owner_t left, right;
    static memory_t             lm, rm;
    split_link_t                ll = {.peer = &right}, rl = {.peer = &left};
    uint8_t                     frame[32];
    uint32_t                    now = 90000u;

    erased_memory(&lm);
    erased_memory(&rm);
    boot_empty_pair(&left, &lm, &right, &rm, &ll, &rl, &now);
    stage_valid_candidate_pair(&left, &right, 110u, &now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 110u);
    send_and_scan_pair(&left, &right, frame, &now);
    for (uint32_t guard = 0u; guard < 2048u && left.store.committed.generation != 1u; guard++) scan_pair(&left, &right, &now);
    for (uint32_t guard = 0u; guard < 32u && (!left.host_barrier.local_published || !left.host_barrier.peer_commit_authorized); guard++) {
        left.scheduler_cursor = 0u;
        assert(noah_profile_owner_scan(&left, true, now++));
    }
    assert(left.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_CONVERGING_PEER);
    assert(noah_profile_split_authority_publish(&left.reconciler.authority, left.committed_descriptor, peer_descriptor(&left, peer_generation), false));
    left.scheduler_cursor = 0u;
    assert(noah_profile_owner_scan(&left, true, now++));
    assert(left.state == expected);
    assert_stays_stopped(&left, &left, NULL, &now);
}

static void test_authority_changing_after_the_decision_stops_the_owner(void) {
    stop_after_the_decision(2u, NOAH_PROFILE_OWNER_POSTCOMMIT_AUTHORITY_LOST);
    stop_after_the_decision(1u, NOAH_PROFILE_OWNER_CONCURRENT_COMMIT);
    puts("lost authority and a concurrent commit after the decision stop the owner");
}

static void test_a_failed_runtime_install_stops_the_owner(void) {
    static noah_profile_owner_t owner;
    static memory_t             memory;
    uint32_t                    now = 100u;

    erased_memory(&memory);
    rgb_install_allowed = false;
    boot_single(&owner, &memory, 0u, &now);
    rgb_install_allowed = true;
    assert(owner.state == NOAH_PROFILE_OWNER_INTEGRATION_ERROR);
    assert_stays_stopped(&owner, &owner, NULL, &now);
    puts("a runtime that cannot install stops the owner");
}

static void test_stopped_owners_and_restarts(void) {
    test_only_states_a_restart_leaves_are_stopped();
    test_a_storage_failure_in_a_save_stops_the_usb_half_and_says_so();
    test_a_half_that_cannot_read_its_slots_stops_and_is_heard();
    test_an_unknown_commit_marker_stops_until_a_restart_reads_it();
    test_two_profiles_of_one_generation_stop_the_owner();
    test_unreadable_compiled_defaults_stop_the_owner();
    test_authority_changing_after_the_decision_stops_the_owner();
    test_a_failed_runtime_install_stops_the_owner();
    test_a_slow_save_is_never_stopped();
    test_a_restart_anywhere_in_a_save_keeps_a_whole_profile();
}
