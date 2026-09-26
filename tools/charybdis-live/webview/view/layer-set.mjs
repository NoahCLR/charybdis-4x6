// Which layers the board shows on at once.
//
// The board normally shows one layer as it is stored. ⌘-clicking more layer
// tabs previews them on together, the way the keyboard runs them: the highest
// one wins, its transparent keys are answered by the highest layer below that
// is also on, and base is always on. `top` is that highest layer — the tab the
// board opens from and the layer every edit is stored on — and `on` lists the
// other layers previewed with it, ascending, never above `top`. Both are
// positions in the layer stack, as `state.layer` is.
//
// Base is under every preview, so it only needs listing to preview a layer
// over base alone: `on` is then `[0]`. With other layers on, base stays on.

export const EMPTY = Object.freeze([]);

// The layers still worth previewing from a remembered set, in a stack of
// `count` layers.
export const layersOn = (top, on, count) =>
    (on || EMPTY).filter((index) => index >= 0 && index < top && index < count);

// Toggles one layer into or out of the set. The highest layer left becomes the
// top; with none left, base is shown on its own. Base toggles only while no
// other layer is on under the top: once one is, base is on regardless.
export function toggleLayer(top, on, index) {
    const held = [...(on || EMPTY)];
    if (index <= 0) {
        if (top <= 0 || held.some((at) => at > 0)) return {top, on: held};
        return {top, on: held.includes(0) ? [] : [0]};
    }
    const set = new Set([top, ...held]);
    if (set.has(index)) set.delete(index);
    else set.add(index);
    const base = held.includes(0);
    set.delete(0);
    const sorted = [...set].sort((a, b) => a - b);
    const next = sorted.pop() ?? 0;
    return {top: next, on: base && next > 0 ? [0, ...sorted] : sorted};
}
