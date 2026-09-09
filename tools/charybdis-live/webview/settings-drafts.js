"use strict";

// A settings form owns its original readback identity until verified save or
// discard. Compare semantic fields instead of DOM positions so forms can move.
function createSettingsDraftStore() {
    let drafts = {};
    const copy = value => JSON.parse(JSON.stringify(value));
    const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
    return {
        capture(key, fields, original, base) {
            if (same(fields, original)) delete drafts[key];
            else drafts[key] = {fields: copy(fields), base: drafts[key]?.base || base};
        },
        get: key => drafts[key] && copy(drafts[key]),
        keys: () => Object.keys(drafts),
        stale: (key, base) => Boolean(drafts[key] && drafts[key].base !== base),
        remove: key => {delete drafts[key];},
        clear: () => {drafts = {};},
        accept(key, fields, previous, next) {
            if (key && same(drafts[key]?.fields, fields)) delete drafts[key];
            for (const draft of Object.values(drafts)) if (draft.base === previous) draft.base = next;
        },
        snapshot: () => copy(drafts),
        restore: value => {drafts = value && typeof value === "object" ? copy(value) : {};},
    };
}

module.exports = {createSettingsDraftStore};
