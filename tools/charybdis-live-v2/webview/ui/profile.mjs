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
    const canExport = portable.available && !busy;
    const canImport = portable.available && portable.eightLayers && !busy;

    const main = el(`<div class="main">${topbar(
        "Profile & backups",
        "A complete backup is everything the keyboard stores: layers, behaviours, combos, both macro banks, lighting and settings.",
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

    const actions = el(`<div class="profile-actions">
        <div class="card profile-action">
            <div class="card-b">
                <span class="profile-action-mark"><svg viewBox="0 0 24 24"><path d="M12 3v12M7.5 10.5 12 15l4.5-4.5M4 19h16"/></svg></span>
                <h3>Export profile</h3>
                <p class="note">Save a complete, portable backup of the profile currently running on the keyboard. Keep it somewhere safe before experimenting or updating firmware.</p>
                <button class="btn primary" data-act="export" ${canExport ? "" : "disabled"}>Export profile…</button>
            </div>
        </div>
        <div class="card profile-action">
            <div class="card-b">
                <span class="profile-action-mark"><svg viewBox="0 0 24 24"><path d="M12 21V9M7.5 13.5 12 9l4.5 4.5M4 5h16"/></svg></span>
                <h3>Import profile</h3>
                <p class="note">Choose a complete backup and review its differences first. Import replaces the local draft; nothing is written to the keyboard until you review and apply it.</p>
                <button class="btn" data-act="import" ${canImport ? "" : "disabled"}>Choose profile…</button>
            </div>
        </div>
    </div>`);
    actions.querySelector('[data-act="export"]').addEventListener("click", () => post({type: "exportPortableProfile"}));
    actions.querySelector('[data-act="import"]').addEventListener("click", () => post({type: "choosePortableProfile"}));
    pad.appendChild(actions);

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

    pad.appendChild(el(`<div class="profile-recovery">
        <i class="dot ${health.recoveryPending ? "draft" : "on"}" style="margin-top:6px"></i>
        <div class="copy"><b>Automatic recovery</b><p class="note">${health.recoveryPending
            ? "An apply was interrupted. The keyboard is still running its last complete profile; the recovery copy restores it, and Apply retries from there."
            : "A recovery copy is written before every apply and released once both halves confirm. Nothing is outstanding."}</p></div>
        <span class="chip state">${health.recoveryPending ? "pending" : "clear"}</span>
    </div>`));

    main.appendChild(content);
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
