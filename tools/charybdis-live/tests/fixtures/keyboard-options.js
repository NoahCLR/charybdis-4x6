"use strict";
function options() {
    return {effects: [{id: 1, name: "SOLID_COLOR"}, {id: 2, name: "BREATHING"}, {id: 3, name: "CYCLE_ALL"}],
        keymapMasks: Array.from({length: 13}, (_, i) => 1 << i), supportedKeymapOptions: 0x0fff, ledFlags: 5};
}
function wire() {
    const value = options(), bytes = Buffer.alloc(26 + value.effects.length * 64), metadata = Buffer.from([1,25,0,0,value.effects.length,13,value.supportedKeymapOptions & 255,value.supportedKeymapOptions >>> 8,value.ledFlags]);
    metadata.writeUInt16LE(bytes.length, 2);
    value.keymapMasks.forEach((mask, i) => bytes.writeUInt16LE(mask, i * 2));
    value.effects.forEach((effect, i) => bytes.write(effect.name, 26 + i * 64));
    return {metadata, bytes};
}
module.exports = {options, wire};
