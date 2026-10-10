#include <stdbool.h>

// These key-runtime unit harnesses do not link or run the macro output engine.
// The actual protected path is covered by the gesture integration harness.
bool macro_payload_engine_protected(void) {return false;}
