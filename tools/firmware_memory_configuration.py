#!/usr/bin/env python3
"""Prove the scoped retained-lighting artifact configuration with its compiler.

The caller runs in the QMK build directory. Reuse the recorded flags, including
forced configuration headers, rather than inferring geometry from a filename.
A nonmatching configuration conservatively keeps its previous RAM policy.
"""
import argparse
from pathlib import Path
import re
import shlex
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("flags", type=Path)
    parser.add_argument("--compiler", default="arm-none-eabi-gcc")
    parser.add_argument("--lighting-live-elf", type=Path,
                        help="also prove the complete linked Lighting Live amendment")
    parser.add_argument("--nm", default="arm-none-eabi-nm")
    args = parser.parse_args()
    source = r'''
#include QMK_KEYBOARD_H
#include "process_combo.h"
_Static_assert(__GNUC__ == 15 && __GNUC_MINOR__ == 2 && __GNUC_PATCHLEVEL__ == 0, "recorded compiler");
_Static_assert(MATRIX_ROWS * MATRIX_COLS == 60 && COMBO_BUFFER_LENGTH == 4, "recorded geometry");
_Static_assert(NOAH_LIGHTING_EVENT_CAPTURE_ENABLE == 1, "enabled retained consumer");
'''
    if args.lighting_live_elf:
        source += r'''
#include "lib/compat/qmk_lighting_live.h"
#include "transactions.h"
_Static_assert(NOAH_LIGHTING_LIVE_AVAILABLE == 1, "complete live prerequisites");
_Static_assert(sizeof(noah_lighting_facts_t) == 621 && NOAH_LIGHTING_FACT_SLICE_COUNT == 17, "reviewed fact modules");
#ifndef NOAH_LIVE_PROFILE_OWNER_ENABLE
#error live profile owner required
#endif
#ifndef VIA_ENABLE
#error VIA consumer required
#endif
#ifndef SPLIT_KEYBOARD
#error split consumer required
#endif
_Static_assert(NOAH_PD_PROFILE_ENABLE && NOAH_PORTABLE_PROFILE_ENABLE, "profile prerequisites");
_Static_assert(NOAH_LIGHTING_LIVE_ENABLE == 1 && RGB_MATRIX_ENABLE, "live RGB consumer");
_Static_assert(NOAH_LIGHTING_TARGET_KEYS_THAT || NOAH_LIGHTING_PAINT_WHAT_IT_DOES, "query consumer");
_Static_assert(RGB_MATRIX_LED_COUNT == 58 && RGB_LEFT_LED_COUNT == 29, "reviewed LED geometry");
_Static_assert(RPC_M2S_BUFFER_SIZE == 128 && RPC_S2M_BUFFER_SIZE == 32, "existing split buffers");
'''
    result = subprocess.run(
        [args.compiler, *shlex.split(args.flags.read_text()), "-x", "c", "-fsyntax-only", "-"],
        input=source, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    if result.returncode:
        return True
    if args.lighting_live_elf:
        # LTO/section GC retain these owned objects and consumers only when the
        # complete path is linked. Flags or an ELF filename are insufficient.
        symbols = subprocess.run([args.nm, "--defined-only", "--print-size",
                                  str(args.lighting_live_elf)], text=True,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if symbols.returncode:
            return True
        owned = {"live_sample": 664, "live_frames": 348, "live_control": 76,
                 "live_split": 112, "live_hid": 72}
        consumers = {"noah_lighting_live_task", "noah_lighting_live_handle",
                     "noah_lighting_live_close", "noah_lighting_live_colour",
                     "noah_rgb_runtime_live_release", "live_rpc",
                     "live_receive_main", "live_send_main", "live_capture",
                     "noah_lighting_keymap_refresh"}
        found_objects, found_consumers = {}, set()
        for line in symbols.stdout.splitlines():
            match = re.fullmatch(r"[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+(\w)\s+(\S+)", line)
            if not match:
                continue
            size, kind, name = match.groups()
            if name in owned and kind in "bB":
                found_objects[name] = int(size, 16)
            # GCC's LTO specialization suffixes keep the original symbol stem.
            stem = re.split(r"\.(?:constprop|isra|part|lto_priv)\.", name)[0]
            if kind in "tT" and int(size, 16) and stem in consumers:
                found_consumers.add(stem)
        return found_objects != owned or found_consumers != consumers
    return False


if __name__ == "__main__":
    raise SystemExit(main())
