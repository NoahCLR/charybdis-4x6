// Small DOM helpers. Device text only ever reaches the page through
// textContent or an escaped interpolation, so a keyboard cannot post markup.

export const esc = (value) => String(value ?? "").replace(/[&<>"']/g, (character) =>
    ({"&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"}[character]));

export function el(markup) {
    const template = document.createElement("template");
    template.innerHTML = String(markup).trim();
    return template.content.firstElementChild;
}
