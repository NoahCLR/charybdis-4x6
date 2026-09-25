// Keeping the keyboard's place across a redraw.
//
// Every render replaces the whole app, so the focused element is thrown away
// with it. Before the redraw the focused control is remembered by the stable
// attribute that names it — the same data-* hooks the screens wire their
// handlers to — and afterwards the control that carries that name again gets
// focus back, with its caret where it was.

const NAMES = [
    "id", "data-key", "data-trackball", "data-cell", "data-term", "data-macro", "data-channel", "data-row",
    "data-edit", "data-editmacro", "data-editpd", "data-pick", "data-lt", "data-mod", "data-screen", "data-tab",
    "data-ltab", "data-slot", "data-b", "data-helper", "data-repeat", "data-output", "data-target", "data-owner",
    "data-source", "data-mode", "data-anchor", "data-act",
];

export function captureFocus(root) {
    const active = root.ownerDocument?.activeElement;
    if (!active || active === root.ownerDocument.body || !root.contains(active)) return null;
    const name = NAMES.find((attribute) => active.hasAttribute(attribute));
    if (!name) return null;
    const value = active.getAttribute(name);
    const selector = name === "id" ? `#${CSS.escape(value)}` : `${active.tagName.toLowerCase()}[${name}="${CSS.escape(value)}"]`;
    const index = [...root.querySelectorAll(selector)].indexOf(active);
    const caret = typeof active.selectionStart === "number" ? [active.selectionStart, active.selectionEnd] : null;
    return {selector, index, caret};
}

export function restoreFocus(root, captured) {
    if (!captured) return;
    const matches = root.querySelectorAll(captured.selector);
    const target = matches[captured.index] || matches[0];
    if (!target || typeof target.focus !== "function") return;
    target.focus({preventScroll: true});
    if (captured.caret && typeof target.setSelectionRange === "function") {
        try { target.setSelectionRange(...captured.caret); } catch { /* a number input has no caret */ }
    }
}

const FOCUSABLE = 'button:not([disabled]), input:not([disabled]), select:not([disabled]), textarea:not([disabled]), [tabindex="0"]';

// A dialog that opens takes focus: whatever was focused behind it is not
// something the keyboard should keep acting on.
export function focusDialog(root) {
    const dialogs = root.querySelectorAll(".scrim");
    const dialog = dialogs[dialogs.length - 1];
    if (!dialog || dialog.contains(root.ownerDocument.activeElement)) return;
    dialog.querySelector(`input:not([disabled]), ${FOCUSABLE}`)?.focus({preventScroll: true});
}

// Tab stays inside an open dialog: the last scrim on screen is the one in
// front, and focus wraps around its own controls rather than leaving it.
export function trapTab(root, event) {
    if (event.key !== "Tab") return false;
    const dialogs = root.querySelectorAll(".scrim");
    const dialog = dialogs[dialogs.length - 1];
    if (!dialog) return false;
    const items = [...dialog.querySelectorAll(FOCUSABLE)].filter((node) => !node.closest("[hidden]"));
    if (!items.length) return false;
    const at = items.indexOf(root.ownerDocument.activeElement);
    const next = event.shiftKey ? (at <= 0 ? items.length - 1 : at - 1) : (at < 0 || at === items.length - 1 ? 0 : at + 1);
    event.preventDefault();
    items[next].focus();
    return true;
}

// SVG keys are buttons to assistive tech but not to the browser, so Enter and
// Space do nothing on their own. They click; Enter on the board's selected key
// opens its picker, as a double-click does.
export function activateOnKey(event) {
    if (event.key !== "Enter" && event.key !== " ") return false;
    const target = event.target;
    if (!(target instanceof Element) || target.getAttribute("role") !== "button" || target.tagName === "BUTTON") return false;
    event.preventDefault();
    const openInstead = event.key === "Enter" && target.matches("[data-key].sel");
    target.dispatchEvent(new MouseEvent(openInstead ? "dblclick" : "click", {bubbles: true}));
    return true;
}
