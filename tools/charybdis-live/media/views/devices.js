import {element, panel} from "../dom.js";

export function renderDevices(snapshot, onConnect) {
    if (!snapshot.scanned) {
        return panel("Devices", [element("p", {class: "muted"}, "Not scanned yet.")]);
    }
    if (!snapshot.devices.length) {
        return panel("Devices", [
            element("p", {class: "muted"}, "No Charybdis Raw HID interface found."),
            element("p", {class: "muted"}, "Connect the keyboard and confirm it runs firmware built with VIA_ENABLE=yes."),
        ]);
    }

    const list = element("ul", {class: "devices"});
    for (const device of snapshot.devices) {
        const label = [device.manufacturer, device.product].filter(Boolean).join(" ") || "Charybdis";
        const selected = device.id === snapshot.selectedDeviceId;
        const row = element("li", {class: selected ? "devices__item is-selected" : "devices__item"});
        row.append(
            element("div", {class: "devices__label"}, [
                element("strong", {}, label),
                element("code", {}, device.id),
                device.serialNumber ? element("span", {class: "muted"}, `serial ${device.serialNumber}`) : null,
            ])
        );
        if (!selected) {
            const connect = element("button", {}, "Connect");
            connect.disabled = Boolean(snapshot.busy);
            connect.addEventListener("click", () => onConnect(device.id));
            row.append(connect);
        } else {
            row.append(element("span", {class: "chip chip--ok"}, "Selected"));
        }
        list.append(row);
    }
    return panel("Devices", [list]);
}
