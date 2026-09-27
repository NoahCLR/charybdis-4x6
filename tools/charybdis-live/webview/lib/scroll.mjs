// Keep a person's place when the current screen is rebuilt. Every edit comes
// back through the host model and the webview deliberately redraws from that
// model, but replacing `.content` must not make an ordinary click feel like
// navigation. Moving to another screen still starts at its top.

export function captureContentScroll(root, previousScreen, nextScreen) {
    if (previousScreen !== nextScreen) return null;
    const content = root.querySelector(".content");
    if (!content) return null;
    return {top: content.scrollTop, left: content.scrollLeft};
}

export function restoreContentScroll(root, scroll) {
    if (!scroll) return;
    const content = root.querySelector(".content");
    if (!content) return;
    content.scrollTop = scroll.top;
    content.scrollLeft = scroll.left;
}

// The common Keys floor keeps ordinary tabs level. If one editor grows beyond
// it, carry that height into the next render before restoring the page scroll.
export function captureKeysBenchHeight(root, previousScreen, nextScreen, reset = false) {
    if (previousScreen !== "keys" || nextScreen !== "keys" || reset) return 0;
    return root.querySelector(".keys-pad .bench")?.getBoundingClientRect().height || 0;
}

export function restoreKeysBenchHeight(root, height) {
    if (!height) return;
    const bench = root.querySelector(".keys-pad .bench");
    if (bench) bench.style.minHeight = `${height}px`;
}

// A behaviour selected from elsewhere may sit below the visible part of the
// one content scroller. Reveal it there, without creating another scrollbar.
export function revealSelectedContentRow(root, margin = 12) {
    const content = root.querySelector(".content");
    const selected = content?.querySelector("[data-reveal-selected] .rowitem.on");
    if (!selected) return;
    const bounds = content.getBoundingClientRect();
    const row = selected.getBoundingClientRect();
    if (row.top < bounds.top + margin) content.scrollTop += row.top - bounds.top - margin;
    else if (row.bottom > bounds.bottom - margin) content.scrollTop += row.bottom - bounds.bottom + margin;
}

// A deliberate jump from an editor to something higher in the same scroller.
// Measure both rectangles after the redraw; offsetTop may belong to a different
// positioned ancestor and cannot tell us where the target is in `.content`.
export function scrollContentTo(root, selector, margin = 20) {
    const content = root.querySelector(".content");
    const target = content?.querySelector(selector);
    if (!target) return;
    const top = content.scrollTop + target.getBoundingClientRect().top - content.getBoundingClientRect().top - margin;
    content.scrollTo({top, left: content.scrollLeft, behavior: "smooth"});
}
