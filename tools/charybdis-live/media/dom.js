// Minimal DOM helpers shared by the webview modules. Deliberately tiny: the
// live app builds nodes directly rather than assembling HTML strings, so
// device-supplied values can never be interpreted as markup.

export function element(tag, attributes = {}, children) {
    const node = document.createElement(tag);
    for (const [name, value] of Object.entries(attributes)) {
        node.setAttribute(name, value);
    }
    if (typeof children === "string" || typeof children === "number") {
        node.textContent = String(children);
    } else if (Array.isArray(children)) {
        node.append(...children.filter(Boolean));
    } else if (children) {
        node.append(children);
    }
    return node;
}

export function panel(title, body) {
    return element("section", {class: "panel"}, [element("h2", {}, title), ...body]);
}

// Renders a label/value table. Values are shown verbatim, including zeros and
// falsy values, because "0" is meaningful device state and blanking it would
// misreport the keyboard.
export function facts(rows) {
    const table = element("dl", {class: "facts"});
    for (const [label, value] of rows) {
        if (value === undefined || value === null || value === "") {
            continue;
        }
        table.append(element("dt", {}, label), element("dd", {}, String(value)));
    }
    return table;
}

export function hex(value, width = 8) {
    if (typeof value !== "number" || !Number.isFinite(value)) {
        return undefined;
    }
    return `0x${(value >>> 0).toString(16).toUpperCase().padStart(width, "0")}`;
}
