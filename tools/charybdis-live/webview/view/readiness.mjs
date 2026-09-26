// The Configure screens show a board only after a complete editable read.
// The host keeps the gate closed across the small gaps between HID requests.

const PHASES = {
    enumerating: "Looking for a keyboard",
    connecting: "Connecting to the keyboard",
    refreshing: "Checking both halves",
    "reading layout": "Reading keys and layers",
    "reading profile": "Reading lighting and behaviours",
    "reading complete profile": "Reading the complete profile",
};

export const configureReady = (model) => model?.load?.state === "ready";

export function screenAvailable(model, screen) {
    if (!model || model.load?.state === "loading") return false;
    if (screen === "device") return Boolean(model.device?.connected);
    if (screen === "profile") return Boolean(model.portable?.available);
    return configureReady(model);
}

export function readScreen(model, screen = "keys") {
    if (!model || model.load?.state === "loading") {
        const phase = model?.load?.phase || "enumerating";
        const progress = model?.load?.progress;
        const count = Number.isInteger(progress?.done) && Number.isInteger(progress?.total) && progress.total > 0
            ? ` · ${progress.done} of ${progress.total}` : "";
        return {state: "loading", title: "Reading your keyboard", detail: `${PHASES[phase] || "Finishing the read"}${count}`};
    }
    if (screen === "profile" && !screenAvailable(model, screen)) return {state: "unavailable", title: "Backups unavailable",
        detail: "Connect a keyboard with complete-profile firmware, then read it to manage backups."};
    if (screen === "device" && !screenAvailable(model, screen)) return {state: "unavailable", title: "Device status unavailable",
        detail: "Read the keyboard to see its connection and diagnostics."};
    if (configureReady(model)) return null;
    const device = model.device || {};
    if (!device.connected) return {state: "unavailable", title: "Connect your keyboard", detail: device.health?.error || "Connect a Charybdis, then read it to begin editing."};
    if (model.portable?.legacy) return {state: "unavailable", title: "This keyboard is read-only", detail: "This firmware has five layers. You can back up its profile, then install the eight-layer firmware to edit it."};
    return {state: "unavailable", title: "Keyboard not ready for edits",
        detail: device.health?.error || "A complete profile could not be read. Read the keyboard again to retry."};
}
