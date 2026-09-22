// Reading a macro payload for display.
//
// The keyboard stores a macro as text: literal characters, {KC_X} to tap,
// {+KC_X} to press, {-KC_X} to release and {120} to wait. This parses that for
// the preview only — the host re-parses and validates it on save, so anything
// this cannot read is reported rather than guessed at.

export function parseMacro(payload) {
    const steps = [];
    let text = "";
    const flush = () => { if (text) steps.push({kind: "text", text}); text = ""; };
    const source = String(payload ?? "");

    for (let index = 0; index < source.length;) {
        const character = source[index++];
        if ((character === "{" || character === "}") && source[index] === character) { text += character; index++; continue; }
        if (character === "}") return fail(steps, "Unexpected } — use }} for a literal closing brace.");
        if (character !== "{") { text += character; continue; }
        flush();
        const end = source.indexOf("}", index);
        if (end < 0) return fail(steps, "A macro command is missing its }.");
        const command = source.slice(index, end).trim();
        index = end + 1;
        if (/^\d+$/.test(command)) { steps.push({kind: "delay", delay: Number(command)}); continue; }
        if (!command) return fail(steps, "An empty {} is not a macro command.");
        const kind = command[0] === "+" ? "press" : command[0] === "-" ? "release" : "tap";
        const keys = (kind === "tap" ? command : command.slice(1)).split(",").map((name) => name.trim()).filter(Boolean);
        if (!keys.length) return fail(steps, "A macro command needs at least one key.");
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
