#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "action_util.h"
#include "keycode_config.h"

keymap_config_t          keymap_config;
static unsigned          sent;
static bool              isolate;
static bool              hide;
static report_keyboard_t received;
#ifdef NKRO_ENABLE
static report_nkro_t received_nkro;
bool                 host_can_send_nkro(void) {
    return true;
}
void host_nkro_send(report_nkro_t *report) {
    received_nkro = *report;
    sent++;
}
void nkro_report_keys_filter_user(report_nkro_t *report) {
    if (isolate || hide) memset(report->bits, 0, sizeof(report->bits));
    if (isolate) report->bits[0] = 1u << 5;
}
#endif
void host_keyboard_send(report_keyboard_t *report) {
    received = *report;
    sent++;
}
void keyboard_report_keys_filter_user(report_keyboard_t *report) {
    if (isolate || hide) memset(report->keys, 0, sizeof(report->keys));
    if (isolate) report->keys[0] = 5;
}
bool keyboard_report_mods_override_user(uint8_t *mods) {
    if (!isolate) return false;
    *mods = 4;
    return true;
}
int main(void) {
    keymap_config.oneshot_enable = true;
    keyboard_report->keys[0]     = 4;
    set_mods(1);
    set_oneshot_mods(2);
    hide = true;
    send_keyboard_report();
    assert(received.keys[0] == 0 && received.mods == 3);
    assert(get_oneshot_mods() == 2); // Hidden live keys must not consume it.
    assert(keyboard_report->keys[0] == 4 && get_mods() == 1);
    isolate = true;
    hide    = false;
    send_keyboard_report();
    assert(received.keys[0] == 5 && received.mods == 4);
    assert(keyboard_report->keys[0] == 4 && get_oneshot_mods() == 2);
    unsigned before = sent;
    send_keyboard_report();
#ifndef PROTOCOL_VUSB
    assert(sent == before);
#else
    assert(sent == before + 1);
#endif
    isolate = false;
    send_keyboard_report();
    assert(received.keys[0] == 4 && received.mods == 3);
    assert(get_oneshot_mods() == 0);
#ifdef NKRO_ENABLE
    keymap_config.nkro   = true;
    nkro_report->bits[0] = 1u << 4;
    set_oneshot_mods(2);
    hide = true;
    send_keyboard_report();
    assert(received_nkro.bits[0] == 0 && get_oneshot_mods() == 2);
    isolate = true;
    hide    = false;
    send_keyboard_report();
    assert(received_nkro.bits[0] == (1u << 5) && received_nkro.mods == 4);
    assert(nkro_report->bits[0] == (1u << 4) && get_oneshot_mods() == 2);
    before = sent;
    send_keyboard_report();
    assert(sent == before);
    isolate = false;
    send_keyboard_report();
    assert(received_nkro.bits[0] == (1u << 4) && received_nkro.mods == 3);
    assert(get_oneshot_mods() == 0);
#endif
    puts("QMK outgoing key filtering preserves live state and uses visible keys for one-shot consumption");
}
