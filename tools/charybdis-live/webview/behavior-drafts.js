"use strict";

// Ephemeral form drafts. A device + row key owns each draft, and the first
// edit pins its profile identity until save or explicit discard.
function createBehaviorDraftStore() {
    const rows = new Map();
    const copy = value => value === undefined ? undefined : JSON.parse(JSON.stringify(value));
    const same = (a, b) => Boolean(a && b) && ["source", "generation", "digest", "originHalf"].every(key => a[key] === b[key]);
    return {
        capture(key, controls, base) {
            if (!key) return;
            rows.set(key, {controls: copy(controls), base: copy(rows.get(key)?.base ?? base)});
        },
        get: key => copy(rows.get(key)),
        keys: () => Array.from(rows.keys()),
        stale: (key, base) => rows.has(key) && !same(rows.get(key).base, base),
        advance: (previous, next) => {for (const row of rows.values()) if (same(row.base, previous)) row.base = copy(next);},
        remove: key => rows.delete(key),
        clear: () => rows.clear(),
    };
}

module.exports = {createBehaviorDraftStore};
