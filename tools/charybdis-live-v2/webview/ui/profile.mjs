// Profile & backups: the whole keyboard as one file.
//
// Layer names and priority are edited beside the board, in Keys → Layers, where
// what a layer holds is visible while it is named.
//
// A complete profile is layers, behaviours, combos, both macro banks, lighting
// and settings. Restoring one replaces all of that, so the review says what is
// in the file against what is on the keyboard before anything is written.

import {el, esc} from "../lib/dom.mjs";
import {getModel, post} from "../store.mjs";
import {topbar} from "./shell.mjs";

export function screenProfile() {
    const model = getModel();
    const portable = model?.portable || {};
    const health = model?.device?.health || {};
    const busy = Boolean(portable.busy);

    const main = el(`<div class="main">${topbar(
        "Profile & backups",
        "A complete backup is everything the keyboard stores: layers, behaviours, combos, both macro banks, lighting and settings.",
        `<button class="btn ghost" data-act="import" ${portable.available && portable.eightLayers && !busy ? "" : "disabled"}>Import…</button>
         <button class="btn" data-act="export" ${portable.available && !busy ? "" : "disabled"}
            data-tip="Write a complete backup of everything the keyboard stores to a file.">Export profile</button>`,
    )}</div>`);

    const content = el(`<div class="content"><div class="pad" style="max-width:960px;display:grid;gap:14px"></div></div>`);
    const pad = content.firstElementChild;

    if (portable.progress) pad.appendChild(el(`<div class="notice"><span class="spin"></span><span>${esc(portable.progress)}</span></div>`));
    if (!portable.available) {
        pad.appendChild(el(`<div class="unavailable">${esc(portable.legacy
            ? "This keyboard runs the five-layer firmware. Install the backup bridge, export your profile there, then install the eight-layer update."
            : "Connect a keyboard with complete-profile firmware to manage its backups and layers.")}</div>`));
    }
    if (portable.review) pad.appendChild(reviewCard(model, portable, busy));

    if (portable.pdUpgradeAvailable) {
        const card = el(`<div class="card"><div class="card-h"><h3>Firmware upgrade</h3>
            <span class="right"><span class="chip"><i class="dot draft"></i>geometry change ahead</span></span></div>
            <div class="card-b" style="display:grid;gap:10px">
                <p class="note">Before flashing a pair that changes the stored geometry, export a verified original and a migrated copy through the old-geometry bridge. The migrated file is the one that restores onto the new firmware.</p>
                <button class="btn" style="justify-self:start" data-act="upgrade" ${busy ? "disabled" : ""}>Export upgrade pair…</button>
            </div></div>`);
        card.querySelector('[data-act="upgrade"]').addEventListener("click", () => post({type: "exportPdUpgrade"}));
        pad.appendChild(card);
    }

    pad.appendChild(el(`<div class="grid2">
        <div class="card"><div class="card-h"><h3>Layer priority</h3></div>
            <div class="card-b"><p class="note">Layer names and order live in <b>Keys → Layers</b>, where the board shows what each one holds.</p></div></div>
        <div class="card"><div class="card-h"><h3>Recovery</h3>
            <span class="right"><span class="chip"><i class="dot ${health.recoveryPending ? "draft" : "on"}"></i>${health.recoveryPending ? "pending" : "clear"}</span></span></div>
            <div class="card-b"><p class="note">${health.recoveryPending
                ? "An apply was interrupted. The keyboard is still running its last complete profile; the recovery copy restores it, and Apply retries from there."
                : "A recovery copy is written before every apply and released once both halves confirm. Nothing is outstanding."}</p></div></div>
    </div>`));

    main.appendChild(content);
    main.querySelector('[data-act="export"]').addEventListener("click", () => post({type: "exportPortableProfile"}));
    main.querySelector('[data-act="import"]').addEventListener("click", () => post({type: "choosePortableProfile"}));
    return main;
}

function reviewCard(model, portable, busy) {
    const {incoming, current} = portable.review;
    const rows = [["Layers", "layers"], ["Key behaviours", "behaviors"], ["Combos", "combos"], ["Macros with content", "macros"]];
    const node = el(`<div class="card">
        <div class="card-h"><h3>${model?.draft ? "Use this profile as your draft?" : "Restore this profile?"}</h3></div>
        <div class="card-b" style="display:grid;gap:12px">
            <table class="t"><thead><tr><th>Contents</th><th>On the keyboard</th><th>In this file</th></tr></thead>
                <tbody>${rows.map(([label, key]) => {
                    const from = current ? String(current[key]) : "—";
                    const to = String(incoming[key]);
                    const same = from === to;
                    return `<tr><td>${label}</td>
                        <td class="mono ${same ? "muted" : "del"}">${esc(from)}</td>
                        <td class="mono ${same ? "muted" : "ins"}">${esc(to)}</td></tr>`;
                }).join("")}</tbody></table>
            <div class="callout warn">${esc(model?.draft
                ? "This replaces your local draft. Review its differences before applying anything to the keyboard."
                : current
                    ? "This replaces the keyboard's layout, behaviours, combos, macros, lighting and settings. A recovery copy is saved automatically before restoring, and both halves are verified afterwards."
                    : "An interrupted restore left an incomplete configuration. This profile replaces it; the interrupted data is kept as a diagnostic copy, so keep your original backup as well.")}</div>
            <div class="row" style="gap:8px">
                <button class="btn primary" data-act="restore" ${busy ? "disabled" : ""}>${model?.draft ? "Use as draft" : "Restore profile"}</button>
                <button class="btn ghost" data-act="cancel" ${busy ? "disabled" : ""}>Cancel</button></div>
        </div></div>`);
    node.querySelector('[data-act="restore"]').addEventListener("click", () => post({type: "restorePortableProfile"}));
    node.querySelector('[data-act="cancel"]').addEventListener("click", () => post({type: "cancelPortableReview"}));
    return node;
}
