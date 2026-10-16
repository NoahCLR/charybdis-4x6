// Negative control only: link unchanged production consumers to unpaced bus
// admission, reproducing the original service-pressure failure deterministically.
#include "users/noah/lib/compat/qmk_split_service.h"
void noah_split_service_begin(void) {}
bool noah_split_service_ready(uint8_t client) {(void)client;return true;}
void noah_split_service_cancel(uint8_t client) {(void)client;}
void noah_split_service_complete(uint8_t client,uint32_t before,bool success,bool urgent) {(void)client;(void)before;(void)success;(void)urgent;}
