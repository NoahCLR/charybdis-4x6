// Which layers the board shows on at once.
//
// The board normally shows one layer as it is stored. ⌘-clicking more layer
// tabs previews them on together, the way the keyboard runs them: the highest
// one wins, its transparent keys are answered by the highest layer below that
// is also on, and base is always on. `top` is that highest layer — the tab the
// board opens from and the layer every edit is stored on — and `on` lists the
// other layers previewed with it, ascending, never base and never above `top`.
// Both are positions in the layer stack, as `state.layer` is.

export const EMPTY = Object.freeze([]);

// The layers still worth previewing from a remembered set, in a stack of
// `count` layers.
export const layersOn = (top, on, count) =>
    (on || EMPTY).filter((index) => index > 0 && index < top && index < count);

// Toggles one layer into or out of the set. Base is always on, so it is not
// toggled. The highest layer left becomes the top; with none left, base is.
export function toggleLayer(top, on, index) {
    if (index <= 0) return {top, on: [...(on || EMPTY)]};
    const set = new Set([top, ...(on || EMPTY)]);
    if (set.has(index)) set.delete(index);
    else set.add(index);
    set.delete(0);
    const sorted = [...set].sort((a, b) => a - b);
    return {top: sorted.pop() ?? 0, on: sorted};
}
