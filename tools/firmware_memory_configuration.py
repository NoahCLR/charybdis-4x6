#!/usr/bin/env python3
"""Prove the scoped retained-lighting artifact configuration with its compiler.

The caller runs in the QMK build directory. Reuse the recorded flags, including
forced configuration headers, rather than inferring geometry from a filename.
A nonmatching configuration conservatively keeps its previous RAM policy.
"""
import argparse
from pathlib import Path
import shlex
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("flags", type=Path)
    parser.add_argument("--compiler", default="arm-none-eabi-gcc")
    args = parser.parse_args()
    source = r'''
#include QMK_KEYBOARD_H
#include "process_combo.h"
_Static_assert(__GNUC__ == 15 && __GNUC_MINOR__ == 2 && __GNUC_PATCHLEVEL__ == 0, "recorded compiler");
_Static_assert(MATRIX_ROWS * MATRIX_COLS == 60 && COMBO_BUFFER_LENGTH == 4, "recorded geometry");
_Static_assert(NOAH_LIGHTING_EVENT_CAPTURE_ENABLE == 1, "enabled retained consumer");
'''
    result = subprocess.run(
        [args.compiler, *shlex.split(args.flags.read_text()), "-x", "c", "-fsyntax-only", "-"],
        input=source, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    return result.returncode != 0


if __name__ == "__main__":
    raise SystemExit(main())
