"use strict";

// Whether a profile's actions sit where the keyboard can run them. The
// keyboard checks a profile it is asked to save and refuses the whole domain
// when one action is misplaced, so the editors check every row before they
// encode, and an upload is checked before it starts, with a message naming the
// row instead of the keyboard's bare rejection.

const {PLACEMENT, actionLimitsFor, actionName, behaviorEmitProblem, keycodeAction, placementProblem} = require("../schema/actions");
const {decodeProfileBlob, PROFILE_DOMAIN_IDS} = require("../schema/profile-blob-v1");
const {decodeKeyBehaviorDomain, KEY_BEHAVIOR_HOLD_MODES} = require("../schema/key-behavior-domain-v1");
const {decodeComboDomainV1} = require("../schema/combo-domain-v1");

function behaviorPlacementProblem(rows, options) {
    for (const row of rows) {
        const where = `The behaviour on ${actionName(row.target)}`;
        // The target is sent too: a tap with no branch of its own sends the key.
        const at = (action, placement) => placementProblem(action, placement, options) || behaviorEmitProblem(action, options);
        const problem = at(row.target, PLACEMENT.KEY)
            || row.steps.map(step => (step.tap && at(step.tap, PLACEMENT.TAP))
                || [step.hold, step.longHold].map(hold => hold && at(hold.action,
                    hold.mode === KEY_BEHAVIOR_HOLD_MODES.PRESS_AND_HOLD_UNTIL_RELEASE ? PLACEMENT.HOLD_PRESS : PLACEMENT.HOLD_OTHER)).find(Boolean))
                .find(Boolean);
        if (problem) return `${where}: ${problem}`;
    }
    return undefined;
}

function comboPlacementProblem(rows, options) {
    for (const [index, row] of rows.entries()) {
        const problem = placementProblem(row.output, PLACEMENT.COMBO_OUTPUT, options);
        if (problem) return `Combo ${index}: ${problem}`;
    }
    return undefined;
}

// Whether a native keycode can sit on a layout key. The keyboard's save check
// covers behaviours and combos only; a layout key it does not own (DF(), PDF(),
// a layer past the bank) would reach QMK's own layer code, so the editors
// refuse to place one.
const keyPlacementProblem = (code, options) => placementProblem(keycodeAction(code), PLACEMENT.KEY, options);

// The first misplaced action in an encoded profile, or undefined.
function profilePlacementProblem(bytes, options) {
    const profile = decodeProfileBlob(bytes);
    const actionOptions = actionLimitsFor(profile.schema.major);
    const domain = id => profile.domains.find(row => row.id === id)?.payload;
    const behaviors = domain(PROFILE_DOMAIN_IDS.KEY_BEHAVIORS);
    const combos = domain(PROFILE_DOMAIN_IDS.COMBOS);
    return (behaviors && behaviorPlacementProblem(decodeKeyBehaviorDomain(behaviors, actionOptions).rows, options))
        || (combos && comboPlacementProblem(decodeComboDomainV1(combos, actionOptions), options))
        || undefined;
}

module.exports = {behaviorPlacementProblem, comboPlacementProblem, keyPlacementProblem, profilePlacementProblem};
