import {element, panel} from "../dom.js";

export function renderDiagnostics(snapshot) {
    if (!snapshot.diagnostics?.length) {
        return element("div", {hidden: "hidden"});
    }
    const list = element("ul", {class: "diagnostics"});
    for (const entry of snapshot.diagnostics.slice(-20)) {
        list.append(element("li", {}, String(entry)));
    }
    return panel("Diagnostics", [list]);
}
