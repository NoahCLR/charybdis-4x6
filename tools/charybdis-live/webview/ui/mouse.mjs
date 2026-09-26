// Mouse: how the trackball moves the pointer — its speed, sniping, and the
// layer movement turns on by itself.
//
// These are settings the keyboard stores with its global policy, so they are
// drawn by the Settings cards and saved a section at a time, exactly as
// Settings saves its own. The keyboard's model says which sections are Mouse
// ones; this screen adds none of its own. Pointing modes, which re-read the
// trackball as scrolling or keys, keep their own screen.

import {el, esc} from "../lib/dom.mjs";
import {getModel, render, state, canEdit as canEditArea} from "../store.mjs";
import {topbar, unavailable} from "./shell.mjs";
import {sectionCard, sectionsIn} from "./settings.mjs";

export function screenMouse() {
    const model = getModel();
    const sections = sectionsIn(model, "Mouse");
    const canEdit = canEditArea("settings");
    const main = el(`<div class="main">${topbar(
        "Mouse",
        "How the trackball moves the pointer: its speed, sniping for fine control, and the mouse layer that movement turns on by itself.",
        `<button class="btn" data-act="pointing" data-tip="Modes that re-read the trackball as scrolling or as keys while their key is held.">Pointing modes</button>`,
    )}</div>`);
    main.querySelector('[data-act="pointing"]').addEventListener("click", () => { state.screen = "pointing"; render(); });

    const content = el(`<div class="content"><div class="pad" style="max-width:980px"></div></div>`);
    const pad = content.firstElementChild;
    if (!sections.length) {
        pad.appendChild(el(`<div class="screen-stub"><h3>No mouse settings read</h3>
            <p class="note">${esc(unavailable(model) || "Choose Read keyboard to load this keyboard's pointer settings.")}</p></div>`));
    } else if (!canEdit) {
        pad.appendChild(el(`<div class="unavailable" style="margin-bottom:14px">${esc(unavailable(model)
            || "This firmware reports its pointer settings but cannot save them. Flash the complete-profile pair to edit them here.")}</div>`));
    }
    for (const section of sections) if (section.fields.length) pad.appendChild(sectionCard(model, section, section.fields, canEdit));
    main.appendChild(content);
    return main;
}
