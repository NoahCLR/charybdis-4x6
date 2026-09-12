"use strict";

function renderProfileDraft(document, host, model, post) {
    const draft = model.draft;
    if (!draft) return;
    const element = (tag, text) => {const node = document.createElement(tag); if (text !== undefined) node.textContent = text; return node;};
    const blocked = draft.busy || !draft.connected || draft.stale;
    const button = (label, type, disabled = false) => {
        const node = element("button", label); node.type = "button"; node.disabled = draft.busy || disabled;
        node.onclick = () => post({type, draftRevision: draft.revision}); return node;
    };
    const panel = element("section"); panel.setAttribute("aria-label", "Profile changes");
    panel.style.cssText = "border:1px solid var(--border);border-radius:12px;padding:16px;margin-bottom:16px";
    const toolbar = element("div"); toolbar.style.cssText = "display:flex;gap:10px;align-items:center;flex-wrap:wrap";
    toolbar.append(element("strong", draft.dirty ? `${draft.changes.length} profile change${draft.changes.length === 1 ? "" : "s"}` : "No changes to apply"));
    toolbar.append(button("Review changes", "reviewProfileDraft", blocked || !draft.dirty), button("Undo", "undoProfileDraft", blocked || !draft.canUndo), button("Redo", "redoProfileDraft", blocked || !draft.canRedo));
    if (draft.dirty || draft.canUndo || draft.canRedo) toolbar.append(button("Discard draft", "discardProfileDraft", !model.portable?.available));
    panel.append(toolbar, element("p", "Keep edits in your draft, then review and apply them together. Changes stay in this window until applied; the keyboard keeps running its saved profile."));
    if (!draft.connected) panel.append(element("p", "Reconnect the keyboard this draft belongs to before applying changes."));
    if (draft.stale) {
        panel.append(element("p", "The keyboard changed since this draft began. Your changes are kept. Review against the latest keyboard state before deciding what to apply."));
        panel.append(button("Review against keyboard", "rebaseProfileDraft", !draft.connected));
    }
    if (draft.reviewed && draft.dirty) {
        const review = element("div"); review.setAttribute("aria-label", "Review profile changes");
        review.append(element("h2", "Review changes"));
        for (const area of new Set(draft.changes.map(change => change.area))) {
            const group = element("details"); group.open = true;
            group.append(element("summary", area));
            const table = element("table"); table.style.cssText = "width:100%;table-layout:fixed;margin:10px 0";
            const head = element("thead"), headings = element("tr");
            for (const title of ["Setting", "On keyboard", "In your draft"]) headings.append(element("th", title));
            head.append(headings); table.append(head);
            const body = element("tbody");
            for (const change of draft.changes.filter(change => change.area === area)) {
                const row = element("tr");
                for (const text of [change.label, change.before, change.after]) {const cell = element("td", text); cell.style.cssText = "white-space:pre-wrap;overflow-wrap:anywhere;vertical-align:top"; row.append(cell);}
                body.append(row);
            }
            table.append(body); group.append(table); review.append(group);
        }
        review.append(element("p", "Apply saves a recovery copy, writes the complete profile and verifies both halves. If interrupted, use the recovery copy to restore the keyboard."));
        const apply = button("Apply to keyboard", "applyProfileDraft", blocked); apply.className = "primary";
        const actions = element("div"); actions.style.cssText = "display:flex;gap:10px;flex-wrap:wrap";
        actions.append(apply, button("Keep editing", "closeProfileDraftReview")); review.append(actions); panel.append(review);
    }
    host.append(panel);
}
module.exports = {renderProfileDraft};
