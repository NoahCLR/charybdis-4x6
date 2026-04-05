#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/state/keyboard_mod_ownership.h"
#include "users/noah/lib/state/keyboard_mod_state.h"

static uint8_t fake_mods;
static uint8_t fake_weak_mods;
static uint8_t fake_oneshot_mods;
static uint8_t fake_oneshot_locked_mods;

static void test_fail(const char *expr, const char *file, int line) {
    fprintf(stderr, "test failed: %s (%s:%d)\n", expr, file, line);
    exit(1);
}

#define CHECK(expr)            \
    do {                       \
        if (!(expr)) {         \
            test_fail(#expr, __FILE__, __LINE__); \
        }                      \
    } while (0)

uint8_t get_mods(void) {
    return fake_mods;
}

uint8_t get_weak_mods(void) {
    return fake_weak_mods;
}

uint8_t get_oneshot_mods(void) {
    return fake_oneshot_mods;
}

uint8_t get_oneshot_locked_mods(void) {
    return fake_oneshot_locked_mods;
}

void set_mods(uint8_t mods) {
    fake_mods = mods;
}

void set_weak_mods(uint8_t mods) {
    fake_weak_mods = mods;
}

void set_oneshot_mods(uint8_t mods) {
    fake_oneshot_mods = mods;
}

void set_oneshot_locked_mods(uint8_t mods) {
    fake_oneshot_locked_mods = mods;
}

void clear_mods(void) {
    fake_mods = 0;
}

void clear_weak_mods(void) {
    fake_weak_mods = 0;
}

void clear_oneshot_mods(void) {
    fake_oneshot_mods = 0;
}

void clear_oneshot_locked_mods(void) {
    fake_oneshot_locked_mods = 0;
}

void add_mods(uint8_t mods) {
    fake_mods |= mods;
}

void del_mods(uint8_t mods) {
    fake_mods &= (uint8_t)~mods;
}

void send_keyboard_report(void) {
}

void register_mods(uint8_t mods);
void unregister_mods(uint8_t mods);

static void test_suspend_allows_nested_same_mod_registration(void) {
    uint8_t              gui_mask = MOD_BIT(KC_LEFT_GUI);
    keyboard_mod_state_t saved;

    keyboard_mod_ownership_register(KC_LEFT_GUI);
    CHECK(get_mods() == gui_mask);

    saved = keyboard_mod_state_suspend();
    CHECK(saved.real == gui_mask);
    CHECK(get_mods() == 0);

    register_mods(gui_mask);
    CHECK(get_mods() == gui_mask);

    unregister_mods(gui_mask);
    CHECK(get_mods() == gui_mask);

    keyboard_mod_state_apply(saved);
    CHECK(get_mods() == gui_mask);

    keyboard_mod_ownership_unregister(KC_LEFT_GUI);
    CHECK(get_mods() == 0);
}

int main(void) {
    test_suspend_allows_nested_same_mod_registration();
    puts("keyboard_mod_ownership host tests passed");
    return 0;
}
