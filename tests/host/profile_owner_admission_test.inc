// ── Admission, through Profile Wire (D-F32) ─────────────────────────────────
//
// Which edit, Save, Revert, mode change, Apply frame or keyboard step may
// start, and the answer when it may not, is one statement in the owner
// (owner_admit). These tests reach each of its rows on a real booted pair and
// ask through the wire: the Profile Wire channel (qmk_profile_edit_channel.c)
// is compiled in, and the link seam it reaches the owner through
// (profile_store_runtime.h) is bound to `wire_owner`, so every answer and
// every edit status bit is composed from real owner state.

#include "users/noah/lib/compat/qmk_profile_edit_channel.h"
#include "users/noah/lib/profile/storage/profile_store_runtime.h"

static noah_profile_owner_t *wire_owner;

noah_profile_working_result_t noah_profile_store_runtime_edit_begin(uint16_t transaction_id, uint32_t session, uint32_t revision, uint8_t splice_count, uint32_t insert_total, uint8_t flags) {
    return noah_profile_owner_edit_begin(wire_owner, transaction_id, session, revision, splice_count, insert_total, flags);
}
noah_profile_working_result_t noah_profile_store_runtime_edit_splice(uint16_t transaction_id, uint8_t index, const noah_profile_working_splice_t *splice) {
    return noah_profile_owner_edit_splice(wire_owner, transaction_id, index, splice);
}
noah_profile_working_result_t noah_profile_store_runtime_edit_data(uint16_t transaction_id, uint32_t offset, const uint8_t *bytes, uint32_t count) {
    return noah_profile_owner_edit_data(wire_owner, transaction_id, offset, bytes, count);
}
noah_profile_working_result_t noah_profile_store_runtime_edit_commit(uint16_t transaction_id, uint32_t crc32) {
    return noah_profile_owner_edit_commit(wire_owner, transaction_id, crc32);
}
noah_profile_working_result_t noah_profile_store_runtime_edit_abort(uint16_t transaction_id) {
    return noah_profile_owner_edit_abort(wire_owner, transaction_id);
}
noah_profile_working_result_t noah_profile_store_runtime_save(uint16_t transaction_id, uint32_t session, uint32_t revision) {
    return noah_profile_owner_save(wire_owner, transaction_id, session, revision);
}
noah_profile_working_result_t noah_profile_store_runtime_revert(uint16_t transaction_id, uint32_t session, uint32_t revision) {
    return noah_profile_owner_revert(wire_owner, transaction_id, session, revision);
}
noah_profile_working_result_t noah_profile_store_runtime_set_edit_mode(uint8_t mode) {
    return noah_profile_owner_set_edit_mode(wire_owner, mode);
}
bool noah_profile_store_runtime_working_status(noah_profile_owner_working_status_t *status) {
    return noah_profile_owner_working_status(wire_owner, status);
}
bool noah_profile_store_runtime_read_working(uint32_t offset, uint8_t *target, uint32_t length) {
    return noah_profile_owner_working_read(wire_owner, offset, target, length);
}

// The channel's other calls, which no owner answers.
uint32_t noah_setting(uint8_t id, uint32_t fallback) {
    (void)id;
    return fallback;
}
bool noah_participation_behaviors_override(void) {
    return true;
}
bool noah_qmk_keyboard_restart_request(void) {
    return false;
}

static uint8_t wire[32];

// A SET of `value` for `transaction`, its fields from byte 5, answered by
// `owner`: the result (reply byte 5). The reply stays in `wire`.
static uint8_t wire_set(noah_profile_owner_t *owner, uint8_t value, uint16_t transaction, const uint8_t *fields, uint8_t length) {
    memset(wire, 0, sizeof(wire));
    wire[0] = NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET;
    wire[1] = NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL;
    wire[2] = value;
    write_u16(&wire[3], transaction);
    if (length != 0u) {
        memcpy(&wire[5], fields, length);
    }
    wire_owner = owner;
    assert(noah_qmk_profile_edit_channel(wire, sizeof(wire)));
    return wire[5];
}

// The edit status flags the last SET's reply carried (bytes 15–16).
static uint16_t wire_reply_flags(void) {
    return (uint16_t)(wire[15] | wire[16] << 8u);
}

// Save or Revert (`value`) naming a session and revision.
static uint8_t wire_named(noah_profile_owner_t *owner, uint8_t value, uint16_t transaction, uint32_t session, uint32_t revision) {
    uint8_t fields[8];

    write_u32(&fields[0], session);
    write_u32(&fields[4], revision);
    return wire_set(owner, value, transaction, fields, sizeof(fields));
}

// The begin of an edit of one splice and two inserted bytes.
static uint8_t wire_begin(noah_profile_owner_t *owner, uint16_t transaction, uint32_t session, uint32_t revision) {
    uint8_t fields[14] = {0};

    write_u32(&fields[0], session);
    write_u32(&fields[4], revision);
    fields[8] = 1u;
    write_u32(&fields[10], 2u);
    return wire_set(owner, NOAH_PROFILE_WIRE_V1_VALUE_EDIT_BEGIN, transaction, fields, sizeof(fields));
}

// An edit's later frames, each for `transaction`: a splice (one setting
// replaced), data (two bytes), its commit and an abort.
static uint8_t wire_splice(noah_profile_owner_t *owner, uint16_t transaction) {
    uint8_t fields[15] = {0};

    fields[1] = NOAH_PROFILE_DOMAIN_V1_SETTINGS;
    write_u32(&fields[7], 2u);
    write_u32(&fields[11], 2u);
    return wire_set(owner, NOAH_PROFILE_WIRE_V1_VALUE_EDIT_SPLICE, transaction, fields, sizeof(fields));
}

static uint8_t wire_data(noah_profile_owner_t *owner, uint16_t transaction) {
    uint8_t fields[7] = {0};

    fields[4] = 2u;
    return wire_set(owner, NOAH_PROFILE_WIRE_V1_VALUE_EDIT_DATA, transaction, fields, sizeof(fields));
}

static uint8_t wire_commit(noah_profile_owner_t *owner, uint16_t transaction) {
    uint8_t fields[4] = {0};

    return wire_set(owner, NOAH_PROFILE_WIRE_V1_VALUE_EDIT_COMMIT, transaction, fields, sizeof(fields));
}

static uint8_t wire_abort(noah_profile_owner_t *owner, uint16_t transaction) {
    return wire_set(owner, NOAH_PROFILE_WIRE_V1_VALUE_EDIT_ABORT, transaction, NULL, 0u);
}

static uint8_t wire_mode(noah_profile_owner_t *owner, uint16_t transaction, uint8_t mode) {
    return wire_set(owner, NOAH_PROFILE_WIRE_V1_VALUE_EDIT_MODE, transaction, &mode, 1u);
}

// GET `value`, page `page`, answered by `owner`: its 25-byte payload.
static void wire_get(noah_profile_owner_t *owner, uint8_t value, uint8_t page, uint8_t payload[NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE]) {
    memset(wire, 0, sizeof(wire));
    wire[0] = NOAH_PROFILE_WIRE_V1_COMMAND_GET;
    wire[1] = NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL;
    wire[2] = value;
    wire[3] = 0x5Au;
    wire[4] = page;
    wire_owner = owner;
    assert(noah_qmk_profile_edit_channel(wire, sizeof(wire)) && wire[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK);
    memcpy(payload, &wire[7], NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE);
}

// Edit status page 0's flags.
static uint16_t wire_flags(noah_profile_owner_t *owner) {
    uint8_t page[NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE];

    wire_get(owner, NOAH_PROFILE_WIRE_V1_VALUE_EDIT_STATUS, 0u, page);
    return (uint16_t)(page[2] | page[3] << 8u);
}

// A candidate frame on the candidate channel, as an Apply sends it: OK when
// it reached the candidate transaction, BUSY when admission held it.
static uint8_t wire_candidate(noah_profile_owner_t *owner, uint8_t frame[32]) {
    assert(noah_profile_owner_receive(owner, frame, 32u));
    if (frame[5] == NOAH_PROFILE_CANDIDATE_V1_ADMISSION_QUEUED) return NOAH_PROFILE_WORKING_OK;
    assert(frame[5] == NOAH_PROFILE_CANDIDATE_V1_ADMISSION_BUSY && frame[6] == NOAH_PROFILE_CANDIDATE_V1_ERROR_MAILBOX_BUSY);
    return NOAH_PROFILE_WORKING_BUSY;
}

// ── The rows ─────────────────────────────────────────────────────────────────

enum {
    ROW_QUIET,                // nothing unsaved, the halves agree
    ROW_UNSAVED,              // a client's edit published, unsaved
    ROW_ADJUSTMENT_QUEUED,    // a keyboard adjustment queued, no scan since
    ROW_ADJUSTMENT_IN_FLIGHT, // the keyboard's adjustment edit waits for its boundary
    ROW_RECEIVING,            // a client's edit being received
    ROW_CLIENT_SAVE,          // a client's Save waits for its boundary
    ROW_KEYBOARD_SAVE,        // the keyboard's own Save in Live waits for its boundary
    ROW_APPLY_STAGED,         // an Apply validated, not committed
    ROW_APPLY_AND_ADJUSTMENT, // the same, and a keyboard adjustment queued
    ROW_APART,                // the other half unheard for 3 s
    ROWS,
};

// Columns asked through the wire. An Apply frame's answer is OK when it
// reached the candidate transaction and BUSY when admission held it.
enum {
    ASK_EDIT,         // a new edit naming the current identity
    ASK_EDIT_STALE,   // a new edit naming another revision
    ASK_SAVE,
    ASK_SAVE_STALE,
    ASK_REVERT,
    ASK_MODE,         // a change to another mode
    ASK_MODE_INVALID, // mode 3, malformed: before admission
    ASK_APPLY_BEGIN,  // a new Apply's begin
    ASK_APPLY_COMMIT, // a commit of the candidate running, or of none
    ASKS,
};

enum {
    // The flags admission composes, pinned per row; the stopped flags are
    // never set by any of these rows.
    ADMISSION_FLAGS = NOAH_PROFILE_EDIT_WIRE_PEER_CONVERGED | NOAH_PROFILE_EDIT_WIRE_PEER_KNOWN | NOAH_PROFILE_EDIT_WIRE_EDITS_BLOCKED | NOAH_PROFILE_EDIT_WIRE_SAVING | NOAH_PROFILE_EDIT_WIRE_WAITING_SAFE | NOAH_PROFILE_EDIT_WIRE_ADJUSTMENT_PENDING | NOAH_PROFILE_EDIT_WIRE_STOPPED | NOAH_PROFILE_EDIT_WIRE_PEER_STOPPED,
    AGREE = NOAH_PROFILE_EDIT_WIRE_PEER_CONVERGED | NOAH_PROFILE_EDIT_WIRE_PEER_KNOWN,
    BLOCKED = NOAH_PROFILE_EDIT_WIRE_EDITS_BLOCKED,
};

enum {
    ANS_OK         = NOAH_PROFILE_WORKING_OK,
    ANS_QUEUED     = NOAH_PROFILE_WORKING_QUEUED,
    ANS_STALE      = NOAH_PROFILE_WORKING_STALE,
    ANS_BUSY       = NOAH_PROFILE_WORKING_BUSY,
    ANS_MALFORMED  = NOAH_PROFILE_WORKING_INVALID_ARGUMENT,
    ANS_NOT_CLEAN  = NOAH_PROFILE_WORKING_NOT_CLEAN,
};

// owner_admit's rows (profile_owner.c), as a client and the keyboard meet
// them in each state.
static const struct {
    const char *name;
    uint8_t     answer[ASKS];
    bool        keyboard_publishes; // an adjustment pressed now publishes
    uint16_t    flags;              // of ADMISSION_FLAGS
} rows[ROWS] = {
    // Answers in column order: edit, stale edit, Save, stale Save, Revert,
    // mode, malformed mode, Apply begin, Apply commit.
    [ROW_QUIET]                = {"quiet", {ANS_OK, ANS_STALE, ANS_OK, ANS_STALE, ANS_OK, ANS_OK, ANS_MALFORMED, ANS_OK, ANS_OK}, true, AGREE},
    [ROW_UNSAVED]              = {"unsaved", {ANS_OK, ANS_STALE, ANS_QUEUED, ANS_STALE, ANS_QUEUED, ANS_NOT_CLEAN, ANS_MALFORMED, ANS_OK, ANS_OK}, true, AGREE},
    [ROW_ADJUSTMENT_QUEUED]    = {"adjustment queued", {ANS_BUSY, ANS_STALE, ANS_BUSY, ANS_STALE, ANS_BUSY, ANS_NOT_CLEAN, ANS_MALFORMED, ANS_BUSY, ANS_OK}, true, AGREE | BLOCKED | NOAH_PROFILE_EDIT_WIRE_ADJUSTMENT_PENDING},
    [ROW_ADJUSTMENT_IN_FLIGHT] = {"adjustment in flight", {ANS_BUSY, ANS_STALE, ANS_BUSY, ANS_STALE, ANS_BUSY, ANS_NOT_CLEAN, ANS_MALFORMED, ANS_BUSY, ANS_BUSY}, false, AGREE | BLOCKED | NOAH_PROFILE_EDIT_WIRE_WAITING_SAFE},
    // Editing alone: no STALE, as nothing else holds the profile.
    [ROW_RECEIVING]            = {"receiving", {ANS_BUSY, ANS_BUSY, ANS_BUSY, ANS_STALE, ANS_OK, ANS_BUSY, ANS_MALFORMED, ANS_BUSY, ANS_BUSY}, false, AGREE | BLOCKED},
    // Saving comes first: even a stale Save is BUSY.
    [ROW_CLIENT_SAVE]          = {"client save", {ANS_BUSY, ANS_STALE, ANS_BUSY, ANS_BUSY, ANS_BUSY, ANS_NOT_CLEAN, ANS_MALFORMED, ANS_BUSY, ANS_BUSY}, false, NOAH_PROFILE_EDIT_WIRE_PEER_KNOWN | BLOCKED | NOAH_PROFILE_EDIT_WIRE_SAVING},
    [ROW_KEYBOARD_SAVE]        = {"keyboard save", {ANS_BUSY, ANS_STALE, ANS_BUSY, ANS_BUSY, ANS_BUSY, ANS_NOT_CLEAN, ANS_MALFORMED, ANS_BUSY, ANS_BUSY}, false, NOAH_PROFILE_EDIT_WIRE_PEER_KNOWN | BLOCKED | NOAH_PROFILE_EDIT_WIRE_SAVING},
    // The mode is stored outside the profile: a staged Apply does not hold it.
    [ROW_APPLY_STAGED]         = {"Apply staged", {ANS_BUSY, ANS_STALE, ANS_BUSY, ANS_STALE, ANS_BUSY, ANS_OK, ANS_MALFORMED, ANS_OK, ANS_OK}, false, AGREE | BLOCKED},
    [ROW_APPLY_AND_ADJUSTMENT] = {"Apply and adjustment", {ANS_BUSY, ANS_STALE, ANS_BUSY, ANS_STALE, ANS_BUSY, ANS_NOT_CLEAN, ANS_MALFORMED, ANS_BUSY, ANS_OK}, false, AGREE | BLOCKED | NOAH_PROFILE_EDIT_WIRE_ADJUSTMENT_PENDING},
    // Revert never waits for the other half; a mode change does.
    [ROW_APART]                = {"apart", {ANS_BUSY, ANS_STALE, ANS_BUSY, ANS_STALE, ANS_OK, ANS_BUSY, ANS_MALFORMED, ANS_OK, ANS_OK}, false, BLOCKED},
};

static struct {
    noah_profile_owner_t left, right;
    memory_t             lm, rm;
    split_link_t         ll, rl;
    split_fault_link_t   lw, rw;
    uint32_t             now, session;
} rig;

enum { RIG_CLIENT_EDIT = 50u, RIG_CLIENT_SAVE = 60u, RIG_APPLY = 80u };

// Boots the pair on whatever its memories hold, linked, and lets it agree.
static noah_profile_owner_t *boot_rig(void) {
    rig.ll = split_link_to(&rig.right);
    rig.rl = split_link_to(&rig.left);
    rig.lw = working_link_to(&rig.right);
    rig.rw = working_link_to(&rig.left);
    rig.now = 1000u;
    activation_block_reason = 0u;
    auto_quiet = true;
    boot_live_pair(&rig.left, &rig.lm, &rig.right, &rig.rm, &rig.ll, &rig.rl, &rig.lw, &rig.rw, ++rig.session, &rig.now);
    scan_until_converged(&rig.left, &rig.right, &rig.now);
    auto_configure(&rig.left);
    return &rig.left;
}

static void rig_scans(uint32_t duration) {
    auto_scans(&rig.left, &rig.right, &rig.now, duration);
}

static void rig_edit(uint16_t transaction) {
    uint8_t settings[NOAH_SETTINGS_MIN_SIZE];
    noah_profile_working_splice_t create = {.domain_id = NOAH_PROFILE_DOMAIN_V1_SETTINGS, .insert = sizeof(settings), .flags = NOAH_PROFILE_WORKING_SPLICE_CREATE};

    auto_settings(settings);
    assert(live_edit(&rig.left, &rig.right, transaction, &create, settings, sizeof(settings), &rig.now) == NOAH_PROFILE_WORKING_OK);
    scan_until_converged(&rig.left, &rig.right, &rig.now);
    assert(rig.left.working.active);
}

// A fresh pair in `row`, held there: a save or an adjustment's edit waits at
// the safe boundary (a key held), and the other half's links are cut.
static noah_profile_owner_t *reach(uint8_t row) {
    noah_profile_owner_t *left;

    erased_memory(&rig.lm);
    erased_memory(&rig.rm);
    left = boot_rig();
    switch (row) {
        case ROW_UNSAVED:
            rig_edit(1u);
            break;
        case ROW_ADJUSTMENT_QUEUED:
            assert(noah_profile_owner_adjust(left, NOAH_SETTING_DEFAULT_DPI, 800u));
            break;
        case ROW_ADJUSTMENT_IN_FLIGHT:
            activation_block_reason = NOAH_PROFILE_ACTIVATION_REASON_PERSISTENT_INTENT;
            assert(noah_profile_owner_adjust(left, NOAH_SETTING_DEFAULT_DPI, 800u));
            for (uint32_t guard = 0u; guard < 400u && !(left->auto_save.edit_transaction && left->working.phase == NOAH_PROFILE_WORKING_PHASE_WAITING_SAFE); guard++) scan_pair(left, &rig.right, &rig.now);
            // It has tried the boundary at least once.
            rig_scans(200u);
            assert(left->auto_save.edit_transaction && !left->auto_save.pending && left->working.phase == NOAH_PROFILE_WORKING_PHASE_WAITING_SAFE);
            break;
        case ROW_RECEIVING:
            assert(wire_begin(left, RIG_CLIENT_EDIT, left->working.session, left->working.revision) == NOAH_PROFILE_WORKING_OK);
            assert(left->working.phase == NOAH_PROFILE_WORKING_PHASE_RECEIVING);
            break;
        case ROW_CLIENT_SAVE:
            rig_edit(1u);
            activation_block_reason = NOAH_PROFILE_ACTIVATION_REASON_PERSISTENT_INTENT;
            assert(wire_named(left, NOAH_PROFILE_WIRE_V1_VALUE_SAVE, RIG_CLIENT_SAVE, left->working.session, left->working.revision) == NOAH_PROFILE_WORKING_QUEUED);
            for (uint32_t guard = 0u; guard < 4096u && !left->host_barrier.waiting_boundary; guard++) scan_pair(left, &rig.right, &rig.now);
            assert(left->save.phase != NOAH_PROFILE_OWNER_SAVE_IDLE && !left->save.automatic && left->host_barrier.waiting_boundary);
            break;
        case ROW_KEYBOARD_SAVE:
            assert(wire_mode(left, 2u, NOAH_PROFILE_EDIT_MODE_LIVE) == NOAH_PROFILE_WORKING_OK);
            rig_edit(1u);
            assert(rig.right.working_sync.mode.mode == NOAH_PROFILE_EDIT_MODE_LIVE);
            activation_block_reason = NOAH_PROFILE_ACTIVATION_REASON_PERSISTENT_INTENT;
            for (uint32_t guard = 0u; guard < 4096u && !left->host_barrier.waiting_boundary; guard++) scan_pair(left, &rig.right, &rig.now);
            assert(left->save.phase != NOAH_PROFILE_OWNER_SAVE_IDLE && left->save.automatic && left->host_barrier.waiting_boundary);
            break;
        case ROW_APPLY_STAGED:
        case ROW_APPLY_AND_ADJUSTMENT:
            stage_valid_candidate_pair(left, &rig.right, RIG_APPLY, &rig.now);
            if (row == ROW_APPLY_AND_ADJUSTMENT) {
                assert(noah_profile_owner_adjust(left, NOAH_SETTING_DEFAULT_DPI, 800u));
            }
            break;
        case ROW_APART:
            rig.ll.link.disconnected = rig.rl.link.disconnected = true;
            rig.lw.disconnected = rig.rw.disconnected = true;
            rig_scans(NOAH_PROFILE_WORKING_SYNC_PEER_TIMEOUT_MS + 100u);
            break;
        default:
            break;
    }
    return left;
}

static uint8_t ask(noah_profile_owner_t *owner, uint8_t column) {
    uint32_t session  = owner->working.session;
    uint32_t revision = owner->working.revision;
    uint16_t running  = owner->host_transaction.status.transaction_id;
    uint8_t  frame[32];

    switch (column) {
        case ASK_EDIT:
            return wire_begin(owner, 901u, session, revision);
        case ASK_EDIT_STALE:
            return wire_begin(owner, 902u, session, revision + 1u);
        case ASK_SAVE:
            return wire_named(owner, NOAH_PROFILE_WIRE_V1_VALUE_SAVE, 903u, session, revision);
        case ASK_SAVE_STALE:
            return wire_named(owner, NOAH_PROFILE_WIRE_V1_VALUE_SAVE, 904u, session, revision + 1u);
        case ASK_REVERT:
            return wire_named(owner, NOAH_PROFILE_WIRE_V1_VALUE_REVERT, 905u, session, revision);
        case ASK_MODE:
            return wire_mode(owner, 906u, owner->working_sync.mode.mode == NOAH_PROFILE_EDIT_MODE_APPLY ? NOAH_PROFILE_EDIT_MODE_LIVE : NOAH_PROFILE_EDIT_MODE_APPLY);
        case ASK_MODE_INVALID:
            return wire_mode(owner, 907u, NOAH_PROFILE_EDIT_MODE_COUNT);
        case ASK_APPLY_BEGIN:
            begin_frame(frame, 908u);
            return wire_candidate(owner, frame);
        default:
            simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, running ? running : 909u);
            return wire_candidate(owner, frame);
    }
}

// Edit status pages 0 and 1: everything a refusal must leave as it was.
typedef struct {
    uint8_t pages[2][NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE];
    uint8_t candidate_state;
    bool    has_candidate, mailbox_pending;
} wire_view_t;

static wire_view_t wire_view(noah_profile_owner_t *owner) {
    wire_view_t view;

    wire_get(owner, NOAH_PROFILE_WIRE_V1_VALUE_EDIT_STATUS, 0u, view.pages[0]);
    wire_get(owner, NOAH_PROFILE_WIRE_V1_VALUE_EDIT_STATUS, 1u, view.pages[1]);
    view.candidate_state = (uint8_t)owner->host_transaction.status.state;
    view.has_candidate   = owner->host_transaction.has_candidate;
    view.mailbox_pending = owner->host_transaction.mailbox.pending;
    return view;
}

static void assert_unchanged(noah_profile_owner_t *owner, const wire_view_t *before) {
    wire_view_t after = wire_view(owner);

    assert(memcmp(after.pages, before->pages, sizeof(after.pages)) == 0);
    assert(after.candidate_state == before->candidate_state && after.has_candidate == before->has_candidate && after.mailbox_pending == before->mailbox_pending);
}

// Names the row and column of a wrong answer before the assertion stops.
static void assert_answer(uint8_t row, unsigned column, uint8_t answer, uint8_t expected) {
    if (answer != expected) printf("admission row \"%s\", column %u: answered %u, not %u\n", rows[row].name, column, answer, expected);
    assert(answer == expected);
}

// A refusal, or a repeat, answered on the row's shared state: it says
// `expected`, and changes nothing.
static void assert_refused(noah_profile_owner_t *owner, uint8_t row, unsigned column, uint8_t answer, uint8_t expected, const wire_view_t *before) {
    assert_answer(row, column, answer, expected);
    assert_unchanged(owner, before);
}

static void test_admission_matrix(void) {
    unsigned boots = 0u;

    for (uint8_t row = 0u; row < ROWS; row++) {
        noah_profile_owner_t *left = reach(row);
        uint16_t              flags;
        wire_view_t           before;

        boots++;
        // The flags, read and in every reply; flag 4 is the edit row's answer.
        flags = wire_flags(left);
        if ((flags & ADMISSION_FLAGS) != rows[row].flags) printf("admission row \"%s\": flags %04x, not %04x\n", rows[row].name, (unsigned)(flags & ADMISSION_FLAGS), (unsigned)rows[row].flags);
        assert((flags & ADMISSION_FLAGS) == rows[row].flags);
        assert(((flags & NOAH_PROFILE_EDIT_WIRE_EDITS_BLOCKED) != 0u) == (rows[row].answer[ASK_EDIT] != NOAH_PROFILE_WORKING_OK));
        before = wire_view(left);
        for (uint8_t column = 0u; column < ASKS; column++) {
            uint8_t expected = rows[row].answer[column];
            uint8_t answer;

            if (expected == NOAH_PROFILE_WORKING_OK || expected == NOAH_PROFILE_WORKING_QUEUED) continue;
            answer = ask(left, column);
            // An edit report's reply carries the flags; a candidate frame's does not.
            assert(column >= ASK_APPLY_BEGIN || wire_reply_flags() == flags);
            assert_refused(left, row, column, answer, expected, &before);
        }
        // Repeats change nothing: the client's begin of the edit it is
        // sending, and its Save of the save running; the keyboard's own save
        // is no client's to repeat.
        if (row == ROW_RECEIVING) {
            assert_refused(left, row, ASKS, wire_begin(left, RIG_CLIENT_EDIT, left->working.session, left->working.revision), NOAH_PROFILE_WORKING_OK, &before);
        }
        if (row == ROW_CLIENT_SAVE || row == ROW_KEYBOARD_SAVE) {
            assert_refused(left, row, ASKS, wire_named(left, NOAH_PROFILE_WIRE_V1_VALUE_SAVE, left->save.transaction_id, left->working.session, left->working.revision), row == ROW_CLIENT_SAVE ? NOAH_PROFILE_WORKING_QUEUED : NOAH_PROFILE_WORKING_BUSY, &before);
        }
        // The keyboard's step, last: an adjustment pressed now.
        assert(noah_profile_owner_adjust(left, NOAH_SETTING_SNIPING_DPI, 300u));
        rig_scans(2000u);
        assert((left->auto_save.pending == 0u) == rows[row].keyboard_publishes);
        if (rows[row].keyboard_publishes) {
            assert(active_scalar(left, NOAH_SETTING_SNIPING_DPI) == 300u);
        }

        // What is admitted changes something: each on a row of its own.
        for (uint8_t column = 0u; column < ASKS; column++) {
            uint8_t expected = rows[row].answer[column];

            if (expected != NOAH_PROFILE_WORKING_OK && expected != NOAH_PROFILE_WORKING_QUEUED) continue;
            left = reach(row);
            boots++;
            assert_answer(row, column, ask(left, column), expected);
        }
        // An Apply already admitted carries on behind a queued adjustment.
        if (row == ROW_APPLY_AND_ADJUSTMENT) {
            uint8_t frame[32];

            left = reach(row);
            boots++;
            begin_frame(frame, RIG_APPLY);
            assert(wire_candidate(left, frame) == NOAH_PROFILE_WORKING_OK);
        }
    }

    // A half with nowhere to store the edit mode says so.
    erased_memory(&rig.lm);
    erased_memory(&rig.rm);
    rig.lm.no_mode_storage = true;
    assert(wire_mode(boot_rig(), 1u, NOAH_PROFILE_EDIT_MODE_APPLY) == NOAH_PROFILE_WORKING_READ_ERROR);
    printf("admission: %u states, %u pairs booted, every answer and flag through the wire\n", (unsigned)ROWS, boots + 1u);
}

// A change of edit mode takes a counter above both halves'. A half that has
// not heard the other cannot know the other's, so it waits until the halves
// agree; admitted apart, the left half's change below would take counter 2
// and lose to the right half's 5 when they reconnect.
static void test_a_mode_change_waits_until_the_halves_agree(void) {
    noah_profile_owner_t *left = &rig.left, *right = &rig.right;
    uint8_t               page[NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE];

    erased_memory(&rig.lm);
    erased_memory(&rig.rm);
    rig.lm.mode = (noah_profile_edit_mode_record_t){.counter = 1u, .mode = NOAH_PROFILE_EDIT_MODE_LIVE_REVIEW};
    // The right half was the USB half alone and changed the mode since.
    rig.rm.mode = (noah_profile_edit_mode_record_t){.counter = 5u, .mode = NOAH_PROFILE_EDIT_MODE_APPLY};
    rig.ll = split_link_to(right);
    rig.rl = split_link_to(left);
    rig.lw = working_link_to(right);
    rig.rw = working_link_to(left);
    rig.ll.link.disconnected = rig.rl.link.disconnected = true;
    rig.lw.disconnected = rig.rw.disconnected = true;
    rig.now = 1000u;
    activation_block_reason = 0u;
    boot_live_pair(left, &rig.lm, right, &rig.rm, &rig.ll, &rig.rl, &rig.lw, &rig.rw, ++rig.session, &rig.now);
    // Each half alone, as the USB half, for longer than the other is remembered.
    for (uint32_t elapsed = 0u; elapsed <= NOAH_PROFILE_WORKING_SYNC_PEER_TIMEOUT_MS + 100u; elapsed += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        (void)noah_profile_owner_scan(left, true, rig.now);
        (void)noah_profile_owner_scan(right, true, rig.now);
        rig.now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
    }
    assert(wire_mode(left, 1u, NOAH_PROFILE_EDIT_MODE_LIVE) == NOAH_PROFILE_WORKING_BUSY && !(wire_reply_flags() & NOAH_PROFILE_EDIT_WIRE_PEER_CONVERGED));
    assert(wire_mode(right, 2u, NOAH_PROFILE_EDIT_MODE_LIVE) == NOAH_PROFILE_WORKING_BUSY);
    assert(left->working_sync.mode.counter == 1u && right->working_sync.mode.counter == 5u && rig.lm.mode_writes == 0u && rig.rm.mode_writes == 0u);

    // Linked again: the higher counter wins, and the left half stores it.
    rig.ll.link.disconnected = rig.rl.link.disconnected = false;
    rig.lw.disconnected = rig.rw.disconnected = false;
    scan_until_converged(left, right, &rig.now);
    rig_scans(1000u);
    wire_get(left, NOAH_PROFILE_WIRE_V1_VALUE_EDIT_MODE, 0u, page);
    assert(page[0] == NOAH_PROFILE_EDIT_MODE_APPLY && page[1] == 5u && page[10] == (1u | 4u));
    assert(rig.lm.mode.counter == 5u && rig.lm.mode.mode == NOAH_PROFILE_EDIT_MODE_APPLY);

    // Agreed, a change is taken at once, above both counters. Each half writes
    // it once its keyboard is quiet.
    rig.lm.mode_waits = rig.rm.mode_waits = true;
    assert(wire_mode(left, 3u, NOAH_PROFILE_EDIT_MODE_LIVE) == NOAH_PROFILE_WORKING_OK);
    rig_scans(1000u);
    assert(right->working_sync.mode.counter == 6u && right->working_sync.mode.mode == NOAH_PROFILE_EDIT_MODE_LIVE);
    assert(rig.lm.mode.counter == 5u && rig.rm.mode.counter == 5u);
    rig.lm.mode_waits = rig.rm.mode_waits = false;
    rig_scans(1000u);
    assert(rig.lm.mode.counter == 6u && rig.lm.mode.mode == NOAH_PROFILE_EDIT_MODE_LIVE);
    assert(rig.rm.mode.counter == 6u && rig.rm.mode.mode == NOAH_PROFILE_EDIT_MODE_LIVE);
    wire_get(left, NOAH_PROFILE_WIRE_V1_VALUE_EDIT_MODE, 0u, page);
    assert(page[0] == NOAH_PROFILE_EDIT_MODE_LIVE && page[1] == 6u && page[5] == NOAH_PROFILE_EDIT_MODE_LIVE && page[6] == 6u && page[10] == (1u | 4u));
    puts("a mode change waits until the halves agree, then takes a counter above both and is stored on both");
}

// The halves also disagree while an Apply copies its profile to the other
// half, with nothing unsaved anywhere: the mode waits for that too, and is
// taken once the Apply lands.
static void test_a_mode_change_waits_for_an_apply_copying_to_the_other_half(void) {
    noah_profile_owner_t *left  = reach(ROW_QUIET);
    noah_profile_owner_t *right = &rig.right;
    uint8_t               frame[32];
    bool                  asked = false;

    stage_valid_candidate_pair(left, right, RIG_APPLY, &rig.now);
    simple_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, RIG_APPLY);
    send_and_scan_pair(left, right, frame, &rig.now);
    for (uint32_t guard = 0u; guard < 4096u && left->host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE; guard++) {
        if (!asked && !(wire_flags(left) & NOAH_PROFILE_EDIT_WIRE_PEER_CONVERGED)) {
            assert(!left->working.active && !left->auto_save.pending && left->save.phase == NOAH_PROFILE_OWNER_SAVE_IDLE);
            assert(noah_profile_working_sync_converged(&left->working_sync, left->now_ms));
            assert(wire_mode(left, 1u, NOAH_PROFILE_EDIT_MODE_APPLY) == NOAH_PROFILE_WORKING_BUSY);
            asked = true;
        }
        scan_pair(left, right, &rig.now);
    }
    assert(asked);
    scan_until_converged(left, right, &rig.now);
    assert(committed_generation(&left->store) == 1u && committed_generation(&right->store) == 1u);
    assert(wire_mode(left, 2u, NOAH_PROFILE_EDIT_MODE_APPLY) == NOAH_PROFILE_WORKING_OK);
    rig_scans(1000u);
    assert(right->working_sync.mode.mode == NOAH_PROFILE_EDIT_MODE_APPLY && right->working_sync.mode.counter == left->working_sync.mode.counter);
    puts("a mode change waits while an Apply copies to the other half, and is taken on both once it lands");
}

// The edit status flags the matrix does not pin, composed from real owner
// state; flags 14 and 15 are the stopped tests'.
static void test_edit_status_flags_come_from_the_owner(void) {
    noah_profile_owner_t *left = reach(ROW_QUIET);
    uint8_t               page[NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE];
    uint16_t              flags;

    // Nothing saved yet: the compiled defaults, and the other half holds them.
    flags = wire_flags(left);
    assert(flags == (NOAH_PROFILE_EDIT_WIRE_PEER_REQUIRED | AGREE | NOAH_PROFILE_EDIT_WIRE_SAVED_COMPILED));
    rig_edit(1u);
    assert(wire_flags(left) == (flags | NOAH_PROFILE_EDIT_WIRE_UNSAVED));
    assert(wire_named(left, NOAH_PROFILE_WIRE_V1_VALUE_SAVE, 2u, left->working.session, left->working.revision) == NOAH_PROFILE_WORKING_QUEUED);
    for (uint32_t guard = 0u; guard < 20000u && left->save.phase != NOAH_PROFILE_OWNER_SAVE_IDLE; guard++) scan_pair(left, &rig.right, &rig.now);
    scan_until_converged(left, &rig.right, &rig.now);
    assert(wire_flags(left) == (NOAH_PROFILE_EDIT_WIRE_PEER_REQUIRED | AGREE));

    // A save here waiting for a quiet keyboard (the storage step's own hook).
    rig.lm.storage_paused = true;
    assert(wire_flags(left) & NOAH_PROFILE_EDIT_WIRE_STORAGE_PAUSED);
    rig.lm.storage_paused = false;
    assert(!(wire_flags(left) & NOAH_PROFILE_EDIT_WIRE_STORAGE_PAUSED));

    // The edit mode's record cannot be written: reported until a retry stores it.
    rig.lm.fail_mode_writes = true;
    assert(wire_mode(left, 3u, NOAH_PROFILE_EDIT_MODE_APPLY) == NOAH_PROFILE_WORKING_OK);
    rig_scans(200u);
    assert((wire_flags(left) & NOAH_PROFILE_EDIT_WIRE_MODE_NOT_STORED) && rig.lm.mode_writes != 0u);
    rig.lm.fail_mode_writes = false;
    rig_scans(NOAH_PROFILE_WORKING_SYNC_MODE_RETRY_MS + 200u);
    assert(!(wire_flags(left) & NOAH_PROFILE_EDIT_WIRE_MODE_NOT_STORED) && rig.lm.mode.mode == NOAH_PROFILE_EDIT_MODE_APPLY);

    // Equal counters that disagree, as two halves each changed alone could be:
    // a conflict, until the next change.
    erased_memory(&rig.lm);
    erased_memory(&rig.rm);
    rig.lm.mode = (noah_profile_edit_mode_record_t){.counter = 3u, .mode = NOAH_PROFILE_EDIT_MODE_LIVE};
    rig.rm.mode = (noah_profile_edit_mode_record_t){.counter = 3u, .mode = NOAH_PROFILE_EDIT_MODE_APPLY};
    left = boot_rig();
    rig_scans(1000u);
    assert(wire_flags(left) & NOAH_PROFILE_EDIT_WIRE_MODE_CONFLICT);
    wire_get(left, NOAH_PROFILE_WIRE_V1_VALUE_EDIT_MODE, 0u, page);
    assert(page[10] == (1u | 2u));
    assert(wire_mode(left, 4u, NOAH_PROFILE_EDIT_MODE_LIVE) == NOAH_PROFILE_WORKING_OK);
    rig_scans(1000u);
    assert(!(wire_flags(left) & NOAH_PROFILE_EDIT_WIRE_MODE_CONFLICT) && rig.rm.mode.counter == 4u && rig.rm.mode.mode == NOAH_PROFILE_EDIT_MODE_LIVE);

    // A keyboard without another half does not need one.
    {
        uint32_t now = 0u;

        memset(&auto_chip, 0, sizeof(auto_chip));
        memset(auto_chip.bytes, 255, sizeof(auto_chip.bytes));
        auto_flash_boot(&now);
        assert(!(wire_flags(&auto_owner) & NOAH_PROFILE_EDIT_WIRE_PEER_REQUIRED));
    }
    puts("edit status flags 0-3, 5, 7, 9 and 12 come from the owner's own state");
}
