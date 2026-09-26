// The Checks section at the top of the review (view/checks.mjs): what the
// draft does to the layers you can reach, and the traps Apply asks about.

import {esc} from "../lib/dom.mjs";
import {checkGroups, checkTags} from "../view/checks.mjs";

// One check: its level and title, what it means, the steps into it, and the
// way out. `show(check)` returns the Show button for one that has a place.
function checkItem(check, show) {
    const path = check.path?.length
        ? `<ol class="ck-path">${check.path.map((step) => `<li>${esc(step)}</li>`).join("")}</ol>` : "";
    return `<div class="ck-item ${esc(check.level)}${check.status === "fixed" ? " fixed" : ""}">
        <div class="ck-h"><span class="ck-level">${check.level === "trap" ? '<i class="dot err"></i>' : ""}${checkTags(check).map(esc).join(" · ")}</span>
            <span class="ck-t">${esc(check.title)}</span><span class="ck-act">${show(check)}</span></div>
        ${check.status === "fixed" ? "" : `<div class="ck-b"><p>${esc(check.detail)}</p>${path}<p class="ck-fix">${esc(check.fix)}</p></div>`}
    </div>`;
}

// The section, or nothing when the walk found nothing.
export function checksSection(checks, show) {
    const groups = checkGroups(checks);
    if (!groups.length) return "";
    return `<section class="rv-sect ck">
        <div class="sect-h"><h4>Checks</h4><span class="note">what the layers let you reach, and whether you can always get back to Base</span></div>
        ${groups.map((group) => `<details class="ck-group" ${group.open ? "open" : ""}>
            <summary>${esc(group.title)} <span class="tag">${group.items.length}</span></summary>
            ${group.items.map((check) => checkItem(check, show)).join("")}</details>`).join("")}
    </section>`;
}
