// Reuse the established storage fake; both reconciler instances and the QMK
// adapter are production code. Storage writes are not needed for equal-profile
// metadata/PENDING/main-consumption/ACK traffic on the shared bus.
#define main service_profile_original_tests
#define transaction_register_rpc service_profile_unused_register
#define transaction_rpc_exec service_profile_unused_exec
#include "qmk_profile_split_transport_test.c"
#undef main
#undef transaction_register_rpc
#undef transaction_rpc_exec
static noah_profile_split_reconciler_t service_profile_local,service_profile_peer;
static noah_profile_peer_store_backend_t service_profile_stores[2];
void service_profile_init(void) {
    init_reconciler(&service_profile_local,&service_profile_stores[0]);
    init_reconciler(&service_profile_peer,&service_profile_stores[1]);
    service_profile_local.config.exchange=noah_qmk_profile_split_transport_exchange;
    CHECK(noah_qmk_profile_split_transport_init(&service_profile_local));
}
bool service_profile_step(bool master,uint32_t now) {
    return noah_profile_split_reconciler_scan(master?&service_profile_local:&service_profile_peer,master,now);
}
bool service_profile_receive(uint8_t n,const void *p,uint8_t r,void *out) {
    return noah_profile_split_reconciler_receive(&service_profile_peer,p,n,out,r);
}

bool noah_profile_store_runtime_matrix_scan_step(void) {return service_profile_step(true,timer_read32());}
