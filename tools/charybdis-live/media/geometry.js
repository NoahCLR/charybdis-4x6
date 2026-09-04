// Charybdis 4x6 physical key geometry.
//
// Forked from Profile Studio's renderer (D-L06): the shape of the board is the
// same whoever is editing it, and it is fiddly to re-derive. Only the geometry
// came across — no model, no C-expression handling.
//
// Layout indexes 0..47 are the finger matrix, 12 per row, left half in columns
// 0-5 and right half in 6-11. Indexes 48..55 are thumb keys, which sit at
// measured positions and angles rather than on the grid.

export const KEYBOARD_GEOMETRY = Object.freeze({
    width: 1120,
    height: 620,
    keyWidth: 58,
    keyHeight: 58,
    radius: 7,
    yOffset: 54,
    rowStep: 64,
    viewBox: {x: 0, y: 86, width: 1120, height: 510},
    leftX: [36, 102, 168, 234, 300, 366],
    leftTopY: [118, 118, 78, 54, 78, 78],
    rightX: [698, 764, 830, 896, 962, 1028],
    rightTopY: [78, 78, 54, 78, 118, 118],
    thumbs: {
        48: {x: 328, y: 354, angle: 0},
        49: {x: 396, y: 350, angle: 10},
        50: {x: 468, y: 358, angle: 17},
        51: {x: 576, y: 358, angle: -17},
        52: {x: 648, y: 350, angle: -10},
        53: {x: 398, y: 432, angle: 9},
        54: {x: 468, y: 446, angle: 15},
        55: {x: 578, y: 432, angle: -15},
    },
});

export function keyVisual(layoutIndex) {
    const geometry = KEYBOARD_GEOMETRY;
    const thumb = geometry.thumbs[layoutIndex];
    if (thumb) {
        return {x: thumb.x, y: thumb.y + geometry.yOffset, angle: thumb.angle};
    }

    const row = Math.floor(layoutIndex / 12);
    const column = layoutIndex % 12;
    if (column < 6) {
        return {
            x: geometry.leftX[column],
            y: geometry.leftTopY[column] + row * geometry.rowStep + geometry.yOffset,
            angle: 0,
        };
    }
    const rightColumn = column - 6;
    return {
        x: geometry.rightX[rightColumn],
        y: geometry.rightTopY[rightColumn] + row * geometry.rowStep + geometry.yOffset,
        angle: 0,
    };
}
