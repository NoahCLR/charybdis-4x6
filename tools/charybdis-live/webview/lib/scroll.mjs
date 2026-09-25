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
