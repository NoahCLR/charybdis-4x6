// Macro playback stub for runners that link action dispatch without the
// profile macro provider: no macro starts.
#include <stdbool.h>
#include <stdint.h>

#include "users/noah/lib/macro/profile_macro_provider.h"

bool noah_profile_macro_start(uint16_t action) {
    (void)action;
    return false;
}

bool noah_profile_macro_try_play(uint16_t action) {
    (void)action;
    return false;
}
