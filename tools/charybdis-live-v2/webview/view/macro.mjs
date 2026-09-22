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
