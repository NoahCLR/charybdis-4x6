#pragma once

#ifdef NOAH_ATOMIC_TEST_HOOKS
unsigned int noah_atomic_test_enter(void);
void noah_atomic_test_leave(void);
#    define ATOMIC_BLOCK_RESTORESTATE for (unsigned int noah_atomic_once = noah_atomic_test_enter(); noah_atomic_once != 0u; noah_atomic_once = 0u, noah_atomic_test_leave())
#else
#define ATOMIC_BLOCK_RESTORESTATE for (unsigned int noah_atomic_once = 1u; noah_atomic_once != 0u; noah_atomic_once = 0u)

#endif
