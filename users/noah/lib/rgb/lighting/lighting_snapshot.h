#pragma once
#include "lighting_working.h"
#include "lighting_freshness.h"
#include "lighting_event_sender.h"

enum { NOAH_LIGHTING_SNAPSHOT_PAYLOAD = 55u, NOAH_LIGHTING_SNAPSHOT_CHUNK = 51u, NOAH_LIGHTING_SNAPSHOT_REQUEST = 64u, NOAH_LIGHTING_SNAPSHOT_REPLY = 14u };
typedef struct {
    noah_lighting_event_link_t link;
    noah_lighting_working_t   *working;
    noah_lighting_facts_t     *facts, *published_facts;
    noah_lighting_freshness_t  clock;
    uint32_t                   capture, before, after, remote, last_time, sent_revision;
    uint16_t                   offset, order_offset;
    uint8_t                    phase;
    uint8_t                    fact_dirty[NOAH_LIGHTING_FACT_DIRTY_BYTES ? NOAH_LIGHTING_FACT_DIRTY_BYTES : 1u];
    bool                       staging, clock_ready, dirty, facts_only, have_baseline, delta_facts, delta_history;
} noah_lighting_snapshot_t;
// Begin leaves the endpoint closed. Bind both owned fact pointers after RPC
// registration; binding publishes the completely initialized role.
void noah_lighting_snapshot_begin(noah_lighting_snapshot_t *, noah_lighting_working_t *, bool master, const uint8_t profile[22]);
void noah_lighting_snapshot_bind_facts(noah_lighting_snapshot_t *, noah_lighting_facts_t *staging, noah_lighting_facts_t *published);
void noah_lighting_snapshot_receive(noah_lighting_snapshot_t *, const uint8_t *, uint8_t length, uint32_t now, uint8_t reply[14]);
bool noah_lighting_snapshot_main(noah_lighting_snapshot_t *, uint32_t now);
// Master prepares immutable bytes; each actual RPC attempt supplies fresh causal
// before/after samples with the returned callback sample. Failed attempts keep
// bytes identical and never advance snapshot/current state.
uint8_t noah_lighting_snapshot_request(noah_lighting_snapshot_t *, uint32_t now);
bool    noah_lighting_snapshot_reply(noah_lighting_snapshot_t *, const uint8_t reply[14], uint32_t before, uint32_t after);

// Main-only, once per lost acknowledged baseline. Unknown status also requires
// full resync; repeated NOT_READY during handshake cannot reset its progress.
void noah_lighting_snapshot_peer_readiness(noah_lighting_snapshot_t *, uint8_t status);
