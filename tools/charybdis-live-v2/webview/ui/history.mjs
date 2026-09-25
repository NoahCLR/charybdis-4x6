// The draft history sheet: what happened to the draft, step by step. Where
// the review compares the draft with the keyboard, this compares each step
// with the step before it, so an edit reads as what it did when it was made,
// undone steps included. Going to a step is several undos or redos at once.

import {el, esc} from "../lib/dom.mjs";
import {getModel, post, render, state} from "../store.mjs";
import {statusSummary} from "../view/review.mjs";
import {historyEntries, stepTime} from "../view/history.mjs";
import {reviewColumns, reviewItem} from "./review-items.mjs";

export function openHistory() {
    state.overlay = "history";
    post({type: "openProfileDraftHistory"});
    render();
}

export function closeHistory() {
    state.overlay = null;
    post({type: "closeProfileDraftHistory"});
    render();
}

function stepCard(model, entry, now) {
    const time = stepTime(entry.at, now);
    const changes = entry.changes;
    const titleSlot = Boolean(changes?.some((change) => change.titleMark));
    const summary = changes?.length ? statusSummary(changes) : "";
    const body = changes === null
        ? `<p class="note hs-empty">${entry.step === 0 && entry.label === "Read from the keyboard"
            ? "The draft began here, from the profile the keyboard holds."
            : "The oldest step the history still keeps. What it changed is no longer known."}</p>`
        : changes.length
            ? `<div class="rv-block">${changes.map((change) => reviewItem(model, change, {titleSlot})).join("")}</div>`
            : `<p class="note hs-empty">Nothing that the review lists changed in this step.</p>`;
    const where = entry.current
        ? `<span class="hs-here"><i class="dot draft"></i> Your draft is here</span>`
        : `<button class="btn tiny" data-step="${entry.step}" ${model.draft.busy ? "disabled" : ""}
            data-tip="${esc(entry.undone ? "Redo every step up to and including this one." : "Undo every step after this one. Redo brings them back.")}">${entry.undone ? "Redo to here" : "Go back to here"}</button>`;
    return `<section class="hs-step ${entry.current ? "current" : ""} ${entry.undone ? "undone" : ""}">
        <div class="hs-head">
            <span class="hs-time" data-tip="${esc(time.clock)}">${esc(time.label)}</span>
            <span class="hs-label">${esc(entry.label)}${entry.undone ? ' <span class="tag">undone</span>' : ""}</span>
            ${summary ? `<span class="note">${esc(summary)}</span>` : ""}
            <span class="hs-go">${where}</span>
        </div>
        ${body}</section>`;
}

export function historyOverlay() {
    const model = getModel();
    const draft = model?.draft;
    if (state.overlay !== "history" || !draft) return null;
    const entries = draft.steps ? historyEntries(draft.steps) : null;
    const now = Date.now();
    const count = draft.historyLength - 1;
    const node = el(`<div class="scrim"><div class="sheet" role="dialog" aria-modal="true" aria-label="Draft history">
        <div class="sheet-h"><h2>Draft history</h2>
            <span class="note">${count ? `${count} step${count === 1 ? "" : "s"} · each compared with the step before it` : "No edits yet"}</span>
            <span class="right" style="margin-left:auto"><button class="btn ghost" data-act="close">Close</button></span></div>
        <div class="sheet-b rv hs">
            ${entries ? `<div class="hs-cols">${reviewColumns("What changed", "Before", "After")}</div>
                ${entries.map((entry) => stepCard(model, entry, now)).join("")}`
                : `<p class="note hs-empty">Reading the history…</p>`}
        </div>
        <div class="sheet-f"><span class="note">Going to a step changes only the draft. The keyboard is not touched until you apply.</span></div>
    </div></div>`);
    node.addEventListener("click", (event) => {
        const step = event.target.closest("[data-step]");
        if (step) {
            post({type: "jumpProfileDraft", step: Number(step.dataset.step)});
            return;
        }
        if (event.target === node || event.target.closest('[data-act="close"]')) closeHistory();
    });
    return node;
}
