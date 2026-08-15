// ────────────────────────────────────────────────────────────────────────────
// Runtime Publication Generation
// ────────────────────────────────────────────────────────────────────────────
//
// Single-writer/single-reader publication seqlock for runtime state that one
// execution context publishes and another renders.
//
// QMK's ChibiOS split transport dispatches slave RPC callbacks from the
// dedicated `SlaveThread` in `platforms/chibios/drivers/serial_protocol.c`,
// which `soft_serial_target_init()` starts at `HIGHPRIO`. A publication can
// therefore preempt the main loop, but the main loop can never preempt a
// publication, and the transport's own `split_shared_memory` mutex is held
// across blocking serial I/O, so main-context readers must never take it.
//
// Two publication shapes share the same generation counter:
//
// - In-place publication (`begin`/`end`), for groups too large to double
//   buffer. The generation stays odd while the group is in flight, so readers
//   skip the copy entirely rather than committing a half-stored group, and
//   settles on a fresh even value once the whole group is stored.
// - Slot publication (`slot`/`publish`), for groups small enough to keep two
//   copies. The writer fills the slot the current generation does not select
//   and then advances the generation, so a reader always copies a slot the
//   writer is not touching and never sees an in-flight state at all.
//
// Readers of either shape re-check the generation around their copy and retry
// a bounded `NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS` times. A reader that
// cannot capture a settled generation keeps its previous coherent copy instead
// of retrying forever, so no reader blocks and no publication is ever delayed
// by rendering work.
//
// The reachable failure mode for in-place publication is an in-flight
// publication, and the pre-check guarantees no destination byte is written in
// that case. Exhausting the budget the other way needs one full publication to
// land inside every one of the retried copies, which the transport cannot
// produce: publications are milliseconds apart and a published group is a few
// dozen bytes.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifndef NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS
#    define NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS 4u
#endif

_Static_assert(NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS >= 2u, "publication readers need at least one retry after a preempted copy");

#define NOAH_RUNTIME_PUBLICATION_BARRIER() __asm__ __volatile__("" ::: "memory")

typedef volatile uint8_t noah_runtime_publication_generation_t;

static inline void noah_runtime_publication_begin(noah_runtime_publication_generation_t *generation) {
    *generation = (uint8_t)(*generation + 1u);
    NOAH_RUNTIME_PUBLICATION_BARRIER();
}

static inline void noah_runtime_publication_end(noah_runtime_publication_generation_t *generation) {
    NOAH_RUNTIME_PUBLICATION_BARRIER();
    *generation = (uint8_t)(*generation + 1u);
}

// Slot publication: hand the freshly filled slot over to readers.
static inline void noah_runtime_publication_publish(noah_runtime_publication_generation_t *generation) {
    noah_runtime_publication_end(generation);
}

static inline uint8_t noah_runtime_publication_observe(const noah_runtime_publication_generation_t *generation) {
    uint8_t observed = *generation;

    NOAH_RUNTIME_PUBLICATION_BARRIER();
    return observed;
}

static inline bool noah_runtime_publication_in_flight(uint8_t observed) {
    return (observed & 1u) != 0u;
}

static inline uint8_t noah_runtime_publication_slot(uint8_t generation) {
    return (uint8_t)(generation & 1u);
}

static inline bool noah_runtime_publication_settled(const noah_runtime_publication_generation_t *generation, uint8_t observed) {
    NOAH_RUNTIME_PUBLICATION_BARRIER();
    return *generation == observed;
}
