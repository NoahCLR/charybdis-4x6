// The draft review: what it lists and how it hangs together.
//
// The host sends one item per thing that differs from the keyboard — a key,
// a behaviour, a pointing slot, a settings section — with whether it was
// added, changed or removed, the fields that differ, the group its edits tie
// it to, and where it is edited. These arrange that for reading: every item
// sits under its own area, groups become blocks with one Discard, and every
// item knows how to go to its editor.

// The order areas are read in, as the rail lists the screens they live on.
export const AREAS = ["Layout", "Layers", "Behaviours", "Combos", "Macros", "Pointing modes", "Lighting", "Settings", "Profile", "Recovery"];
const rank = (area) => (AREAS.indexOf(area) + 1 || AREAS.length + 1);

// Items by area, in rail order, and within an area as blocks in the order the
// host listed them. A group whose items span areas is shown in each area as
// the part of it that lives there: every part carries the group's size, for
// its Discard, and the other areas it reaches, so a swap of two layers reads
// as keys under Layout, behaviours under Behaviours, and so on.
export function reviewBlocks(changes) {
    const reach = new Map();
    for (const change of changes) {
        const group = reach.get(change.group) ?? {size: 0, areas: new Set()};
        group.size++;
        group.areas.add(change.area);
        reach.set(change.group, group);
    }
    const sections = new Map();
    changes.forEach((change, index) => {
        if (!sections.has(change.area)) sections.set(change.area, new Map());
        const blocks = sections.get(change.area), key = change.group ?? `solo:${index}`;
        if (!blocks.has(key)) {
            const group = change.group === null || change.group === undefined ? {size: 1, areas: new Set([change.area])} : reach.get(change.group);
            blocks.set(key, {group: change.group, title: change.groupTitle || null, area: change.area, size: group.size,
                elsewhere: [...group.areas].filter((area) => area !== change.area).sort((left, right) => rank(left) - rank(right)), items: []});
        }
        blocks.get(key).items.push(change);
    });
    return [...sections.keys()].sort((left, right) => rank(left) - rank(right)).map((area) => {
        const blocks = [...sections.get(area).values()];
        return {area, blocks, count: blocks.reduce((total, block) => total + block.items.length, 0)};
    });
}

// "3 added · 4 changed · 1 removed", leaving out a status nothing has.
export function statusSummary(changes) {
    const counts = {added: 0, changed: 0, removed: 0};
    for (const change of changes) if (change.status in counts) counts[change.status]++;
    return Object.entries(counts).filter(([, count]) => count).map(([status, count]) => `${count} ${status}`).join(" · ");
}

// A profile file's differences as counts, by what they configure rather than
// by which screen stores them: the categories of the rail, each split the way
// its screen is, and the Settings sections filed with what they tune (Key
// Timing with keys, Sniping with pointing). `names` are the file's layer names;
// `stages` the lighting stages in paint order, as the vocabulary words them.
export const CATEGORIES = ["Keys", "Lighting", "Macros", "Pointing modes", "Other"];
const SECTIONS = {
    keyTiming: ["Keys", "Key timing", 31], startupLayers: ["Keys", "Startup layers", 22], comboReferences: ["Keys", "Combo layer matching", 41],
    keyboardOptions: ["Keys", "Key options", 50], rgbAppearance: ["Lighting", "Base effect", 1], lightingFeedback: ["Lighting", "Key feedback", 15],
    normalPointerSpeed: ["Pointing modes", "Pointer speed", 1], pointingModeSpeeds: ["Pointing modes", "Mode speeds", 2],
    sniping: ["Pointing modes", "Sniping", 3], autoMouse: ["Pointing modes", "Auto-mouse", 4],
};
// Where one difference is counted: its category, its row's words, and the
// row's place within the category.
function categoryOf(item, names, stages) {
    const place = item.place || {};
    switch (place.kind) {
        case "key": return ["Keys", `Keys on ${names[place.layer] || `Layer ${place.layer}`}`, place.layer];
        case "layers": return item.unit === "layerOrder" ? ["Keys", "Layer priority", 20] : ["Keys", "Layer names", 21];
        case "behaviour": return ["Keys", "Behaviours", 30];
        case "combo": return ["Keys", "Combos", 40];
        case "macro": return ["Macros", "Macros", 0];
        case "pointing": return ["Pointing modes", "Modes", 0];
        case "lighting": {
            if (!place.stage) return ["Lighting", "Stages on or off", 0];
            if (place.stage === "groups") return ["Lighting", "LED groups", 20];
            const index = stages.findIndex((stage) => stage.id === place.stage);
            return ["Lighting", stages[index]?.label || place.stage, 10 + Math.max(index, 0)];
        }
        case "settings": return SECTIONS[place.section] || (item.unit === "settings:otherKeyOptions" ? SECTIONS.keyboardOptions : ["Other", item.title, 0]);
        default: return ["Other", item.title, 0];
    }
}
export function categorySummary(items, {names = [], stages = []} = {}) {
    const categories = new Map();
    for (const item of items) {
        const [category, label, order] = categoryOf(item, names, stages);
        if (!categories.has(category)) categories.set(category, {items: [], rows: new Map()});
        const entry = categories.get(category);
        entry.items.push(item);
        if (!entry.rows.has(label)) entry.rows.set(label, {label, order, count: 0});
        entry.rows.get(label).count++;
    }
    const rank = (category) => CATEGORIES.indexOf(category);
    // A category whose one row is itself, as Macros is, has no rows to show.
    return [...categories].sort(([left], [right]) => rank(left) - rank(right)).map(([category, entry]) => {
        const rows = [...entry.rows.values()].sort((left, right) => left.order - right.order).map(({label, count}) => ({label, count}));
        return {category, count: entry.items.length, status: statusSummary(entry.items), rows: rows.length === 1 && rows[0].label === category ? [] : rows};
    });
}

// A group's Discard names how much it takes back, counting the parts shown
// under other areas too; an item on its own says Discard.
export const discardLabel = (block) => block.size === 1 ? "Discard" : block.size === 2 ? "Discard both" : `Discard all ${block.size}`;

// What a group's header says about its size: all of it, or the part here and
// where the rest is.
export const groupNote = (block) => block.items.length === block.size
    ? `${block.size} changes, discarded together`
    : `${block.items.length} of ${block.size} here, the rest in ${listed(block.elsewhere)} · discarded together`;
const listed = (names) => names.length < 3 ? names.join(" and ") : `${names.slice(0, -1).join(", ")} and ${names.at(-1)}`;

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
            ? {screen: "settings", settingsSearch: "", settingsOpen: [place.section], reveal: `.settings-group[data-section="${place.section}"]`}
            : {screen: "settings", settingsSearch: ""};
        case "layers": return {screen: "keys", layersOpen: true};
        default: return null;
    }
}

// How many fields an item shows before the rest fold away behind "Show all".
export const FIELDS_SHOWN = 4;

// ── marks in the editors ────────────────────────────────────────────────

// A removed behaviour or combo is gone from its list; a cleared pointing slot
// or emptied macro slot is still on screen, so it keeps its mark and its Show.
export const stillShown = (change) => change.status !== "removed" || ["pointing", "macro"].includes(change.place?.kind);

// What the draft changed, by where it is edited, so every editor can mark its
// own changed things the way the review lists them.
// Every screen asks for these on every render; the answer is kept with the
// changes list, which only changes when a new model arrives.
const marksCache = new WeakMap();
export function draftMarks(changes = []) {
    if (marksCache.has(changes)) return marksCache.get(changes);
    const marks = computeMarks(changes);
    marksCache.set(changes, marks);
    return marks;
}
function computeMarks(changes) {
    const marks = {keys: new Map(), layers: new Set(), behaviours: new Set(), combos: new Set(), macros: new Set(),
        pointing: new Set(), lighting: new Set(), lightingLayers: new Set(), lightingSlots: new Set(), settings: new Set(), layerNames: false};
    for (const change of changes) {
        const {place} = change;
        if (!place || !stillShown(change)) continue;
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
        else if (place.kind === "layers") {
            // A reorder marks the layers it moved, not every key they hold.
            marks.layerNames = true;
            for (const layer of place.layers || []) marks.layers.add(layer);
        }
    }
    return marks;
}

// The mark itself, one look everywhere: the draft's dot, and a tip that says
// it. In a row it follows the label; on a tile (a key, a macro slot, a
// pointing slot card) it sits in the tile's top-right corner, as on the board.
export const draftDot = (tip = "Changed in your draft", place = "") => `<i class="draft-dot ${place}" data-tip="${tip}" aria-label="${tip}"></i>`;
