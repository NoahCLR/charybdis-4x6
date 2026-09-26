// The grouped lists the Keys tabs share: a counted header per group, the
// first open and the rest a click away, and rows that pick themselves.
// Titles and order come from view/reach-groups.mjs.

import {el, esc} from "../lib/dom.mjs";
import {render, state} from "../store.mjs";
import {draftDot} from "../view/review.mjs";
import {GROUP_TITLES, inGroupOrder} from "../view/reach-groups.mjs";

// A folded group holding something the draft changed says so on its header;
// open, its rows carry their own marks.
export function groupHeader(tab, id, count, drafted = false) {
    const open = state.groups[tab][id];
    return `<button class="group-h" data-group-tab="${tab}" data-group="${id}" aria-expanded="${open}">
        <span class="chev">${open ? "▾" : "▸"}</span><span class="ttl">${esc(GROUP_TITLES[id])}${drafted && !open ? draftDot("Holds a change in your draft") : ""}</span>
        <span class="n">${count}</span></button>`;
}

export const groupOpen = (tab, id) => state.groups[tab][id];

export const attachGroupToggles = (node) => node.querySelectorAll("[data-group-tab]").forEach((button) =>
    button.addEventListener("click", () => {
        const bag = state.groups[button.dataset.groupTab];
        bag[button.dataset.group] = !bag[button.dataset.group];
        render();
    }));

// Rows across the tabs pick themselves the same way, and picking the same row
// again lets go of it.
export const attachReachRows = (node, tab) => node.querySelectorAll("[data-reach]").forEach((row) =>
    row.addEventListener("click", (event) => {
        if (event.target.closest("button")) return;
        const picked = row.dataset.reach;
        state.reachRow[tab] = state.reachRow[tab] === picked ? null : picked;
        render();
    }));

// One thing can be listed under several routes, so a pick names the route it
// was made from — otherwise picking it in one group would ring the keys of all
// of them.
export const reachAttrs = (tab, group, name) => {
    const picked = `${group}:${name}`;
    return ` data-reach="${esc(picked)}"${state.reachRow[tab] === picked ? ' data-picked="true"' : ""}`;
};

// A grouped table: the column header once, then a counted header row per
// group. One table keeps the columns lined up across the groups being
// compared, which is the point of showing them together. Each column is given
// its width, so unfolding a group never re-sizes the ones already on screen.
export function reachTable(tab, groups, columns) {
    const section = (group) => `<tbody class="rowgroup">
        <tr><td colspan="${columns.length}" class="t-group">${groupHeader(tab, group.id, group.rows.length, group.drafted)}</td></tr>
        ${groupOpen(tab, group.id)
            ? (group.rows.length ? group.rows.join("")
                : `<tr><td colspan="${columns.length}"><p class="note">${esc(group.empty)}</p></td></tr>`)
            : ""}</tbody>`;
    const node = el(`<div style="padding:2px 0"><table class="t fixed">
        <colgroup>${columns.map(([, width]) => `<col style="width:${width}">`).join("")}</colgroup>
        <thead><tr>${columns.map(([name]) => `<th>${esc(name)}</th>`).join("")}</tr></thead>
        ${inGroupOrder(groups).map(section).join("")}
    </table></div>`);
    attachGroupToggles(node);
    return node;
}
