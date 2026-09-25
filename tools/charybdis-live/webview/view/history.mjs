// The draft history: every step the draft took, newest first, each with when
// it was made and what it changed from the step before it.
//
// The host sends the steps oldest first while the history sheet is open.
// These arrange them for reading: newest on top, a step's items in rail order
// with the area each lives in, and a time a person can place.

import {AREAS} from "./review.mjs";

const rank = (area) => (AREAS.indexOf(area) + 1 || AREAS.length + 1);

// Steps newest first; within one, its items in rail order, each noting its
// area, since a step's items are not sectioned by area as the review's are.
export function historyEntries(steps = []) {
    return steps.slice().reverse().map((step) => ({
        ...step,
        changes: step.changes === null ? null : step.changes
            .map((change, index) => ({change, index}))
            .sort((left, right) => rank(left.change.area) - rank(right.change.area) || left.index - right.index)
            .map(({change}) => ({...change, note: change.note ? `${change.area} · ${change.note}` : change.area})),
    }));
}

// When a step was made: close by, how long ago; otherwise the clock time,
// and the day too once it is not today. `clock` is the exact time, for a tip.
export function stepTime(at, now = Date.now()) {
    const when = new Date(at), seconds = Math.max(0, Math.round((now - at) / 1000));
    const pad = (n) => String(n).padStart(2, "0");
    const clock = `${pad(when.getHours())}:${pad(when.getMinutes())}:${pad(when.getSeconds())}`;
    const sameDay = new Date(now).toDateString() === when.toDateString();
    const label = seconds < 45 ? "just now"
        : seconds < 60 * 60 ? `${Math.max(1, Math.round(seconds / 60))} min ago`
            : sameDay ? clock.slice(0, 5) : `${when.toLocaleDateString(undefined, {day: "numeric", month: "short"})} ${clock.slice(0, 5)}`;
    return {label, clock};
}
