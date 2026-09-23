"use strict";

// The steps of one Apply, as the person watching it sees them.
//
// restoreProfile() reports into a tracker at each real boundary of the logical
// transaction; the tracker only moves forward, because the coordinators poll
// and may report an earlier phase again after a later one began (commit status
// keeps arriving while the connected half writes its keys). A failure is
// pinned to the step that was running, with the reason the keyboard gave.

const APPLY_STEPS = Object.freeze([
    {id: "check", label: "Check the keyboard"},
    {id: "backup", label: "Save a recovery copy"},
    {id: "upload", label: "Send the profile"},
    {id: "validate", label: "Keyboard checks the profile"},
    {id: "stage", label: "Stage keys and macros on the other half"},
    {id: "peer", label: "Copy the profile to the other half"},
    {id: "commit", label: "Save it on this half"},
    {id: "converge", label: "Finish the other half"},
    {id: "local", label: "Write keys and macros on this half"},
    {id: "verify", label: "Check both halves"},
]);
const STEP_INDEX = Object.freeze(Object.fromEntries(APPLY_STEPS.map((step, index) => [step.id, index])));

// Up to this step nothing reaches durable storage; the commit decision is made
// inside "commit". Where a failure lands decides what can honestly be said
// about what was saved, together with whether the keyboard confirmed a cancel.
const LAST_UNSAVED_STEP = STEP_INDEX.peer;

function bytes(completed, total) {
    return Number.isInteger(completed) && Number.isInteger(total) && total > 0 ? {completed: Math.min(completed, total), total} : null;
}

let applySequence = 0;

class ApplyProgress {
    constructor(onChange = () => {}) {
        this.onChange = onChange;
        // Tells one Apply's view from the next, so a dismissed failure stays
        // dismissed without hiding a later identical one.
        this.id = ++applySequence;
        this.index = -1;
        this.detail = "";
        this.bytes = null;
        this.failure = null;
        this.finished = false;
    }

    // Moves to `step` (never backwards) and records what it is doing now.
    report(step, {detail = "", completed, total} = {}) {
        const index = STEP_INDEX[step];
        if (index === undefined) throw new RangeError(`Unknown Apply step ${step}.`);
        if (this.finished || this.failure || index < this.index) return;
        if (index > this.index) {
            this.index = index;
            this.bytes = null;
        }
        this.detail = detail;
        const counted = bytes(completed, total);
        if (counted) this.bytes = counted;
        this.onChange(this.view());
    }

    get current() {
        return this.index >= 0 ? APPLY_STEPS[this.index].id : "check";
    }

    // Nothing was saved when the failure came before the commit decision.
    get beforeDecision() {
        return this.index <= LAST_UNSAVED_STEP;
    }

    fail({reason = "", saved = "unknown"} = {}) {
        if (this.failure) return this.failure;
        const step = APPLY_STEPS[Math.max(this.index, 0)];
        this.failure = {step: step.id, label: step.label, reason, saved};
        this.onChange(this.view());
        return this.failure;
    }

    finish() {
        this.finished = true;
        this.index = APPLY_STEPS.length - 1;
        this.detail = "";
        this.bytes = null;
        this.onChange(this.view());
    }

    view() {
        const failedAt = this.failure ? STEP_INDEX[this.failure.step] : -1;
        return {
            id: this.id,
            state: this.failure ? "failed" : this.finished ? "done" : "applying",
            steps: APPLY_STEPS.map((step, index) => ({
                id: step.id,
                label: step.label,
                state: index === failedAt ? "failed"
                    : this.finished || index < this.index ? "done"
                        : index === this.index && !this.failure ? "active" : "pending",
            })),
            current: this.current,
            detail: this.detail,
            bytes: this.bytes ? {...this.bytes} : null,
            failure: this.failure ? {...this.failure} : null,
        };
    }
}

const DOMAIN_NAMES = Object.freeze({0x10: "lighting", 0x20: "key behaviours", 0x30: "combos", 0x40: "settings", 0x50: "pointing modes"});

// What the keyboard's candidate error means, in the words of the person who
// pressed Apply. Unlisted errors fall back to their wire name.
const DEVICE_REASONS = Object.freeze({
    CAPACITY_EXCEEDED: "The profile is larger than the keyboard can store.",
    INCOMPATIBLE_SCHEMA: "The keyboard's firmware does not accept this profile format.",
    UNSUPPORTED_DOMAIN: "The keyboard's firmware does not support part of this profile.",
    INCOMPATIBLE_ACTION_ABI: "The profile was made for different firmware.",
    CHECKSUM_MISMATCH: "The profile arrived damaged.",
    STORAGE_FAILURE: "The keyboard could not write its storage.",
    VALIDATION_REJECTED: "The keyboard rejected the profile",
    ACTIVATION_FAILED: "The keyboard saved the profile but could not start using it.",
    DURABILITY_UNKNOWN: "The keyboard could not confirm whether the save was written.",
    TIMEOUT: "The keyboard gave up waiting for this save.",
    PEER_SUPERSEDED: "The other half already had a newer profile.",
    PEER_PREPARE_YIELDED: "The other half was saving a profile of its own at the same time.",
    POSTCOMMIT_AUTHORITY_LOST: "The other half reported a newer profile after this half saved.",
    PEER_COMMIT_CONFLICT: "The two halves disagree about which profile is current.",
});

// The other half's last answer to the split transfer, when the keyboard
// reports it (candidate status page 1).
const PEER_REASONS = Object.freeze({
    BUSY: "The other half kept answering that it was busy.",
    STALE: "The other half already had a newer profile.",
    CONFLICT: "The other half holds a conflicting profile.",
    CORRUPT: "The other half's copy was damaged.",
    INCOMPATIBLE: "The other half runs incompatible firmware.",
    STORAGE_ERROR: "The other half could not write its storage.",
    VALIDATION_ERROR: "The other half rejected the profile.",
    DIGEST_MISMATCH: "The copy on the other half did not match what was sent.",
    RANGE_ERROR: "The copy to the other half went out of order.",
    INVALID_FRAME: "The link between the halves garbled a message.",
});

function locate(error) {
    if (!error || error.domainId === 0xff || error.domainId === undefined) return "";
    const domain = DOMAIN_NAMES[error.domainId] || `domain 0x${error.domainId.toString(16)}`;
    return error.rowIndex !== 0xffff && error.rowIndex !== undefined ? ` in ${domain}, row ${error.rowIndex + 1}` : ` in ${domain}`;
}

// Why the other half kept answering BUSY, when it said.
const PEER_BUSY_REASONS = Object.freeze({
    MAILBOX_FULL: "The other half stopped handling requests from this half.",
    OTHER_COPY: "The other half was still holding an earlier, unfinished copy.",
    NO_LEASE: "The other half dropped the copy partway and did not take it up again.",
    STORE_WORKING: "The other half did not finish storing its copy.",
    PULLING: "The other half was fetching a profile of its own.",
    CONVERGENCE_ONLY: "The other half was finishing another save.",
});

// The keyboard's own reason when it gave one, else the other half's last
// answer, else the host's message.
function failureReason(error, peer) {
    const device = error?.deviceError || error?.status?.error;
    if (device?.name && device.name !== "NONE") {
        const known = DEVICE_REASONS[device.name];
        if (known) return device.name === "VALIDATION_REJECTED" ? `${known}${locate(device)}.` : known;
        return `The keyboard reported ${device.name}.`;
    }
    if (peer && peer.busyStreak >= 3 && PEER_BUSY_REASONS[peer.busyReason]) return PEER_BUSY_REASONS[peer.busyReason];
    if (peer && peer.lastStatusName && peer.lastStatusName !== "OK" && PEER_REASONS[peer.lastStatusName]) return PEER_REASONS[peer.lastStatusName];
    if (peer && peer.transportFailureCount > 0 && /progress|timed out|timeout/i.test(error?.message || "")) return "The link between the halves stopped answering.";
    return error?.message || "The keyboard stopped answering.";
}

module.exports = {APPLY_STEPS, ApplyProgress, failureReason};
