#pragma once

#define ATOMIC_BLOCK_RESTORESTATE for (unsigned int noah_atomic_once = 1u; noah_atomic_once != 0u; noah_atomic_once = 0u)
