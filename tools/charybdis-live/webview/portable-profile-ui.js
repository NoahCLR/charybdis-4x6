"use strict";
const {renderProfileDraft} = require("./profile-draft-ui");
function renderPortableProfile(document, model, post) {
    const host = document.getElementById("portableProfile");
    if (!host) return;
    const state = model.portable || {}, busy = Boolean(state.busy);
    if (!state.layers) host.portableNames = null;
    else if (!host.portableNames || host.portableNames.key !== state.layers.key) host.portableNames = {key: state.layers.key, values: [...state.layers.names]};
    const el = (tag, text, className) => {const node = document.createElement(tag); if (text !== undefined) node.textContent = text; if (className) node.className = className; return node;};
    const button = (text, action, disabled = false) => {const b = el("button", text); b.type = "button"; b.disabled = busy || disabled; b.onclick = action; return b;};
    host.replaceChildren();
    const toolbar = el("div"); toolbar.style.cssText = "display:flex;align-items:center;gap:10px;flex-wrap:wrap;padding:12px 0";
    toolbar.append(button("Export profile", () => post({type: "exportPortableProfile"}), !state.available));
    toolbar.append(button("Import profile", () => post({type: "choosePortableProfile"}), !state.available || !state.eightLayers));
    toolbar.append(button("Manage layers", () => post({type: "managePortableLayers"}), !state.available || !state.eightLayers));
    const guidance = state.legacy
        ? (state.available ? "Export your profile before installing the eight-layer update." : "Install the five-layer backup bridge first. Export your profile there before installing the eight-layer update.")
        : (state.available ? "A complete backup of the saved keyboard configuration." : "Connect a keyboard with complete-profile firmware to manage its backups and layers.");
    toolbar.append(el("span", state.progress || guidance, "muted"));
    host.append(toolbar);
    if (model.draft) renderProfileDraft(document, host, model, post);
    if (state.review) {
        const review = el("section"); review.setAttribute("aria-label", "Review profile import"); review.style.cssText = "padding:18px;border:1px solid var(--border);border-radius:12px;margin-bottom:18px";
        review.append(el("h2", model.draft ? "Use this profile as your draft?" : "Restore this profile?"));
        const incoming = state.review.incoming, current = state.review.current;
        for (const [label, key] of [["Layers", "layers"], ["Key behaviours", "behaviors"], ["Combos", "combos"], ["Macros with content", "macros"]]) review.append(el("p", `${label}: ${current ? current[key] + " → " : ""}${incoming[key]}`));
        review.append(el("p", model.draft ? "This replaces your local draft. Review its differences before applying anything to the keyboard." : current ? "This replaces the keyboard's layout, behaviours, combos, macros, lighting and settings. A recovery copy is saved automatically before restoring." : "An interrupted restore left incomplete macros. This profile will replace the incomplete configuration. The interrupted data will be kept as a diagnostic copy; retain your original backup for recovery."));
        review.append(button(model.draft ? "Use as draft" : "Restore profile", () => post({type: "restorePortableProfile"})), button("Cancel", () => post({type: "cancelPortableReview"})));
        host.append(review);
    }
    if (state.layers) {
        const names = host.portableNames.values;
        const currentNames = () => {
            for (const input of host.querySelectorAll("input[data-portable-layer]")) names[Number(input.dataset.portableLayer)] = input.value;
            return [...names];
        };
        const panel = el("section"); panel.setAttribute("aria-label", "Layer priority"); panel.style.cssText = "padding:18px;border:1px solid var(--border);border-radius:12px;margin-bottom:18px";
        panel.append(el("h2", "Layer priority"), el("p", "Higher layers take priority. Base stays underneath. Moving a layer updates the keys and settings that refer to it."));
        const list = el("ol"); list.style.cssText = "list-style:none;padding:0;display:grid;gap:8px";
        for (const oldIndex of [...state.layers.order].reverse()) {
            const position = state.layers.order.indexOf(oldIndex), row = el("li"); row.style.cssText = "display:flex;align-items:center;gap:8px";
            const name = el("input"); name.type = "text"; name.value = names[oldIndex]; name.disabled = busy; name.dataset.portableLayer = oldIndex;
            name.style.cssText = "flex:1;min-width:0";
            name.setAttribute("aria-label", oldIndex ? `Name for layer ${oldIndex}` : "Base layer name");
            name.oninput = event => {event.stopPropagation(); names[oldIndex] = name.value;}; row.append(name);
            if (oldIndex) {
                const up = button("Move up", () => post({type: "editPortableLayer", id: oldIndex, direction: 1, names: currentNames()}), position === 7);
                const down = button("Move down", () => post({type: "editPortableLayer", id: oldIndex, direction: -1, names: currentNames()}), position === 1);
                up.style.whiteSpace = down.style.whiteSpace = "nowrap"; row.append(up, down);
            }
            else row.append(el("span", "Base · always underneath", "muted"));
            list.append(row);
        }
        panel.append(list, button(model.draft ? "Keep layer changes" : "Save layer changes", () => post({type: "savePortableLayers", names: currentNames()})), button("Cancel", () => post({type: "cancelPortableReview"})));
        host.append(panel);
    }
}
module.exports = {renderPortableProfile};
