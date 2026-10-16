// Production durable VIA consumer, existing storage + peer protocol fake.
#define main service_via_original_tests
#define is_keyboard_master service_via_unused_master
#define timer_read32 service_via_unused_clock
#define transaction_register_rpc service_via_unused_register
#define transaction_rpc_exec service_via_receive
#include "qmk_via_split_sync_test.c"
#undef main
#undef is_keyboard_master
#undef timer_read32
#undef transaction_register_rpc
#undef transaction_rpc_exec
void service_via_init(void) {test_reset();rpc_mode=TEST_RPC_PEER_DIVERGED;test_init_ready();}
void service_via_step(void) {(void)noah_qmk_via_split_sync_matrix_scan_step();}
void service_via_dirty(void) {noah_qmk_via_split_sync_note_mutation(NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR);local_keymap[0]++;}
#include "users/noah/lib/compat/qmk_via_split_mirror.h"
#include "via.h"
extern bool is_keyboard_master(void);
void service_via_mirror_write(void) {
    noah_qmk_via_split_sync_note_mutation(NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR);
    local_keymap[0]++;local_keymap[1]++;
    uint8_t frame[]={id_dynamic_keymap_set_buffer,0,0,2,local_keymap[0],local_keymap[1]};
    noah_qmk_via_split_mirror_command(frame,sizeof frame);
    // Caller-owned report is immediately reusable; queued bytes stay exact.
    memset(frame,0,sizeof frame);
}
void service_mirror_peer_changed(void) {}
void dynamic_keymap_set_buffer(uint16_t offset,uint16_t n,uint8_t *data) {
    CHECK(offset+n<=sizeof peer_keymap);memcpy((is_keyboard_master()?local_keymap:peer_keymap)+offset,data,n);
}
void dynamic_keymap_set_keycode(uint8_t layer,uint8_t row,uint8_t col,uint16_t code) {
    (void)layer;(void)row;uint8_t bytes[]={(uint8_t)(code>>8),(uint8_t)code};dynamic_keymap_set_buffer(col,2,bytes);
}
void dynamic_keymap_reset(void) {memset(is_keyboard_master()?local_keymap:peer_keymap,0,3);}
void dynamic_keymap_macro_reset(void) {memset(is_keyboard_master()?local_macro:peer_macro,0,2);}
void dynamic_keymap_macro_set_buffer(uint16_t offset,uint16_t n,uint8_t *data) {
    CHECK(offset+n<=sizeof peer_macro);memcpy((is_keyboard_master()?local_macro:peer_macro)+offset,data,n);
}
void service_via_assert_converged(void) {
    CHECK(!memcmp(local_keymap,peer_keymap,sizeof local_keymap));
    CHECK(!memcmp(local_config,peer_config,sizeof local_config));
    CHECK(!memcmp(local_macro,peer_macro,sizeof local_macro));
    noah_qmk_via_split_sync_debug_snapshot_t debug=noah_qmk_via_split_sync_debug_snapshot();
    CHECK(debug.last_peer_ack_digest==local_digest());
}

uint16_t dynamic_keymap_macro_get_buffer_size(void) {return sizeof local_macro;}
void raw_hid_send(uint8_t *data,uint8_t n) {(void)data;(void)n;}
void service_via_late_mirror_begin(void) {
    service_via_mirror_write();
    noah_qmk_via_split_sync_note_mutation(NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR);
    dynamic_keymap_reset();local_keymap[0]=0x91;local_keymap[2]=0x93;
    dynamic_keymap_macro_reset();local_macro[1]=0xa7;
    // The old exact frame stays queued while later same/range/reset/macro
    // changes converge through the actual authoritative full reconciler.
}
bool service_via_freeze(void) {
    noah_qmk_via_sync_frame_t request={.kind=NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_BEGIN,.generation=100,.digest=0x12345678};
    CHECK(noah_qmk_via_logical_submit(99,&request));
    CHECK(!noah_qmk_via_logical_mirror_allowed());return true;
}
void service_via_unfreeze(void) {
    CHECK(noah_qmk_via_logical_cancel(99,100,0x12345678));
}
bool service_via_can_mirror(void) {return noah_qmk_via_logical_mirror_allowed();}
void service_via_boot_fence(void) {CHECK(noah_qmk_via_logical_boot_recover(100,0x12345678));}
