// Reading a macro payload for display.
//
// The keyboard stores a macro as text: literal characters, {KC_X} to tap,
// {+KC_X} to press, {-KC_X} to release and {120} to wait. This parses it for
// the preview and checks it by the same rules the host enforces on save
// (core/schema/macro-payload.js), so the preview never calls valid what the
// host would refuse. `options.keys` is the host's list of key names a macro
// may send; without it, key names are not checked. A key still held at the end
// is not an error here — the step builder passes through that state — and
// unreleased() reports it.

const MAX_CHARACTERS = 32768, MAX_DELAY = 65535, MAX_KEYS = 16;
const printable = (character) => {
    const code = character.charCodeAt(0);
    return code === 9 || code === 10 || (code >= 32 && code <= 126);
};

export function parseMacro(payload, options = {}) {
    const steps = [];
    const held = new Set();
    let text = "";
    const flush = () => { if (text) steps.push({kind: "text", text}); text = ""; };
    const source = String(payload ?? "");
    const allowed = options.keys?.length ? new Set(options.keys) : null;
    if (source.length > MAX_CHARACTERS) return fail(steps, "A macro is at most 32,768 characters.");

    for (let index = 0; index < source.length;) {
        const character = source[index++];
        if ((character === "{" || character === "}") && source[index] === character) { text += character; index++; continue; }
        if (character === "}") return fail(steps, "Unexpected } — use }} for a literal closing brace.");
        if (character !== "{") {
            if (!printable(character)) return fail(steps, "Macros support ASCII text, tabs and newlines.");
            text += character; continue;
        }
        flush();
        const end = source.indexOf("}", index);
        if (end < 0) return fail(steps, "A macro command is missing its }.");
        const command = source.slice(index, end).trim();
        index = end + 1;
        if (/^\d+$/.test(command)) {
            if (Number(command) > MAX_DELAY) return fail(steps, "A macro delay must be between 0 and 65,535 milliseconds.");
            steps.push({kind: "delay", delay: Number(command)}); continue;
        }
        if (!command) return fail(steps, "An empty {} is not a macro command.");
        const kind = command[0] === "+" ? "press" : command[0] === "-" ? "release" : "tap";
        const keys = (kind === "tap" ? command : command.slice(1)).split(",").map((name) => name.trim()).filter(Boolean);
        if (!keys.length) return fail(steps, "A macro command needs at least one key.");
        if (kind !== "tap" && keys.length !== 1) return fail(steps, "A press or release names one key; use a tap for a chord.");
        if (keys.length > MAX_KEYS || new Set(keys).size !== keys.length) return fail(steps, "A chord is up to 16 distinct keys.");
        const unknown = allowed && keys.find((key) => !allowed.has(key));
        if (unknown) return fail(steps, `${unknown} is not a key a macro can send.`);
        if (kind === "release") {
            if (!held.delete(keys[0])) return fail(steps, `${keys[0]} is released but was never pressed.`);
        } else {
            if (keys.some((key) => held.has(key))) return fail(steps, "A macro presses a key that is already held.");
            if (held.size + keys.length > MAX_KEYS) return fail(steps, "A macro holds at most 16 keys at once.");
            if (kind === "press") held.add(keys[0]);
        }
        steps.push({kind, keys});
    }
    flush();
    return {steps, error: ""};
}

const fail = (steps, error) => ({steps, error});

export const describeStep = (step) => step.kind === "text" ? step.text
    : step.kind === "delay" ? `${step.delay} ms`
    : step.keys.join(" + ");

export function serializeMacroStep(step) {
    if (step.kind === "text") return String(step.text ?? "").replaceAll("{", "{{").replaceAll("}", "}}");
    if (step.kind === "delay") return `{${Math.max(0, Number(step.delay) || 0)}}`;
    const keys = (step.keys || []).join(",");
    if (!keys) return "";
    return step.kind === "press" ? `{+${keys}}` : step.kind === "release" ? `{-${keys}}` : `{${keys}}`;
}

export const serializeMacro = (steps) => steps.map(serializeMacroStep).join("");

// What a slot shows on its cell. Every macro on a real keyboard tends to open
// with the same modifier press, so the head of the payload makes them all look
// identical; what tells them apart is the key they actually send.
export function macroPeek(payload, label = (name) => name) {
    const {steps} = parseMacro(payload);
    const text = steps.filter((step) => step.kind === "text").map((step) => step.text).join("").trim();
    if (text) return text;
    const taps = steps.filter((step) => step.kind === "tap").flatMap((step) => step.keys);
    if (taps.length) return taps.map(label).join(" ");
    const held = steps.filter((step) => step.kind === "press").flatMap((step) => step.keys);
    if (held.length) return held.map(label).join(" + ");
    return steps[0] ? describeStep(steps[0]) : "";
}

// Held keys that are never released leave the keyboard holding them, so the
// preview says so rather than letting it through quietly.
export function unreleased(steps) {
    const held = [];
    for (const step of steps) {
        if (step.kind === "press") held.push(...step.keys);
        if (step.kind === "release") for (const key of step.keys) {
            const at = held.indexOf(key);
            if (at >= 0) held.splice(at, 1);
        }
    }
    return held;
}

// Whether a slot answers a search. A macro is found by the name it was given,
// and by the slot it sits in as the screens label it — "M3", "Macro 3" or its
// keycode — so an unnamed macro can still be found. Case is ignored.
export function macroMatches(slot, query) {
    const needle = String(query ?? "").trim().toLowerCase();
    if (!needle) return true;
    const number = String(slot?.keycode ?? "").split("_").at(-1);
    return [slot?.name, `M${number}`, `Macro ${number}`, slot?.keycode]
        .some((text) => String(text ?? "").toLowerCase().includes(needle));
}
