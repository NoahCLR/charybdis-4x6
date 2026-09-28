#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "users/noah/lib/compat/qmk_split_diagnostics.h"
static uint32_t now;
uint32_t        test_clock(void) {
    return now;
}
bool is_keyboard_master(void) {
    return true;
}
void            split_transaction_diagnostic(uint8_t, uint8_t, uint8_t, uint32_t, bool);
static uint32_t get32(uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void command(uint8_t *data, uint8_t op, uint8_t page) {
    memset(data, 0, 32);
    data[0] = op;
    data[2] = 10;
    data[3] = 1;
    data[4] = page;
    assert(noah_split_diagnostics_command(data, 32));
}
int main(void) {
    uint8_t data[32];
    command(data, 7, 1);
    assert(data[5] == 3);
    now = UINT32_MAX - 1000;
    command(data, 8, 0);
    assert(data[5] == 0);
    split_transaction_diagnostic(1, 12, 0, 700, true);
    split_transaction_diagnostic(1, 12, 0, 5000, false);
    command(data, 7, 2);
    assert(data[5] == 3);
    now += 10000000u;
    command(data, 7, 0);
    assert(data[9] == 0 && data[10] == 1);
    command(data, 7, 2);
    assert(data[5] == 0 && data[8] == 1);
    assert(get32(data + 9) == 2 && get32(data + 13) == 1 && get32(data + 17) == 28);
    assert(get32(data + 21) == 5700 && get32(data + 25) == 5000);
    split_transaction_diagnostic(1, 12, 0, 9999, true);
    command(data, 7, 2);
    assert(get32(data + 9) == 2);
    command(data, 7, 99);
    assert(data[5] == 2);
    command(data, 8, 1);
    assert(data[5] == 1);
    command(data, 8, 0);
    assert(data[5] == 0);
    now += 10000000u;
    command(data, 7, 2);
    assert(get32(data + 9) == 0);
    memset(data, 0, 32);
    data[0] = 8;
    data[2] = 10;
    data[3] = 1;
    data[8] = 1;
    assert(noah_split_diagnostics_command(data, 32) && data[5] == 1);
    puts("split diagnostics tests passed");
}
