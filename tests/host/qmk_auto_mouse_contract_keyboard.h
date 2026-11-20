#pragma once
// QMK_KEYBOARD_H for userspace that run_qmk_contract_checks.sh links with
// QMK's own auto-mouse: QMK's headers that the keyboard's own header brings
// in. The runner gives the matrix and the keymap's layer count.
#include "action.h"
#include "keyboard.h"
#include "quantum_keycodes.h"
