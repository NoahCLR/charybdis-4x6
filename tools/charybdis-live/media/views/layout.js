import {element, panel} from "../dom.js";
import {KEYBOARD_GEOMETRY, keyVisual} from "../geometry.js";

const SVG_NS = "http://www.w3.org/2000/svg";

// Everything drawn here came off the keyboard. There is no authored source in
// this view, so what you see is what the device is running.
export function renderLayout(snapshot, actions) {
    const layout = snapshot.layout;
    const body = [renderControls(snapshot, actions)];

    if (!layout || layout.state === "reading") {
        body.push(renderProgress(layout));
    } else if (layout.state === "read") {
        body.push(renderLayerTabs(layout, snapshot.activeLayer ?? 0, actions.selectLayer));
        body.push(renderBoard(layout, snapshot.activeLayer ?? 0));
        body.push(renderFooter(layout));
    }

    return panel("Layout", body);
}

function renderControls(snapshot, actions) {
    const row = element("div", {class: "layout__controls"});
    const read = element("button", {}, snapshot.layout?.state === "read" ? "Re-read layout" : "Read layout");
    read.disabled = Boolean(snapshot.busy) || !snapshot.connected || !snapshot.capabilities;
    read.addEventListener("click", actions.readLayout);
    row.append(read);

    if (!snapshot.capabilities) {
        row.append(element("span", {class: "muted"}, "Refresh capabilities first."));
    }
    return row;
}

function renderProgress(layout) {
    if (!layout) {
        return element("p", {class: "muted"}, "The layout has not been read from the keyboard yet.");
    }
    const {done = 0, total = 0} = layout.progress || {};
    const percent = total ? Math.round((done / total) * 100) : 0;
    return element("div", {class: "layout__progress"}, [
        element("div", {class: "layout__bar"}, [
            element("div", {class: "layout__bar-fill", style: `width:${percent}%`}),
        ]),
        element(
            "p",
            {class: "muted"},
            total
                ? `Reading ${done} of ${total} keys. Standard VIA has no bulk read, so this is one round trip per key.`
                : "Starting read…"
        ),
    ]);
}

function renderLayerTabs(layout, activeLayer, selectLayer) {
    const tabs = element("div", {class: "layout__tabs"});
    for (const entry of layout.layers) {
        const assigned = entry.keys.filter((key) => key.resolved.name !== "KC_TRANSPARENT" && key.resolved.name !== "KC_NO").length;
        const tab = element("button", {
            class: entry.layer === activeLayer ? "layout__tab is-active" : "layout__tab",
            title: `${assigned} assigned keys`,
        }, `Layer ${entry.layer}`);
        tab.addEventListener("click", () => selectLayer(entry.layer));
        tabs.append(tab);
    }
    return tabs;
}

function renderBoard(layout, activeLayer) {
    const entry = layout.layers.find((candidate) => candidate.layer === activeLayer) || layout.layers[0];
    const {viewBox, keyWidth, keyHeight, radius} = KEYBOARD_GEOMETRY;

    const svg = document.createElementNS(SVG_NS, "svg");
    svg.setAttribute("viewBox", `${viewBox.x} ${viewBox.y} ${viewBox.width} ${viewBox.height}`);
    svg.setAttribute("class", "layout__board");
    svg.setAttribute("role", "img");
    svg.setAttribute("aria-label", `Layer ${entry.layer} as read from the keyboard`);

    for (const key of entry.keys) {
        const visual = keyVisual(key.layoutIndex);
        const group = document.createElementNS(SVG_NS, "g");
        group.setAttribute("transform", `rotate(${visual.angle} ${visual.x} ${visual.y})`);
        group.setAttribute("class", keyClass(key));

        const rect = document.createElementNS(SVG_NS, "rect");
        rect.setAttribute("x", visual.x - keyWidth / 2);
        rect.setAttribute("y", visual.y - keyHeight / 2);
        rect.setAttribute("width", keyWidth);
        rect.setAttribute("height", keyHeight);
        rect.setAttribute("rx", radius);
        group.append(rect);

        const text = document.createElementNS(SVG_NS, "text");
        text.setAttribute("x", visual.x);
        text.setAttribute("y", visual.y);
        text.setAttribute("text-anchor", "middle");
        text.setAttribute("dominant-baseline", "middle");
        text.textContent = keyFace(key);
        group.append(text);

        const title = document.createElementNS(SVG_NS, "title");
        title.textContent = `${key.resolved.name}\n${key.resolved.label}\nrow ${key.row}, column ${key.column}`;
        group.append(title);

        svg.append(group);
    }
    return svg;
}

function keyClass(key) {
    const parts = ["layout__key"];
    if (!key.resolved.known) {
        parts.push("layout__key--unknown");
    } else if (key.resolved.name === "KC_TRANSPARENT") {
        parts.push("layout__key--transparent");
    } else if (key.resolved.name === "KC_NO") {
        parts.push("layout__key--empty");
    } else if (key.resolved.kind === "layer" || key.resolved.kind === "layer-tap") {
        parts.push("layout__key--layer");
    }
    return parts.join(" ");
}

// Short enough to fit a 58px cap, but never invented: fall back to the name so
// an unrecognised keycode is visible rather than blank.
function keyFace(key) {
    const {resolved} = key;
    if (resolved.name === "KC_TRANSPARENT") {
        return "▽";
    }
    if (resolved.name === "KC_NO") {
        return "";
    }
    if (resolved.kind === "layer-tap") {
        return shorten(resolved.tap);
    }
    if (resolved.kind === "layer") {
        return `L${resolved.layer}`;
    }
    if (resolved.kind === "mod-tap") {
        return shorten(resolved.tap);
    }
    return resolved.label.length <= 5 ? resolved.label : shorten(resolved.name);
}

function shorten(name) {
    const stripped = String(name || "").replace(/^KC_/, "");
    return stripped.length <= 5 ? stripped : stripped.slice(0, 5);
}

function renderFooter(layout) {
    const unknown = layout.layers.flatMap((entry) => entry.keys).filter((key) => !key.resolved.known).length;
    const parts = [`Read from the keyboard at ${new Date(layout.readAt).toLocaleTimeString()}.`];
    if (layout.catalog) {
        parts.push(`Keycodes resolved against the vendored catalog for QMK ${layout.catalog.qmkVersion}.`);
    }
    if (unknown) {
        parts.push(`${unknown} keycodes are not in that catalog and are shown as hex.`);
    }
    return element("p", {class: "muted"}, parts.join(" "));
}
