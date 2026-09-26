// A duration edited as a share of another one (the auto-mouse fade's hold, as
// a share of the timeout): what a share would store, in milliseconds.
//
// The core decides what is stored; this previews it while a slider moves, so
// it follows the same rule, and tests/edits.test.mjs holds the two together.
// The share that was read is shown exactly as stored, not as it rounds.
export function shareHold(field, percent) {
    const whole = Number(field.whole);
    if (percent === Number(field.value)) return Number(field.ms);
    return Math.min(whole - 1, Math.round(percent * whole / 100));
}
