// Colour, exactly as the keyboard means it.
//
// The device stores HSV in QMK's 0–255 space and the model hands it over as
// strings. Everything that turns those numbers into pixels goes through here,
// so one rule decides what a colour looks like and when a legend is black.

export const hsv = (colour) => colour
    ? [Number(colour.h) || 0, Number(colour.s) || 0, Number(colour.v) || 0]
    : [0, 0, 0];

export const isOff = (colour) => hsv(colour)[2] === 0;

export function rgb(colour) {
    const [h255, s255, v255] = hsv(colour);
    const h = h255 * 360 / 255, s = s255 / 255, v = v255 / 255;
    const c = v * s, x = c * (1 - Math.abs(((h / 60) % 2) - 1)), m = v - c;
    const [r, g, b] = h < 60 ? [c, x, 0] : h < 120 ? [x, c, 0] : h < 180 ? [0, c, x]
        : h < 240 ? [0, x, c] : h < 300 ? [x, 0, c] : [c, 0, x];
    return [r, g, b].map((channel) => Math.round((channel + m) * 255));
}

export const css = (colour) => `rgb(${rgb(colour).join(", ")})`;

export const label = (colour) => {
    const [h, s, v] = hsv(colour);
    return `HSV(${h}, ${s}, ${v})`;
};

// Relative luminance decides black or white legends: the keyboard picks the
// colour, the label still has to be readable on it.
export function idealText(colour) {
    const [r, g, b] = rgb(colour).map((channel) => {
        const c = channel / 255;
        return c <= 0.03928 ? c / 12.92 : ((c + 0.055) / 1.055) ** 2.4;
    });
    return 0.2126 * r + 0.7152 * g + 0.0722 * b > 0.32 ? "rgba(0,0,0,.82)" : "rgba(255,255,255,.94)";
}
