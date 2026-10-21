// Working copy split sync (D-F32): two halves, each with its own provider
// over the same saved profile, joined by the RPC the USB half drives, over
// the in-memory fault link.

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "split_fault_link.h"
#include "users/noah/lib/profile/split/profile_working_sync.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"

_Static_assert(NOAH_PROFILE_WORKING_SYNC_FRAME_MAX <= SPLIT_FAULT_LINK_FRAME_MAX, "the fault link carries the largest working-sync frame");

enum {
    ACTION_ABI_DIGEST = 0x2468ACE0u,
    CAPACITY          = NOAH_PROFILE_BLOB_V1_MAX_SIZE,
    POSITIONS         = 60u,
    KEYMAP_KEY        = 2u * 61u,
};

typedef struct {
    uint8_t                           bytes[CAPACITY];
    noah_effective_profile_provider_t provider;
    noah_profile_working_copy_t       copy;
    noah_profile_working_sync_t       sync;
    noah_profile_edit_mode_record_t   stored_mode;
    bool                              mode_stored;
    bool                              noisy;      // keys held: no write may run
    bool                              store_fails;
    uint32_t                          mode_writes;
    uint32_t                          mode_attempts;
} half_t;

static uint8_t                           saved_bytes[CAPACITY];
static uint32_t                          saved_length;
static noah_profile_validator_v1_compatibility_t rules;
static noah_profile_validator_v1_compatibility_t strict_rules;
static noah_effective_profile_snapshot_t saved_snapshot;
static half_t                            usb;
static half_t                            other;
static uint32_t                          now;
// The USB half's link to the other half.
static split_fault_link_t                link;
// This exchange answers with a sealed reply of its request's kind, one
// sequence behind (the answer to an earlier request), never delivering it.
static uint32_t                          stale_reply_at;

static uint32_t no_reasons(void *context) {
    (void)context;
    return 0u;
}

static void no_invalidator(void *context, uint32_t publication, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *view) {
    (void)context;
    (void)publication;
    (void)previous;
    (void)active;
    (void)view;
}

static bool mode_load(void *context, noah_profile_edit_mode_record_t *record) {
    half_t *half = context;

    if (!half->mode_stored) {
        return false;
    }
    *record = half->stored_mode;
    return true;
}

static bool mode_store(void *context, const noah_profile_edit_mode_record_t *record) {
    half_t *half = context;

    half->mode_attempts++;
    if (half->store_fails) {
        return false;
    }
    half->stored_mode = *record;
    half->mode_stored = true;
    half->mode_writes++;
    return true;
}

static bool mode_may_store(void *context) {
    return !((half_t *)context)->noisy;
}

// The other half's RPC callback body.
static bool other_receive(void *context, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    return noah_profile_working_sync_receive(&((half_t *)context)->sync, request, request_length, response, response_length);
}

static bool usb_exchange(void *context, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    split_fault_link_t *usb_link = context;

    if (stale_reply_at != 0u && stale_reply_at == usb_link->exchanges + 1u) {
        uint16_t sequence = (uint16_t)((uint16_t)(request[1] | request[2] << 8u) - 1u);

        memset(usb_link->injected, 0, sizeof(usb_link->injected));
        usb_link->injected[0] = (uint8_t)(request[0] | 0x80u);
        usb_link->injected[1] = (uint8_t)sequence;
        usb_link->injected[2] = (uint8_t)(sequence >> 8u);
        noah_profile_working_sync_seal(usb_link->injected, NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE);
        usb_link->injected_length = NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE;
        usb_link->inject          = stale_reply_at;
        stale_reply_at            = 0u;
    }
    return split_fault_link_exchange(usb_link, request, request_length, response, response_length);
}

static void load_fixture(const char *path) {
    FILE *file = fopen(path, "rb");

    assert(file);
    saved_length = (uint32_t)fread(saved_bytes, 1u, sizeof(saved_bytes), file);
    fclose(file);
}

static noah_profile_validator_v1_profile_t validate_whole(void) {
    noah_profile_reader_t                   reader = noah_profile_reader_from_memory(saved_bytes, saved_length);
    noah_profile_validator_v1_t             validator;
    noah_profile_validator_v1_profile_t     profile;
    noah_profile_validator_v1_declaration_t declaration = {
        .schema_major      = NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR,
        .schema_minor      = NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR,
        .domain_mask       = NOAH_PROFILE_DOMAIN_MASK_ALL,
        .byte_length       = saved_length,
        .crc32             = noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, saved_bytes, saved_length)),
        .digest            = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, saved_bytes, saved_length),
        .action_abi_digest = ACTION_ABI_DIGEST,
    };
    noah_profile_validator_v1_result_t result = noah_profile_validator_v1_begin(&validator, &reader, 0u, &declaration, &rules, NULL);

    while (result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS) {
        result = noah_profile_validator_v1_step(&validator, NOAH_PROFILE_VALIDATOR_V1_STEP_READ_MAX, NULL);
    }
    assert(result == NOAH_PROFILE_VALIDATOR_V1_VALID && noah_profile_validator_v1_profile(&validator, &profile, NULL) == NOAH_PROFILE_VALIDATOR_V1_VALID);
    return profile;
}

static void setup_half(half_t *half, uint32_t session, bool usb_half, const noah_profile_validator_v1_compatibility_t *peer_rules) {
    noah_effective_profile_invalidator_t invalidator = {.callback = no_invalidator};
    noah_profile_edit_mode_storage_t     storage     = {.load = mode_load, .store = mode_store, .may_store = mode_may_store, .context = half};

    assert(noah_effective_profile_provider_init(&half->provider, &saved_snapshot, no_reasons, NULL, &invalidator, 1u) == NOAH_EFFECTIVE_PROFILE_OK);
    noah_profile_working_copy_init(&half->copy, half->bytes, CAPACITY, &half->provider, &rules, peer_rules, session);
    noah_profile_working_sync_init(&half->sync, &half->copy, usb_half ? usb_exchange : NULL, usb_half ? &link : NULL, &storage);
}

static void setup(void) {
    noah_profile_validator_v1_profile_t profile;
    noah_profile_reader_t               reader = noah_profile_reader_from_memory(saved_bytes, saved_length);

    rules                            = noah_profile_validator_v1_default_compatibility(ACTION_ABI_DIGEST);
    rules.placement_positions        = POSITIONS;
    strict_rules                     = rules;
    strict_rules.max_blob_size       = saved_length; // refuses any growth
    profile                          = validate_whole();
    assert(noah_effective_profile_snapshot_make_compiled(&profile, &reader, 0u, &saved_snapshot) == NOAH_EFFECTIVE_PROFILE_OK);
    memset(&usb, 0, sizeof(usb));
    memset(&other, 0, sizeof(other));
    setup_half(&usb, 0x1111u, true, &rules);
    setup_half(&other, 0x2222u, false, &rules);
    now            = 1000u;
    stale_reply_at = 0u;
    link           = (split_fault_link_t){.receive = other_receive, .peer = &other};
}

static void run(uint32_t scans) {
    for (uint32_t scan = 0u; scan < scans; scan++, now++) {
        (void)noah_profile_working_sync_master_scan(&usb.sync, now);
        (void)noah_profile_working_sync_peer_scan(&other.sync);
        (void)noah_profile_working_copy_scan(&usb.copy);
        (void)noah_profile_working_copy_scan(&other.copy);
        (void)noah_profile_working_sync_store_mode(&usb.sync, now);
        (void)noah_profile_working_sync_store_mode(&other.sync, now);
    }
}

static bool run_until_converged(uint32_t limit) {
    for (uint32_t scan = 0u; scan < limit; scan += 10u) {
        run(10u);
        if (noah_profile_working_sync_converged(&usb.sync, now)) {
            return true;
        }
    }
    return false;
}

static uint32_t crc_of(const uint8_t *bytes, uint32_t length) {
    return noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, bytes, length));
}

static void usb_edit(uint16_t transaction, const noah_profile_working_splice_t *splice, const uint8_t *bytes, uint32_t length) {
    assert(noah_profile_working_copy_begin(&usb.copy, transaction, usb.copy.session, usb.copy.revision, 1u, length, 0u, false) == NOAH_PROFILE_WORKING_OK);
    assert(noah_profile_working_copy_splice(&usb.copy, transaction, 0u, splice) == NOAH_PROFILE_WORKING_OK);
    if (length) {
        assert(noah_profile_working_copy_data(&usb.copy, transaction, 0u, bytes, length) == NOAH_PROFILE_WORKING_OK);
    }
    assert(noah_profile_working_copy_commit(&usb.copy, transaction, crc_of(bytes, length)) == NOAH_PROFILE_WORKING_QUEUED);
    for (uint32_t scan = 0u; scan < 4096u && noah_profile_working_copy_busy(&usb.copy); scan++) {
        (void)noah_profile_working_copy_scan(&usb.copy);
    }
    assert(usb.copy.last_result == NOAH_PROFILE_WORKING_OK);
}

static void assert_same_profile(void) {
    static uint8_t                    left[CAPACITY];
    static uint8_t                    right[CAPACITY];
    noah_profile_working_status_t     a;
    noah_profile_working_status_t     b;

    assert(noah_profile_working_copy_status(&usb.copy, &a) && noah_profile_working_copy_status(&other.copy, &b));
    assert(a.length == b.length && a.session == b.session && a.revision == b.revision && a.active == b.active);
    assert(noah_profile_working_copy_read(&usb.copy, 0u, left, a.length) && noah_profile_working_copy_read(&other.copy, 0u, right, b.length));
    assert(memcmp(left, right, a.length) == 0);
}

static void test_adopt_edit_revert(void) {
    const uint8_t                 key[2] = {0x04u, 0x00u};
    const uint8_t                 x      = 'x';
    noah_profile_working_splice_t keymap = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .offset = KEYMAP_KEY, .remove = 2u, .insert = 2u};
    noah_profile_working_splice_t macros = {.domain_id = NOAH_PROFILE_DOMAIN_V1_MACROS, .offset = 0u, .insert = 1u};

    setup();
    assert(!noah_profile_working_sync_converged(&usb.sync, now));
    // Both on the same saved profile: the other half takes the USB half's
    // identity, with nothing copied.
    assert(run_until_converged(200u));
    assert(other.copy.session == 0x1111u && other.copy.revision == 0u && !other.copy.active);
    // An edit is sent as the same edit and lands at the same revision.
    usb_edit(1u, &keymap, key, sizeof(key));
    assert(!noah_profile_working_sync_converged(&usb.sync, now));
    assert(run_until_converged(400u));
    assert(other.copy.active && other.copy.revision == 1u);
    assert_same_profile();
    // A resizing edit, then Revert on the USB half: the other half follows.
    usb_edit(2u, &macros, &x, 1u);
    assert(run_until_converged(400u));
    assert_same_profile();
    assert(noah_profile_working_copy_revert(&usb.copy, 3u, usb.copy.session, usb.copy.revision) == NOAH_PROFILE_WORKING_QUEUED);
    run(5u);
    assert(!usb.copy.active);
    assert(run_until_converged(400u));
    assert(!other.copy.active && other.copy.revision == 3u);
    assert_same_profile();
    puts("the other half adopts, follows each edit and follows Revert");
}

static void test_reset_other_half_is_sent_the_whole_copy(void) {
    const uint8_t                 key[2] = {0x05u, 0x00u};
    noah_profile_working_splice_t keymap = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .offset = KEYMAP_KEY, .remove = 2u, .insert = 2u};
    noah_profile_working_splice_t other_key = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .offset = KEYMAP_KEY + 2u, .remove = 2u, .insert = 2u};
    uint32_t                      before;

    setup();
    assert(run_until_converged(200u));
    usb_edit(1u, &keymap, key, sizeof(key));
    usb_edit(2u, &other_key, key, sizeof(key)); // two edits ahead before it hears
    // The other half restarts (its own session, its saved profile): it is
    // brought back with the whole working copy.
    memset(&other, 0, sizeof(other));
    setup_half(&other, 0x3333u, false, &rules);
    before = link.exchanges;
    assert(run_until_converged(20000u));
    assert(other.copy.active && other.copy.session == 0x1111u && other.copy.revision == 2u);
    assert_same_profile();
    printf("a restarted other half is sent the whole working copy (%u exchanges)\n", (unsigned)(link.exchanges - before));
}

static void test_restart_after_agreeing_on_an_edit(void) {
    const uint8_t                 first[2]  = {0x07u, 0x00u};
    const uint8_t                 second[2] = {0x08u, 0x00u};
    noah_profile_working_splice_t keymap    = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .offset = KEYMAP_KEY, .remove = 2u, .insert = 2u};
    noah_profile_working_splice_t other_key = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .offset = KEYMAP_KEY + 2u, .remove = 2u, .insert = 2u};

    setup();
    assert(run_until_converged(200u));
    usb_edit(1u, &keymap, first, sizeof(first));
    assert(run_until_converged(400u));
    // The halves agreed on an unsaved edit; the other half restarts before
    // the next one reaches it. Adopting puts it on its saved bytes at the
    // revision just before: only the whole copy brings it back, never the
    // last edit alone on top of the saved bytes.
    usb_edit(2u, &other_key, second, sizeof(second));
    memset(&other, 0, sizeof(other));
    setup_half(&other, 0x4444u, false, &rules);
    assert(run_until_converged(20000u));
    assert(other.copy.active && other.copy.session == 0x1111u && other.copy.revision == 2u);
    assert_same_profile();
    puts("a half restarted after agreeing on an edit gets every unsaved edit back");
}

static void test_revert_during_a_forward_does_not_stall(void) {
    const uint8_t                 key[2] = {0x09u, 0x00u};
    noah_profile_working_splice_t keymap = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .offset = KEYMAP_KEY, .remove = 2u, .insert = 2u};
    uint32_t                      started;

    setup();
    assert(run_until_converged(200u));
    usb_edit(1u, &keymap, key, sizeof(key));
    run(1u);
    assert(usb.sync.forward_phase != NOAH_PROFILE_WORKING_FORWARD_NONE && usb.sync.forward_phase != NOAH_PROFILE_WORKING_FORWARD_WAIT);
    // Revert lands while the edit is on its way: the attempt ends, the
    // other half's half-received edit is aborted, and it follows Revert well
    // inside the 3 s a lost half takes.
    assert(noah_profile_working_copy_revert(&usb.copy, 2u, usb.copy.session, usb.copy.revision) == NOAH_PROFILE_WORKING_QUEUED);
    started = now;
    assert(run_until_converged(500u));
    assert(now - started < 500u && !other.copy.active && other.copy.phase == NOAH_PROFILE_WORKING_PHASE_IDLE);
    assert_same_profile();
    puts("Revert during a forward ends it at once and the other half follows");
}

static void test_lost_replies_still_converge(void) {
    const uint8_t                 key[2] = {0x06u, 0x00u};
    noah_profile_working_splice_t keymap = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .offset = KEYMAP_KEY, .remove = 2u, .insert = 2u};

    setup();
    link.drop_every = 3u;
    assert(run_until_converged(2000u));
    usb_edit(1u, &keymap, key, sizeof(key));
    assert(run_until_converged(4000u));
    assert_same_profile();
    puts("lost replies are retried and still converge");
}

typedef enum {
    FAULT_DROP_BEFORE_DELIVERY,
    FAULT_DROP_AFTER_DELIVERY,
    FAULT_REPLAY_PREVIOUS,
    FAULT_GARBLE,
    FAULT_CORRUPT_REPLY,
    FAULT_STALE_REPLY,
    FAULT_KINDS,
} fault_t;

static const char *const fault_names[FAULT_KINDS] = {"request lost", "reply lost", "previous reply replayed", "request garbled", "reply corrupted", "stale reply"};

// One edit that inserts a byte, with `fault` planted at exchange `at` of its
// forward (0 plants none). Returns the exchanges the edit took and whether
// the fault fired. Inserted twice, the byte would leave the halves apart.
static uint32_t edit_with_fault(fault_t fault, uint32_t at, bool *fired) {
    const uint8_t                 x      = 'x';
    noah_profile_working_splice_t macros = {.domain_id = NOAH_PROFILE_DOMAIN_V1_MACROS, .offset = 0u, .insert = 1u};
    uint32_t                      revision;
    uint32_t                      base;

    setup();
    assert(run_until_converged(200u));
    revision = other.copy.revision;
    usb_edit(1u, &macros, &x, 1u);
    base = link.exchanges;
    if (at != 0u) {
        switch (fault) {
            case FAULT_DROP_BEFORE_DELIVERY: link.drop_before_delivery = base + at; break;
            case FAULT_DROP_AFTER_DELIVERY: link.drop_after_delivery = base + at; break;
            case FAULT_REPLAY_PREVIOUS: link.replay_previous = base + at; break;
            case FAULT_GARBLE: link.garble = base + at; break;
            case FAULT_STALE_REPLY: stale_reply_at = base + at; break;
            case FAULT_CORRUPT_REPLY:
                // The next delivered reply: armed just before exchange `at`.
                for (uint32_t scan = 0u; scan < 400u && link.exchanges + 1u < base + at; scan++) run(1u);
                assert(link.exchanges + 1u == base + at);
                link.corrupt_next_reply = true;
                break;
            case FAULT_KINDS: assert(!"no such fault"); break;
        }
    }
    if (!run_until_converged(1000u)) {
        fprintf(stderr, "%s at exchange %u of an edit: the halves did not converge\n", fault_names[fault], (unsigned)at);
        assert(!"one fault wedged an edit");
    }
    assert_same_profile();
    assert(usb.copy.revision == revision + 1u && other.copy.revision == revision + 1u);
    assert(other.copy.active && other.copy.last_result == NOAH_PROFILE_WORKING_OK);
    *fired = link.drop_before_delivery == 0u && link.drop_after_delivery == 0u && link.replay_previous == 0u && link.garble == 0u && link.inject == 0u && stale_reply_at == 0u && !link.corrupt_next_reply;
    return link.exchanges - base;
}

// Each way the link fails, planted at every exchange of one edit's forward in
// turn: the edit lands on the other half once, at the next revision. A request
// sent again is answered from its reply, matched by kind and sequence, never
// taken twice; anything else that comes back is refused and the request sent
// again.
static void test_one_fault_anywhere_in_an_edit_lands_it_once(void) {
    bool     fired;
    uint32_t exchanges = edit_with_fault(FAULT_DROP_BEFORE_DELIVERY, 0u, &fired);

    assert(exchanges >= 8u); // BEGIN, SPLICE, DATA and COMMIT, each admitted, then answered
    for (unsigned fault = 0u; fault < FAULT_KINDS; fault++) {
        for (uint32_t at = 1u; at <= exchanges; at++) {
            (void)edit_with_fault((fault_t)fault, at, &fired);
            assert(fired);
        }
    }
    printf("one fault at any of an edit's %u exchanges still lands it once, for each of %u faults\n", (unsigned)exchanges, (unsigned)FAULT_KINDS);
}

// The link drops in the middle of an edit for longer than the timeout: the
// USB half forgets the other half and what they agreed on, and brings it
// back once the link returns.
static void test_a_link_lost_past_the_timeout_brings_the_other_half_back(void) {
    const uint8_t                 x      = 'x';
    noah_profile_working_splice_t macros = {.domain_id = NOAH_PROFILE_DOMAIN_V1_MACROS, .offset = 0u, .insert = 1u};

    setup();
    assert(run_until_converged(200u));
    usb_edit(1u, &macros, &x, 1u);
    run(1u);
    assert(usb.sync.forward_phase != NOAH_PROFILE_WORKING_FORWARD_NONE && usb.sync.forward_phase != NOAH_PROFILE_WORKING_FORWARD_WAIT);
    link.disconnected = true;
    run(NOAH_PROFILE_WORKING_SYNC_PEER_TIMEOUT_MS + 10u);
    assert(!usb.sync.peer.known && !usb.sync.agreed && !noah_profile_working_sync_converged(&usb.sync, now));
    link.disconnected = false;
    assert(run_until_converged(20000u));
    assert(other.copy.active && other.copy.session == usb.copy.session && other.copy.revision == usb.copy.revision);
    assert_same_profile();
    puts("a link lost past the timeout forgets the other half, which is brought back when it returns");
}

static void test_an_edit_the_other_half_refuses_stops(void) {
    const uint8_t                 x      = 'x';
    noah_profile_working_splice_t macros = {.domain_id = NOAH_PROFILE_DOMAIN_V1_MACROS, .offset = 0u, .insert = 1u};

    setup();
    memset(&other, 0, sizeof(other));
    setup_half(&other, 0x2222u, false, &strict_rules);
    assert(run_until_converged(200u));
    usb_edit(1u, &macros, &x, 1u);
    // The other half refuses the growth, and then the whole copy, tried
    // once: the halves stay apart, reported, with no endless resending.
    assert(!run_until_converged(3000u));
    assert(usb.sync.forward_failed_whole && noah_profile_working_sync_refused(&usb.sync));
    {
        uint32_t before = link.exchanges;
        run(3000u);
        assert(link.exchanges - before < 100u);
    }
    assert(!other.copy.active);
    // Revert brings them together again.
    assert(noah_profile_working_copy_revert(&usb.copy, 2u, usb.copy.session, usb.copy.revision) == NOAH_PROFILE_WORKING_QUEUED);
    assert(run_until_converged(400u) && !noah_profile_working_sync_refused(&usb.sync));
    assert_same_profile();
    puts("an edit the other half refuses is tried once whole, then the halves report apart until Revert");
}

static void test_edit_mode_follows_the_higher_counter(void) {
    setup();
    assert(usb.sync.mode.mode == NOAH_PROFILE_EDIT_MODE_LIVE_REVIEW && usb.sync.mode.counter == 0u);
    assert(run_until_converged(200u));
    assert(noah_profile_working_sync_set_mode(&usb.sync, NOAH_PROFILE_EDIT_MODE_APPLY));
    run(200u);
    assert(other.stored_mode.mode == NOAH_PROFILE_EDIT_MODE_APPLY && other.stored_mode.counter == 1u && other.sync.mode.counter == 1u);
    // The other half was set alone (say before a reconnect) with a higher
    // counter: the USB half takes its record.
    other.sync.mode = (noah_profile_edit_mode_record_t){.counter = 5u, .mode = NOAH_PROFILE_EDIT_MODE_LIVE};
    run(2500u);
    assert(usb.sync.mode.counter == 5u && usb.sync.mode.mode == NOAH_PROFILE_EDIT_MODE_LIVE && usb.stored_mode.mode == NOAH_PROFILE_EDIT_MODE_LIVE);
    // Equal counters that disagree are a conflict a new choice resolves.
    other.sync.mode = (noah_profile_edit_mode_record_t){.counter = 5u, .mode = NOAH_PROFILE_EDIT_MODE_APPLY};
    run(2500u);
    assert(usb.sync.mode_conflict);
    assert(noah_profile_working_sync_set_mode(&usb.sync, NOAH_PROFILE_EDIT_MODE_LIVE_REVIEW) && usb.sync.mode.counter == 6u);
    run(200u);
    assert(!usb.sync.mode_conflict && other.sync.mode.counter == 6u && other.sync.mode.mode == NOAH_PROFILE_EDIT_MODE_LIVE_REVIEW);
    puts("the edit mode follows the higher counter; equal counters that disagree are reported");
}

static void test_edit_mode_is_written_on_a_quiet_keyboard(void) {
    setup();
    assert(run_until_converged(200u));
    // Keys held on both halves: the mode holds and travels at once, but
    // nothing is written, on either half, until the keyboard is quiet.
    usb.noisy   = true;
    other.noisy = true;
    assert(noah_profile_working_sync_set_mode(&usb.sync, NOAH_PROFILE_EDIT_MODE_APPLY));
    run(300u);
    assert(usb.sync.mode.counter == 1u && other.sync.mode.counter == 1u && other.sync.mode.mode == NOAH_PROFILE_EDIT_MODE_APPLY);
    assert(usb.mode_attempts == 0u && other.mode_attempts == 0u && usb.sync.mode_store_pending);
    usb.noisy   = false;
    other.noisy = false;
    run(5u);
    assert(usb.mode_writes == 1u && other.mode_writes == 1u && other.stored_mode.counter == 1u && !usb.sync.mode_store_pending);
    // A failing write is not retried on every poll: it backs off, and the
    // failure is reported until a later write succeeds.
    usb.store_fails = true;
    assert(noah_profile_working_sync_set_mode(&usb.sync, NOAH_PROFILE_EDIT_MODE_LIVE));
    run(900u);
    assert(usb.mode_attempts == 2u && usb.sync.mode_store_failed);
    run(2500u);
    assert(usb.mode_attempts == 4u && usb.sync.mode_store_failed);
    usb.store_fails = false;
    run(4100u);
    assert(!usb.sync.mode_store_failed && !usb.sync.mode_store_pending && usb.stored_mode.mode == NOAH_PROFILE_EDIT_MODE_LIVE);
    puts("the edit mode is written only on a quiet keyboard, and a failing write backs off");
}

// A half whose owner stopped (D-F34) answers every poll saying so and acts on
// nothing; the USB half only polls it, once a second, and tells its status.
static void test_a_stopped_half_says_so_and_takes_nothing(void) {
    const uint8_t                 key[2] = {0x06u, 0x00u};
    noah_profile_working_splice_t keymap = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .offset = KEYMAP_KEY, .remove = 2u, .insert = 2u};
    uint32_t                      before;

    setup();
    other.sync.stopped = true;
    usb_edit(1u, &keymap, key, sizeof(key));
    run(50u);
    assert(noah_profile_working_sync_peer_stopped(&usb.sync, now) && !noah_profile_working_sync_converged(&usb.sync, now));
    assert(usb.sync.peer.flags & NOAH_PROFILE_WORKING_SYNC_PEER_STOPPED);
    // Nothing was adopted, reverted or edited on the stopped half.
    assert(other.copy.session == 0x2222u && other.copy.revision == 0u && !other.copy.active);
    before = link.exchanges;
    run(10000u);
    assert(link.exchanges - before <= 2u * 11u && !other.copy.active && other.copy.session == 0x2222u);
    assert(noah_profile_working_sync_peer_stopped(&usb.sync, now));
    // Not heard from, it is no longer known to have stopped.
    other.sync.stopped = false;
    link.disconnected  = true;
    run(NOAH_PROFILE_WORKING_SYNC_PEER_TIMEOUT_MS + 10u);
    assert(!noah_profile_working_sync_peer_stopped(&usb.sync, now));
    // After a restart it is brought back as any other half is.
    link.disconnected = false;
    assert(run_until_converged(20000u));
    assert_same_profile();
    puts("a stopped half says so on every reply and takes nothing; the USB half only polls it");
}

static void test_frames(void) {
    uint8_t frame[NOAH_PROFILE_WORKING_SYNC_FRAME_MAX] = {NOAH_PROFILE_WORKING_SYNC_STATUS};
    uint8_t reply[NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE];

    setup();
    noah_profile_working_sync_seal(frame, NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE);
    assert(noah_profile_working_sync_frame_length(frame, sizeof(frame)) == NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE);
    // The other half takes only a whole frame with room for its reply; the
    // split RPC answers anything it refuses with zeros.
    assert(!noah_profile_working_sync_receive(&other.sync, NULL, NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE, reply, sizeof(reply)));
    assert(!noah_profile_working_sync_receive(&other.sync, frame, NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE, NULL, sizeof(reply)));
    assert(!noah_profile_working_sync_receive(&other.sync, frame, NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE, reply, NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE - 1u));
    assert(!noah_profile_working_sync_receive(&other.sync, frame, NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE - 1u, reply, sizeof(reply)));
    assert(!other.sync.mailbox_full);
    assert(noah_profile_working_sync_receive(&other.sync, frame, NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE, reply, sizeof(reply)) && other.sync.mailbox_full);
    frame[20] ^= 1u;
    assert(!noah_profile_working_sync_receive(&other.sync, frame, NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE, reply, sizeof(reply)));
    frame[0] = NOAH_PROFILE_WORKING_SYNC_DATA;
    frame[17] = 0u;
    assert(noah_profile_working_sync_frame_length(frame, sizeof(frame)) == 0u);
    frame[17] = NOAH_PROFILE_WORKING_SYNC_DATA_MAX + 1u;
    assert(noah_profile_working_sync_frame_length(frame, sizeof(frame)) == 0u);
    frame[17] = NOAH_PROFILE_WORKING_SYNC_DATA_MAX;
    assert(noah_profile_working_sync_frame_length(frame, sizeof(frame)) == NOAH_PROFILE_WORKING_SYNC_FRAME_MAX);
    puts("frames are sized by kind and sealed");
}

int main(int argc, char **argv) {
    assert(argc == 2);
    load_fixture(argv[1]);
    test_frames();
    test_adopt_edit_revert();
    test_reset_other_half_is_sent_the_whole_copy();
    test_restart_after_agreeing_on_an_edit();
    test_revert_during_a_forward_does_not_stall();
    test_lost_replies_still_converge();
    test_one_fault_anywhere_in_an_edit_lands_it_once();
    test_a_link_lost_past_the_timeout_brings_the_other_half_back();
    test_an_edit_the_other_half_refuses_stops();
    test_edit_mode_follows_the_higher_counter();
    test_edit_mode_is_written_on_a_quiet_keyboard();
    test_a_stopped_half_says_so_and_takes_nothing();
    return 0;
}
