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

// A behaviour group scrolls independently of the page. Measure the actual
// first five rows because a long reach note can make one row taller.
export function limitBehaviourGroups(root) {
    root.querySelectorAll(".beh-rows.scroll").forEach((list) => {
        const rows = [...list.children].slice(0, 5);
        list.style.maxHeight = `${rows.reduce((height, row) => height + row.getBoundingClientRect().height, 0)}px`;
    });
}

export function captureNestedScroll(root, previousScreen, nextScreen) {
    if (previousScreen !== nextScreen) return new Map();
    return new Map([...root.querySelectorAll("[data-scroll-key]")]
        .map((list) => [list.dataset.scrollKey, list.scrollTop]));
}

export function restoreNestedScroll(root, positions) {
    root.querySelectorAll("[data-scroll-key]").forEach((list) => {
        list.scrollTop = positions.get(list.dataset.scrollKey) ?? 0;
        if (!list.hasAttribute("data-reveal-selected")) return;
        const selected = list.querySelector(".rowitem.on");
        if (!selected) return;
        const bounds = list.getBoundingClientRect();
        const row = selected.getBoundingClientRect();
        if (row.top < bounds.top) list.scrollTop += row.top - bounds.top;
        else if (row.bottom > bounds.bottom) list.scrollTop += row.bottom - bounds.bottom;
    });
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
