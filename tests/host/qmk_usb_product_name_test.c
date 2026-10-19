// The keyboard name as the USB product string (D-F25): what USB serves before
// it starts, and when the half on USB restarts it to serve a new name.
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/compat/qmk_usb_product_name.h"
#include "users/noah/lib/profile/runtime/effective_settings_runtime.h"
#include "users/noah/lib/profile/storage/profile_store_runtime.h"

// The world the module reads, set by each test.
static bool                         master, usb_active, settled, boot_found;
static noah_profile_keyboard_name_t boot_name, live_name;
static uint32_t                     publication, now;
static unsigned                     restarts, boot_reads;

bool is_keyboard_master(void) {
    return master;
}
uint32_t timer_read32(void) {
    return now;
}
uint32_t timer_elapsed32(uint32_t last) {
    return now - last;
}
bool noah_qmk_usb_product_name_test_usb_active(void) {
    return usb_active;
}
void noah_qmk_usb_product_name_test_restart(void) {
    restarts++;
}
bool noah_profile_store_runtime_boot_keyboard_name(noah_profile_keyboard_name_t *name) {
    boot_reads++;
    *name = boot_found ? boot_name : (noah_profile_keyboard_name_t){0};
    return boot_found;
}
bool noah_profile_store_runtime_usb_restart_safe(void) {
    return settled;
}
void noah_effective_settings_keyboard_name(noah_profile_keyboard_name_t *name) {
    *name = live_name;
}
uint32_t noah_effective_settings_publication(void) {
    return publication;
}

static noah_profile_keyboard_name_t name_of(const char *text) {
    noah_profile_keyboard_name_t name = {.length = (uint8_t)strlen(text)};
    memcpy(name.bytes, text, name.length);
    return name;
}
static void publish(const char *text) {
    live_name = name_of(text);
    publication++;
}
// Asserts USB serves `text` (NULL: QMK's built-in name).
static void serves(const char *text) {
    const uint8_t *descriptor = noah_qmk_usb_product_name_descriptor();
    if (!text) {
        assert(!descriptor);
        return;
    }
    size_t length = strlen(text);
    assert(descriptor && descriptor[0] == 2u + 2u * length && descriptor[1] == 3u);
    for (size_t i = 0; i < length; i++)
        assert(descriptor[2 + 2 * i] == (uint8_t)text[i] && descriptor[3 + 2 * i] == 0u);
}
// A booted, enumerated master half running `text`, with nothing in flight.
static void boot(const char *committed, bool is_master) {
    boot_found = committed != NULL;
    boot_name  = boot_found ? name_of(committed) : (noah_profile_keyboard_name_t){0};
    master = is_master, usb_active = true, settled = true;
    restarts = 0;
    noah_qmk_usb_product_name_pre_init();
    publish(committed ? committed : "");
}

static void test_encoding(void) {
    uint8_t                      descriptor[NOAH_QMK_USB_PRODUCT_NAME_DESCRIPTOR_MAX];
    noah_profile_keyboard_name_t name = name_of("A\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80");
    // A, é, €, then a surrogate pair for U+1F600.
    assert(noah_qmk_usb_product_name_encode(&name, descriptor, sizeof(descriptor)) == 12u);
    static const uint8_t expected[] = {12, 3, 'A', 0, 0xe9, 0, 0xac, 0x20, 0x3d, 0xd8, 0x00, 0xde};
    assert(!memcmp(descriptor, expected, sizeof(expected)));
    // Thirty-two ASCII bytes fill the largest descriptor exactly.
    name = name_of("0123456789abcdef0123456789abcdef");
    assert(noah_qmk_usb_product_name_encode(&name, descriptor, sizeof(descriptor)) == NOAH_QMK_USB_PRODUCT_NAME_DESCRIPTOR_MAX);
    assert(noah_qmk_usb_product_name_encode(&name, descriptor, sizeof(descriptor) - 1u) == 0u);
    // Empty and malformed names serve the built-in name.
    name = name_of("");
    assert(noah_qmk_usb_product_name_encode(&name, descriptor, sizeof(descriptor)) == 0u);
    name = name_of("ab\xc3");
    assert(noah_qmk_usb_product_name_encode(&name, descriptor, sizeof(descriptor)) == 0u);
    name = name_of("\x80");
    assert(noah_qmk_usb_product_name_encode(&name, descriptor, sizeof(descriptor)) == 0u);
    assert(noah_qmk_usb_product_name_encode(NULL, descriptor, sizeof(descriptor)) == 0u);
}

static void test_boot_serves_the_committed_name(void) {
    boot("Desk", true);
    assert(boot_reads == 1u);
    serves("Desk");
    // The activated profile agrees: nothing to restart.
    noah_qmk_usb_product_name_task();
    assert(restarts == 0u);
    serves("Desk");
    // Nothing usable committed: QMK's built-in name.
    boot(NULL, true);
    serves(NULL);
    noah_qmk_usb_product_name_task();
    assert(restarts == 0u);
}

static void test_rename_restarts_once_when_safe(void) {
    boot("Desk", true);
    publish("Travel");
    // Every condition holds it back until all are clear.
    settled = false;
    noah_qmk_usb_product_name_task();
    assert(restarts == 0u);
    settled    = true;
    usb_active = false; // suspended or not enumerated
    noah_qmk_usb_product_name_task();
    assert(restarts == 0u);
    usb_active = true;
    // A client still talking keeps it waiting for a quiet second.
    now = 5000u;
    noah_qmk_usb_product_name_note_host_frame();
    now += NOAH_QMK_USB_PRODUCT_NAME_QUIET_MS - 1u;
    noah_qmk_usb_product_name_task();
    assert(restarts == 0u);
    serves("Desk");
    now++;
    noah_qmk_usb_product_name_task();
    assert(restarts == 1u);
    serves("Travel");
    // Once enumerated under the name, it stays.
    noah_qmk_usb_product_name_task();
    noah_qmk_usb_product_name_task();
    assert(restarts == 1u);
    // Clearing the name restarts back to the built-in one.
    publish("");
    noah_qmk_usb_product_name_task();
    assert(restarts == 2u);
    serves(NULL);
    // A republished profile with the same name changes nothing.
    publish("");
    noah_qmk_usb_product_name_task();
    assert(restarts == 2u);
}

static void test_half_without_usb_never_restarts(void) {
    boot("Desk", false);
    // Before its role is known, the half keeps the name in memory only.
    serves("Desk");
    publish("Travel");
    for (int i = 0; i < 5; i++)
        noah_qmk_usb_product_name_task();
    assert(restarts == 0u);
    serves("Desk");
}

static void test_boot_activates_a_different_profile(void) {
    // Boot read one name, the owner settled on another (the peer's newer
    // profile, or a stored one it refused): restart to the running name.
    boot("Desk", true);
    publish("");
    settled = false; // still activating
    noah_qmk_usb_product_name_task();
    assert(restarts == 0u);
    settled = true;
    noah_qmk_usb_product_name_task();
    assert(restarts == 1u);
    serves(NULL);
}

static void test_unencodable_name_settles(void) {
    boot(NULL, true);
    live_name = name_of("ab\xc3");
    publication++;
    noah_qmk_usb_product_name_task();
    assert(restarts == 1u);
    serves(NULL);
    noah_qmk_usb_product_name_task();
    assert(restarts == 1u);
}

int main(void) {
    test_encoding();
    test_boot_serves_the_committed_name();
    test_rename_restarts_once_when_safe();
    test_half_without_usb_never_restarts();
    test_boot_activates_a_different_profile();
    test_unencodable_name_settles();
    puts("qmk USB product name tests passed");
    return 0;
}
