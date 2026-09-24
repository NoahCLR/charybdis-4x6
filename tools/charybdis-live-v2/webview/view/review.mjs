// The draft review: what it lists and how it hangs together.
//
// The host sends one item per thing that differs from the keyboard — a key,
// a behaviour, a pointing slot, a settings section — with whether it was
// added, changed or removed, the fields that differ, the group its edits tie
// it to, and where it is edited. These arrange that for reading: groups become
// blocks with one Discard, blocks sit under the area they start in, and every
// item knows how to go to its editor.

// The order areas are read in, as the rail lists the screens they live on.
export const AREAS = ["Layout", "Layers", "Behaviours", "Combos", "Macros", "Pointing modes", "Lighting", "Settings", "Profile", "Recovery"];

// Items as blocks: every item of a group together, in the order the host
// listed them, under the area of the group's first item. A group whose items
// span areas is shown once, whole, not split across tables.
export function reviewBlocks(changes) {
    const blocks = new Map();
    for (const change of changes) {
        const key = change.group ?? `solo:${blocks.size}`;
        if (!blocks.has(key)) blocks.set(key, {group: change.group, title: change.groupTitle || null, area: change.area, items: []});
        blocks.get(key).items.push(change);
    }
    const rank = (area) => (AREAS.indexOf(area) + 1 || AREAS.length + 1);
    const areas = [...new Set([...blocks.values()].map((block) => block.area))].sort((left, right) => rank(left) - rank(right));
    return areas.map((area) => {
        const own = [...blocks.values()].filter((block) => block.area === area);
        return {area, blocks: own, count: own.reduce((total, block) => total + block.items.length, 0)};
    });
}

// "3 added · 4 changed · 1 removed", leaving out a status nothing has.
export function statusSummary(changes) {
    const counts = {added: 0, changed: 0, removed: 0};
    for (const change of changes) if (change.status in counts) counts[change.status]++;
    return Object.entries(counts).filter(([, count]) => count).map(([status, count]) => `${count} ${status}`).join(" · ");
}

// A group's Discard names how much it takes back; an item on its own says
// Discard.
export const discardLabel = (block) => block.items.length === 1 ? "Discard" : block.items.length === 2 ? "Discard both" : `Discard all ${block.items.length}`;

// Where an item is edited, as the state that shows it. Nothing for an item
// with no editor of its own. `reveal` names the element the next render
// scrolls to and marks, for an editor that lists many things at once.
export function placeState(place, layers = []) {
    if (!place) return null;
    switch (place.kind) {
        case "key": {
            const layer = layers.findIndex((entry) => entry.index === place.layer);
            return {screen: "keys", tab: "key", layer: layer < 0 ? place.layer : layer, selected: place.layoutIndex};
        }
        case "behaviour": return {screen: "keys", tab: "behaviours", behaviourRow: place.keycode, behaviourRowShown: null, cell: null,
            reveal: ".rowitem.on"};
        // Which group lists a combo depends on the layer shown, so the combo is
        // asked for by id and the tab picks it where it finds it.
        case "combo": return {screen: "keys", tab: "combos", pickCombo: place.index, reveal: '[data-reach][data-picked="true"]'};
        case "macro": return {screen: "macros", macroSlot: `VIA_MACRO_${place.index}`};
        case "pointing": return {screen: "pointing", pdSlot: place.slot, pdKind: null, pdButtons: null};
        case "lighting": return place.stage ? {screen: "lighting", stage: place.stage} : {screen: "lighting"};
        case "settings": return place.section
            ? {screen: "settings", settingsSearch: "", settingsOpen: place.section, reveal: `.settings-group[data-section="${place.section}"]`}
            : {screen: "settings", settingsSearch: ""};
        case "layers": return {screen: "keys", layersOpen: true};
        default: return null;
    }
}

// How many fields an item shows before the rest fold away behind "Show all".
export const FIELDS_SHOWN = 4;

// ── marks in the editors ────────────────────────────────────────────────

// What the draft changed, by where it is edited, so every editor can mark its
// own changed things the way the review lists them. A removed thing has no
// place left to mark.
export function draftMarks(changes = []) {
    const marks = {keys: new Map(), layers: new Set(), behaviours: new Set(), combos: new Set(), macros: new Set(),
        pointing: new Set(), lighting: new Set(), lightingLayers: new Set(), lightingSlots: new Set(), settings: new Set(), layerNames: false};
    for (const {place, status} of changes) {
        if (!place || status === "removed") continue;
        if (place.kind === "key") {
            if (!marks.keys.has(place.layer)) marks.keys.set(place.layer, new Set());
            marks.keys.get(place.layer).add(place.layoutIndex);
            marks.layers.add(place.layer);
        } else if (place.kind === "behaviour") marks.behaviours.add(place.keycode);
        else if (place.kind === "combo") marks.combos.add(place.index);
        else if (place.kind === "macro") marks.macros.add(`VIA_MACRO_${place.index}`);
        else if (place.kind === "pointing") marks.pointing.add(place.slot);
        else if (place.kind === "lighting") {
            if (place.stage) marks.lighting.add(place.stage);
            if (place.layer !== undefined) { marks.lightingLayers.add(place.layer); marks.layers.add(place.layer); }
            if (place.slot !== undefined) marks.lightingSlots.add(place.slot);
        } else if (place.kind === "settings" && place.section) marks.settings.add(place.section);
        else if (place.kind === "layers") marks.layerNames = true;
    }
    return marks;
}

// The mark itself, one look everywhere: the draft's dot, and a tip that says it.
export const draftDot = (tip = "Changed in your draft") => `<i class="draft-dot" data-tip="${tip}" aria-label="${tip}"></i>`;
