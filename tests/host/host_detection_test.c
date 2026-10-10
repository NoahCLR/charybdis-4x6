#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define OS_DETECTION_ENABLE
// Deliberately different from portable IDs: the adapter must translate.
enum { OS_UNSURE, OS_LINUX, OS_WINDOWS, OS_MACOS, OS_IOS };
static uint8_t os;
static uint8_t detected_host_os(void) { return os; }
#include "users/noah/lib/compat/qmk_host.h"
int main(void) {
    const uint8_t expected[] = {0, 3, 2, 1, 0};
    for (os = 0; os <= OS_IOS; os++) {
        assert(noah_host_detected() == expected[os]);
        assert(noah_host_effective(0x100, noah_host_detected()) == expected[os]);
        for (uint8_t override = 1; override <= 3; override++)
            assert(noah_host_effective(override | 0x100, noah_host_detected()) == override);
    }
    puts("host detection translation and override tests passed");
}
