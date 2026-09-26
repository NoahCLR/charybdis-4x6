// The review's checks: what the host's layer walk (core/model/layer-reach.js)
// found in the draft, arranged for reading, and whether Apply has to be
// confirmed.
//
// Each check arrives with a level (a trap, a warning or a notice), a status
// (new in the draft, already on the keyboard, or fixed by the draft), what it
// is about in words, the steps into it and where it is edited. Only a trap asks
// for a confirmation, and it asks whether the draft made it or the keyboard
// already has it: applying either leaves a layer that can lock with no way back.

const LEVEL_RANK = {trap: 0, warning: 1, notice: 2};
export const LEVEL_WORD = {trap: "Trap", warning: "Warning", notice: "Notice"};
export const STATUS_WORD = {new: "new", existing: "on the keyboard", fixed: "fixed"};

const byLevel = (a, b) => (LEVEL_RANK[a.level] ?? 3) - (LEVEL_RANK[b.level] ?? 3);

// The groups the review lists, in reading order, leaving out empty ones. A
// group opens by default when it has something to act on: what the draft adds,
// and anything on the keyboard that is a trap.
export function checkGroups(checks = []) {
    const of = (status) => checks.filter((check) => check.status === status).sort(byLevel);
    return [
        {id: "new", title: "New in this draft", items: of("new"), open: true},
        {id: "existing", title: "Already on the keyboard", items: of("existing"), open: of("existing").some((check) => check.level === "trap")},
        {id: "fixed", title: "Fixed by this draft", items: of("fixed"), open: false},
    ].filter((group) => group.items.length);
}

// The traps Apply asks about.
export const trapsToConfirm = (checks = []) => checks.filter((check) => check.level === "trap" && check.status !== "fixed");

// What the confirmation says before Apply goes ahead.
export function confirmText(traps) {
    if (!traps.length) return "";
    const lead = traps.length === 1 ? traps[0].title : `${traps.length} sets of layers can lock with no way back`;
    return `${lead}. Once locked, only unplugging the keyboard clears it.`;
}

// The line a check's header shows beside its title: its level, and where it
// comes from unless it is new.
export function checkTags(check) {
    return [LEVEL_WORD[check.level] || check.level, ...(check.status === "new" ? [] : [STATUS_WORD[check.status] || check.status])];
}
