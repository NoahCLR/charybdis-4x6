// Reads macOS keyboard layouts from the system's own layout data and prints
// the raw host-layout source used by tools/host_layouts/build.py.
//
//   swift tools/host_layouts/extract_macos.swift > tools/host_layouts/sources/macos.json
//
// Developer tool, macOS only. Keys are named by the HID usage QMK sends and
// mapped to the virtual key macOS assigns to an ANSI-classified keyboard; the
// ISO swap of KC_GRV and KC_NUBS is applied at lookup time
// (docs/architecture/host-layouts-v1.md).

import Carbon
import Foundation

let layouts: [(slug: String, inputSource: String, optionLayer: Bool)] = [
    ("macos-abc", "com.apple.keylayout.ABC", true),
    ("macos-dutch", "com.apple.keylayout.Dutch", true),
    // Unicode Hex Input reserves Option for hexadecimal entry.
    ("macos-unicode-hex-input", "com.apple.keylayout.UnicodeHexInput", false),
    ("macos-british", "com.apple.keylayout.British", true),
    ("macos-german", "com.apple.keylayout.German", true),
    ("macos-french", "com.apple.keylayout.French", true),
]

// QMK basic keycode name -> macOS virtual key code on an ANSI keyboard.
let keys: [(name: String, vk: UInt16)] = [
    ("KC_A", 0x00), ("KC_B", 0x0B), ("KC_C", 0x08), ("KC_D", 0x02), ("KC_E", 0x0E), ("KC_F", 0x03),
    ("KC_G", 0x05), ("KC_H", 0x04), ("KC_I", 0x22), ("KC_J", 0x26), ("KC_K", 0x28), ("KC_L", 0x25),
    ("KC_M", 0x2E), ("KC_N", 0x2D), ("KC_O", 0x1F), ("KC_P", 0x23), ("KC_Q", 0x0C), ("KC_R", 0x0F),
    ("KC_S", 0x01), ("KC_T", 0x11), ("KC_U", 0x20), ("KC_V", 0x09), ("KC_W", 0x0D), ("KC_X", 0x07),
    ("KC_Y", 0x10), ("KC_Z", 0x06),
    ("KC_1", 0x12), ("KC_2", 0x13), ("KC_3", 0x14), ("KC_4", 0x15), ("KC_5", 0x17),
    ("KC_6", 0x16), ("KC_7", 0x1A), ("KC_8", 0x1C), ("KC_9", 0x19), ("KC_0", 0x1D),
    ("KC_SPC", 0x31), ("KC_MINS", 0x1B), ("KC_EQL", 0x18), ("KC_LBRC", 0x21), ("KC_RBRC", 0x1E),
    ("KC_BSLS", 0x2A), ("KC_NUHS", 0x2A), ("KC_SCLN", 0x29), ("KC_QUOT", 0x27), ("KC_GRV", 0x32),
    ("KC_COMM", 0x2B), ("KC_DOT", 0x2F), ("KC_SLSH", 0x2C), ("KC_NUBS", 0x0A),
]

let layers: [(mods: UInt32, wrap: (String) -> String)] = [
    (0, { $0 }),
    (UInt32(shiftKey), { "S(\($0))" }),
    (UInt32(optionKey), { "ALGR(\($0))" }),
    (UInt32(shiftKey | optionKey), { "S(ALGR(\($0)))" }),
]

let ansiKeyboardType: UInt32 = 40
let spaceVK: UInt16 = 0x31

func layoutData(_ id: String) -> Data {
    let props = [kTISPropertyInputSourceID as String: id] as CFDictionary
    guard let list = TISCreateInputSourceList(props, true)?.takeRetainedValue() as? [TISInputSource],
          let source = list.first,
          let pointer = TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData) else {
        FileHandle.standardError.write("missing input source \(id)\n".data(using: .utf8)!)
        exit(1)
    }
    return Unmanaged<CFData>.fromOpaque(pointer).takeUnretainedValue() as Data
}

// One key press; returns the text produced and leaves `state` holding any
// pending dead key.
func press(_ data: Data, _ vk: UInt16, _ mods: UInt32, _ state: inout UInt32) -> String {
    var length = 0
    var chars = [UniChar](repeating: 0, count: 8)
    data.withUnsafeBytes { raw in
        _ = UCKeyTranslate(raw.baseAddress!.assumingMemoryBound(to: UCKeyboardLayout.self), vk,
                           UInt16(kUCKeyActionDown), (mods >> 8) & 0xFF, ansiKeyboardType, 0,
                           &state, chars.count, &length, &chars)
    }
    return String(utf16CodeUnits: chars, count: length)
}

// A typed result is kept only when it is one printable scalar.
func printable(_ text: String) -> String? {
    let scalars = Array(text.unicodeScalars)
    guard scalars.count == 1, let value = scalars.first?.value else { return nil }
    if value < 0x20 || (value >= 0x7F && value < 0xA0) { return nil }
    return text
}

var output: [String: Any] = [:]
for layout in layouts {
    let data = layoutData(layout.inputSource)
    var keyTable: [String: [Any]] = [:]
    var deadTable: [String: Any] = [:]
    for key in keys {
        var row: [Any] = []
        for (index, layer) in layers.enumerated() {
            if index >= 2 && !layout.optionLayer { row.append(NSNull()); continue }
            var state: UInt32 = 0
            let text = press(data, key.vk, layer.mods, &state)
            if !(text.isEmpty && state != 0) {
                row.append(printable(text).map { $0 as Any } ?? NSNull())
                continue
            }
            let stroke = layer.wrap(key.name)
            row.append(["dead": stroke])
            var spaceState = state
            let spacing = printable(press(data, spaceVK, 0, &spaceState))
            var compose: [String: String] = [:]
            for base in keys where base.name != "KC_SPC" {
                for (baseIndex, baseLayer) in layers.prefix(2).enumerated() {
                    var composeState = state
                    var plainState: UInt32 = 0
                    let composed = press(data, base.vk, baseLayer.mods, &composeState)
                    let plain = press(data, base.vk, baseLayer.mods, &plainState)
                    if let result = printable(composed), result != plain {
                        compose[layers[baseIndex].wrap(base.name)] = result
                    }
                }
            }
            deadTable[stroke] = ["space": spacing.map { $0 as Any } ?? NSNull(), "compose": compose]
        }
        keyTable[key.name] = row
    }
    output[layout.slug] = ["source": layout.inputSource, "keys": keyTable, "dead": deadTable]
}

let document: [String: Any] = [
    "generator": "tools/host_layouts/extract_macos.swift",
    "system": "macOS \(ProcessInfo.processInfo.operatingSystemVersionString)",
    "layouts": output,
]
let json = try JSONSerialization.data(withJSONObject: document, options: [.prettyPrinted, .sortedKeys, .withoutEscapingSlashes])
FileHandle.standardOutput.write(json)
FileHandle.standardOutput.write("\n".data(using: .utf8)!)
