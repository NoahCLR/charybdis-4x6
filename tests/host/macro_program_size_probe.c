// Reads one VIA macro per line as hex and prints the program length the
// firmware's decoder compiles it to, summed over the windows it plays in, or
// -1 when it refuses the macro. The app's macroProgramBytes() is checked
// against this output.
#include <stdio.h>
#include <string.h>

#include "qmk_stub.h"
#include "users/noah/lib/macro/macro_payload.h"

static uint8_t buffer[8192];

static bool read_byte(uint16_t offset, uint8_t *byte, void *context) {
    (void)context;
    *byte = buffer[offset];
    return true;
}

int main(void) {
    static char line[16384 + 2];
    while (fgets(line, sizeof(line), stdin)) {
        uint16_t length = 0;
        for (size_t i = 0; line[i] && line[i + 1] && line[i] != '\n'; i += 2) {
            unsigned value = 0;
            if (sscanf(line + i, "%2x", &value) != 1 || length >= sizeof(buffer) - 1u) return 2;
            buffer[length++] = (uint8_t)value;
        }
        buffer[length++] = 0;
        macro_payload_ir_t            ir     = {0};
        macro_payload_stream_cursor_t cursor = {0};
        long                          total  = 0;
        do {
            if (!macro_payload_decode_qmk_window(&ir, &cursor, length, read_byte, NULL)) {
                total = -1;
                break;
            }
            total += ir.length;
        } while (ir.more);
        printf("%ld\n", total);
    }
    return 0;
}
