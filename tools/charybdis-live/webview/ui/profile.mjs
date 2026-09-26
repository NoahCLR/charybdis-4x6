// Profile & backups: the whole keyboard as one file.
//
// Layer names and priority are edited beside the board, in Keys → Layers, where
// what a layer holds is visible while it is named.
//
// A complete profile is layers, behaviours, combos, both macro banks, lighting
// and settings. Restoring one replaces all of that, so the card counts, area
// by area, what the file changes on the keyboard before anything is written.

import {el, esc} from "../lib/dom.mjs";
import {getModel, post, canEdit as canEditArea} from "../store.mjs";
import {topbar} from "./shell.mjs";
import {categorySummary, statusSummary} from "../view/review.mjs";
import {stageOrder} from "../view/vocabulary.mjs";

export function screenProfile() {
    const model = getModel();
    const portable = model?.portable || {};
    const health = model?.device?.health || {};
    const busy = Boolean(portable.busy);
    const canExport = canEditArea("export");
    const canImport = canEditArea("import");

    const main = el(`<div class="main">${topbar(
        "Profile & backups",
        "A complete backup is everything the keyboard stores: layers, behaviours, combos, both macro banks, lighting and settings.",
    )}</div>`);

    const content = el(`<div class="content"><div class="pad" style="max-width:960px;display:grid;gap:14px"></div></div>`);
    const pad = content.firstElementChild;

    if (portable.progress) pad.appendChild(el(`<div class="profile-progress"><span class="spin"></span><span>${esc(portable.progress)}</span></div>`));
    if (!portable.available) {
        pad.appendChild(el(`<div class="unavailable">${esc(portable.legacy
            ? "This keyboard runs the five-layer firmware. Install the backup bridge, export your profile there, then install the eight-layer update."
            : "Connect a keyboard with complete-profile firmware to manage its backups and layers.")}</div>`));
    }
    if (portable.review) pad.appendChild(reviewCard(model, portable, busy));

    const actions = el(`<div class="profile-actions">
        <div class="card profile-action">
            <div class="card-b">
                <span class="profile-action-mark"><svg viewBox="0 0 24 24"><path d="M12 21V9M7.5 13.5 12 9l4.5 4.5M4 5h16"/></svg></span>
                <h3>Import profile</h3>
                <p class="note">Choose a complete backup and review its differences first. Import replaces the local draft; nothing is written to the keyboard until you review and apply it.</p>
                <button class="btn" data-act="import" ${canImport ? "" : "disabled"}>Choose profile…</button>
            </div>
        </div>
        <div class="card profile-action">
            <div class="card-b">
                <span class="profile-action-mark"><svg viewBox="0 0 24 24"><path d="M12 3v12M7.5 10.5 12 15l4.5-4.5M4 19h16"/></svg></span>
                <h3>Export profile</h3>
                <p class="note">Save a complete, portable backup of the profile currently running on the keyboard. Keep it somewhere safe before experimenting or updating firmware.</p>
                <button class="btn primary" data-act="export" ${canExport ? "" : "disabled"}>Export profile…</button>
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

// The file against the keyboard, counted area by area: enough to decide
// whether to take it. What each difference is waits for the draft's own
// review, which the file becomes.
function reviewCard(model, portable, busy) {
    const {incoming, current, fileName, differences} = portable.review;
    const draft = model?.draft;
    const pending = draft?.dirty ? draft.changes.length : 0;
    const same = differences && !differences.length;
    const summary = differences ? categorySummary(differences, {names: incoming.names, stages: stageOrder(model)}) : [];
    const counted = differences?.length
        ? `${differences.length} difference${differences.length === 1 ? "" : "s"} from the keyboard · ${statusSummary(differences)}` : "";
    const callout = !current
        ? "An interrupted restore left an incomplete configuration, so this file cannot be compared with it. This profile replaces it; the interrupted data is kept as a diagnostic copy, so keep your original backup as well."
        : draft
            ? `${pending ? `This replaces your draft and its ${pending} change${pending === 1 ? "" : "s"}; Undo brings ${pending === 1 ? "it" : "them"} back. ` : ""}Nothing is written to the keyboard until you review and apply; the review shows every change field by field.`
            : "This replaces the keyboard's layout, behaviours, combos, macros, lighting and settings. A recovery copy is saved automatically before restoring, and both halves are verified afterwards.";
    const node = el(`<div class="card">
        <div class="card-h"><h3>${draft ? "Use this profile as your draft?" : "Restore this profile?"}</h3>
            ${fileName ? `<span class="right note mono">${esc(fileName)}</span>` : ""}</div>
        <div class="card-b" style="display:grid;gap:12px">
            ${counted ? `<p class="note">${esc(counted)}</p>` : ""}
            ${same ? `<div class="callout">This file holds exactly what the keyboard runs. Using it changes nothing.</div>` : ""}
            ${summary.length ? `<table class="t import-diff"><thead><tr><th>What this file changes</th><th>Differences</th><th></th></tr></thead>
                <tbody>${summary.map((group) => `<tr class="cat"><td>${esc(group.category)}</td><td class="mono">${group.count}</td><td class="note">${esc(group.status)}</td></tr>
                    ${group.rows.map((row) => `<tr class="sub"><td>${esc(row.label)}</td><td class="mono">${row.count}</td><td></td></tr>`).join("")}`).join("")}</tbody></table>` : ""}
            ${same ? "" : `<div class="callout warn">${esc(callout)}</div>`}
            <div class="row" style="gap:8px">
                <button class="btn primary" data-act="restore" ${busy || same ? "disabled" : ""}>${draft ? "Use as draft" : "Restore profile"}</button>
                <button class="btn ghost" data-act="cancel" ${busy ? "disabled" : ""}>Cancel</button></div>
        </div></div>`);
    node.querySelector('[data-act="restore"]').addEventListener("click", () => post({type: "restorePortableProfile"}));
    node.querySelector('[data-act="cancel"]').addEventListener("click", () => post({type: "cancelPortableReview"}));
    return node;
}
