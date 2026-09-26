// How the Keys tabs group what a layer reaches.
//
// Every tab that answers "what does this layer reach" groups its rows the
// same way and in the same order: what the layer holds itself, then what it
// only reaches under a transparent key, then the rest of the board. The reach
// functions in keyface.mjs return one list per group; these name the groups,
// order them, and say which list each reads.

export const GROUP_TITLES = {
    here: "On this layer",
    branches: "Through a behaviour on this layer",
    combos: "Through a combo on this layer",
    through: "Through a transparent key",
    belowBranches: "Through a behaviour under a transparent key",
    belowCombos: "Through a combo under a transparent key",
    elsewhere: "Unreachable from this layer",
};

// A tab lists the groups it has in any order and they come out in this one,
// so the headers do not move between tabs.
export const GROUP_ORDER = ["here", "branches", "combos", "through", "belowBranches", "belowCombos", "elsewhere"];
export const inGroupOrder = (groups) => GROUP_ORDER.map((id) => groups.find((group) => group.id === id)).filter(Boolean);

// The Behaviours rail is an accordion: opening one route closes the others.
export const singleOpenGroup = (groups, id, open = true) => Object.fromEntries(
    Object.keys(groups).map((group) => [group, group === id && open]));

// The reach list each group reads. `elsewhere` is a tab's own remainder.
export const REACH_FIELDS = {here: "onKeys", branches: "fromBranches", combos: "fromCombos", through: "throughKeys",
    belowBranches: "fromBranchesBelow", belowCombos: "fromCombosBelow"};
export const reachEntries = (reach, group) => reach?.[REACH_FIELDS[group]] || [];
