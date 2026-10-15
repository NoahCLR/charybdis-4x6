#include "lighting_snapshot.h"
#include <string.h>
#define BARRIER() __asm__ __volatile__("" ::: "memory")
enum { PING = 3u, BEGIN = 0u, CHUNK = 1u, COMMIT = 2u, ORDER = 4u };
static uint16_t get16(const uint8_t *p) {
    return (uint16_t)p[0] | (uint16_t)p[1] << 8u;
}
static void put16(uint8_t *p, uint16_t n) {
    p[0] = (uint8_t)n;
    p[1] = (uint8_t)(n >> 8u);
}
static void reset(void *context) {
    noah_lighting_snapshot_t *s = context;
    s->link.scene_readiness = NOAH_LIGHTING_SCENE_NOT_READY;
    BARRIER();
    noah_lighting_freshness_reset(&s->clock);
    noah_lighting_working_release_snapshot(s->working);
    s->staging = false;
    s->offset = s->order_offset = 0;
    s->phase   = PING;
    s->clock_ready = false;
    s->dirty = true;
    s->have_baseline = s->facts_only = s->delta_facts = s->delta_history = false;
    s->sent_revision = 0u;
    memset(s->fact_dirty, 0, sizeof s->fact_dirty);
    if (!s->link.master) {
        // Session reset clears transient histories. Program storage is unchanged;
        // no allocation or source read occurs on this path.
        s->working->revision++;
        s->working->render_dirty = true;
        memset(s->working->bytes + s->working->current * s->working->bank_bytes + 12u * s->working->rules, 0, s->working->bank_bytes - 12u * s->working->rules);
    }
}
void noah_lighting_snapshot_begin(noah_lighting_snapshot_t *s, noah_lighting_working_t *w, bool master, const uint8_t profile[22]) {
    s->link.scene_readiness = NOAH_LIGHTING_SCENE_NOT_READY;
    BARRIER();
    s->link.ready = false;
    BARRIER();
    s->working = w;
    s->clock_ready = false;
    s->dirty = true;
    s->have_baseline = s->facts_only = s->delta_facts = s->delta_history = false;
    s->sent_revision = 0u;
    memset(s->fact_dirty, 0, sizeof s->fact_dirty);
    s->offset = s->order_offset = 0;
    s->staging = false;
    s->phase   = PING;
    noah_lighting_freshness_reset(&s->clock);
    if (master)
        noah_lighting_snapshot_sender_prepare(&s->link, profile);
    else
        noah_lighting_snapshot_link_begin(&s->link, false, profile);
}
void noah_lighting_snapshot_bind_facts(noah_lighting_snapshot_t *s, noah_lighting_facts_t *staging, noah_lighting_facts_t *published) {
    s->facts           = staging;
    s->published_facts = published;
    noah_lighting_event_link_publish(&s->link);
}
void noah_lighting_snapshot_peer_readiness(noah_lighting_snapshot_t *s, uint8_t status) {
    if (!s->link.ready || !s->link.master || !s->have_baseline || status == NOAH_LIGHTING_SCENE_READY) return;
    reset(s);
    s->link.next_sequence++;
    s->link.active = s->link.waiting_confirm = s->link.mailbox_ready = false;
    s->link.packet.outgoing[0] = NOAH_LIGHTING_EVENT_HELLO | 0xa0u;
    memcpy(s->link.packet.outgoing + 1u, s->link.profile, 22u);
    noah_lighting_event_put_u32(s->link.packet.outgoing + 23u, s->link.next_sequence);
}
static void observe_clock(noah_lighting_snapshot_t *s, uint32_t now) {
    if (s->clock_ready && now - s->last_time > INT32_MAX) {
        // A detected discontinuity invalidates the entire causal exchange,
        // including a pinned old snapshot. Re-handshake before accepting data.
        reset(s);
        if (s->link.master) {
            s->link.next_sequence++;
            s->link.active = s->link.waiting_confirm = s->link.mailbox_ready = false;
            s->link.packet.outgoing[0] = NOAH_LIGHTING_EVENT_HELLO | 0xa0u;
            memcpy(s->link.packet.outgoing + 1u, s->link.profile, 22u);
            noah_lighting_event_put_u32(s->link.packet.outgoing + 23u, s->link.next_sequence);
        } else {
            s->link.handshake_sequence = s->link.next_sequence;
            s->link.reset_requested = true;
        }
    }
    s->last_time = now;
    s->clock_ready = true;
}
static uint16_t snapshot_bytes(const noah_lighting_snapshot_t *s) {
    return (s->facts_only ? 0u : s->working->bank_bytes) + (s->facts ? sizeof *s->facts : 0u);
}
void noah_lighting_snapshot_receive(noah_lighting_snapshot_t *s, const uint8_t *bytes, uint8_t length, uint32_t now, uint8_t reply[14]) {
    noah_lighting_event_receive(&s->link, bytes, length, reply);
    noah_lighting_event_put_u32(reply + 10u, now);
}
static bool consume(void *context, const uint8_t *p, uint32_t now) {
    noah_lighting_snapshot_t *s    = context;
    uint8_t                   used = noah_lighting_snapshot_payload_size(p);
    if (used > 55u) return false;
    for (uint8_t i = used; i < 55u; i++)
        if (p[i]) return false;
    if (p[0] == PING) return true;
    if (p[0] == BEGIN) {
        if ((p[19] != 0u && p[19] != 1u && p[19] != 3u && p[19] != 5u) || ((p[19] & 1u) && (!s->have_baseline || noah_lighting_event_get_u32(p + 20u) != s->sent_revision))) return false;
        s->delta_facts = (p[19] & 1u) != 0u;
        s->facts_only = (p[19] & 2u) != 0u;
        s->delta_history = (p[19] & 4u) != 0u;
        if (get16(p + 17u) != snapshot_bytes(s)) return false;
        s->sent_revision = noah_lighting_event_get_u32(p + 20u);
        // Main owns staging for this complete snapshot. Current is intact.
        s->staging = true;
        s->offset = s->order_offset = 0;
        if (s->delta_history) noah_lighting_working_stage_delta(s->working);
        else if (!s->facts_only) noah_lighting_working_stage_empty(s->working);
        if (s->facts) {
            if (s->delta_facts && s->published_facts) memcpy(s->facts, s->published_facts, sizeof *s->facts);
            else memset(s->facts, 0, sizeof *s->facts);
        }
        if (!s->facts_only) (void)noah_lighting_freshness_calibrate(&s->clock, noah_lighting_event_get_u32(p + 1u), noah_lighting_event_get_u32(p + 5u), noah_lighting_event_get_u32(p + 13u), noah_lighting_event_get_u32(p + 9u));
        return true;
    }
    if (!s->staging) return false;
    if (p[0] == ORDER) {
        uint16_t first = get16(p + 1u);
        if (!s->delta_history || first < s->order_offset || !noah_lighting_working_stage_orders(s->working, first, p + 4u, p[3])) return false;
        s->order_offset = first + p[3];
        return true;
    }
    if (p[0] == CHUNK) {
        uint16_t offset = get16(p + 1u);
        uint8_t  length = p[3];
        if (offset < s->offset || !length || length > NOAH_LIGHTING_SNAPSHOT_CHUNK || offset > snapshot_bytes(s) || length > snapshot_bytes(s) - offset || (!s->facts_only && offset < 12u * s->working->rules)) return false;
        uint16_t bank_bytes = s->facts_only ? 0u : s->working->bank_bytes;
        uint8_t bank_length = offset < bank_bytes ? (uint8_t)((bank_bytes - offset) < length ? bank_bytes - offset : length) : 0u;
        if (bank_length && !noah_lighting_working_stage(s->working, offset, p + 4u, bank_length)) return false;
        if (length > bank_length) memcpy((uint8_t *)s->facts + offset + bank_length - bank_bytes, p + 4u + bank_length, length - bank_length);
        s->offset = offset + length;
        return true;
    }
    if (p[0] != COMMIT || get16(p + 1u) != snapshot_bytes(s) || s->offset > snapshot_bytes(s)) return false;
    uint16_t lower, upper;
    if (!noah_lighting_freshness_age(&s->clock, now, &lower, &upper)) lower = upper = 65535u;
    if (!s->facts_only && !noah_lighting_working_commit(s->working, lower, upper)) return false;
    if (s->facts_only) { s->working->revision++; s->working->render_dirty = true; }
    s->have_baseline = true;
    s->sent_revision++;
    if (s->facts && s->published_facts) memcpy(s->published_facts, s->facts, sizeof *s->facts);
    s->working->last_time   = now;
    s->working->clock_ready = true;
    s->staging              = false;
    return true;
}
bool noah_lighting_snapshot_main(noah_lighting_snapshot_t *s, uint32_t now) {
    observe_clock(s, now);
    noah_lighting_working_advance(s->working, now);
    return noah_lighting_event_receive_main(&s->link, now, consume, reset, s);
}
uint8_t noah_lighting_snapshot_request(noah_lighting_snapshot_t *s, uint32_t now) {
    if (!s->link.ready || !s->link.master) return 0;
    observe_clock(s, now);
    if (s->link.active && !s->link.mailbox_ready) {
        uint8_t payload[55] = {0};
        if (!s->working->snapshot_ready) {
            if (!s->dirty && !s->working->dirty) return 0;
            s->dirty = false;
            noah_lighting_working_advance(s->working, now);
            s->facts_only = s->have_baseline && !s->working->dirty;
            s->delta_facts = s->have_baseline;
            s->delta_history = s->have_baseline && !s->facts_only;
            noah_lighting_working_snapshot(s->working);
            s->capture = now;
            s->phase   = s->facts_only ? BEGIN : PING;
            s->offset = s->order_offset = 0;
        }
        payload[0] = s->phase;
        if (s->phase == BEGIN) {
            noah_lighting_event_put_u32(payload + 1u, s->capture);
            noah_lighting_event_put_u32(payload + 5u, s->before);
            noah_lighting_event_put_u32(payload + 9u, s->after);
            noah_lighting_event_put_u32(payload + 13u, s->remote);
            put16(payload + 17u, snapshot_bytes(s));
            payload[19] = (uint8_t)(s->delta_facts | (s->facts_only ? 2u : 0u) | (s->delta_history ? 4u : 0u));
            noah_lighting_event_put_u32(payload + 20u, s->sent_revision);
        } else if (s->phase == CHUNK) {
            uint16_t offset = snapshot_bytes(s);
            uint8_t length = 0;
            uint16_t bank_bytes = s->facts_only ? 0u : s->working->bank_bytes;
            if ((s->facts_only || !noah_lighting_working_snapshot_span(s->working, s->offset, &offset, payload + 4u, &length, s->delta_history)) && s->facts) {
                uint16_t at = s->offset > bank_bytes ? s->offset - bank_bytes : 0u;
                const uint8_t *facts = (const uint8_t *)s->facts;
                if (s->delta_facts) {
                    // Each changed slice is sent in full, including changed-to-
                    // zero bytes. Unchanged slices come from the exact ACKed
                    // baseline; no extra fact frame is reserved on the source.
                    uint16_t ordinal = 0;
                    (void)ordinal;
#define NOAH_LIGHTING_FACT_DELTA_SPAN(name, bytes, synced, gated) \
                    if (!length && (s->fact_dirty[ordinal / 8u] & (1u << (ordinal % 8u))) && at < offsetof(noah_lighting_facts_t, name) + (bytes)) { \
                        if (at < offsetof(noah_lighting_facts_t, name)) at = offsetof(noah_lighting_facts_t, name); \
                        offset = bank_bytes + at; \
                        uint16_t left = offsetof(noah_lighting_facts_t, name) + (bytes) - at; \
                        length = left < 51u ? (uint8_t)left : 51u; \
                        memcpy(payload + 4u, facts + at, length); \
                    } \
                    ordinal++;
                    NOAH_LIGHTING_SLICE_ROWS(NOAH_LIGHTING_FACT_DELTA_SPAN)
#undef NOAH_LIGHTING_FACT_DELTA_SPAN
                } else {
                    // Complete reset: omitted fact bytes are canonical zero.
                    while (at < sizeof *s->facts && !facts[at]) at++;
                    if (at < sizeof *s->facts) {
                        offset = bank_bytes + at;
                        length = sizeof *s->facts - at < 51u ? (uint8_t)(sizeof *s->facts - at) : 51u;
                        while (length && !facts[at + length - 1u]) length--;
                        memcpy(payload + 4u, facts + at, length);
                    }
                }
            }
            if (!length && s->delta_history && noah_lighting_working_snapshot_orders(s->working, s->order_offset, &offset, payload + 4u, &length)) {
                payload[0] = ORDER;
                put16(payload + 1u, offset);
                payload[3] = length;
            } else if (!length) {
                payload[0] = COMMIT; // explicit end: omitted trailing bytes retain the defined baseline/zero
                put16(payload + 1u, snapshot_bytes(s));
            } else {
                put16(payload + 1u, offset);
                payload[3] = length;
            }

        }
        if (!noah_lighting_snapshot_sender_offer(&s->link, payload)) return 0;
    }
    return (uint8_t)noah_lighting_event_sender_size(&s->link);
}
static void ack(void *context, const uint8_t *payload) {
    noah_lighting_snapshot_t *s = context;
    if (payload[0] == PING)
        s->phase = BEGIN;
    else if (payload[0] == BEGIN)
        s->phase = snapshot_bytes(s) ? CHUNK : COMMIT;
    else if (payload[0] == ORDER) {
        s->order_offset = get16(payload + 1u) + payload[3];
    } else if (payload[0] == CHUNK) {
        s->offset = get16(payload + 1u) + payload[3];
        if (!payload[3]) s->phase = COMMIT;
    } else {
        noah_lighting_working_release_snapshot(s->working);
        s->have_baseline = true;
        s->sent_revision++;
        memset(s->fact_dirty, 0, sizeof s->fact_dirty);
        s->phase = PING;
    }
}
bool noah_lighting_snapshot_reply(noah_lighting_snapshot_t *s, const uint8_t reply[14], uint32_t before, uint32_t after) {
    // Only committed ACK of the actual PING attempt supplies calibration.
    // Duplicate ACK samples still correspond to this successful causal RPC.
    if (s->link.mailbox_ready && s->link.packet.outgoing[9] == PING && reply[1] == NOAH_LIGHTING_EVENT_ACK) {
        s->before = before;
        s->after  = after;
        s->remote = noah_lighting_event_get_u32(reply + 10u);
    }
    return noah_lighting_event_sender_reply(&s->link, reply, 10u, reset, ack, s);
}
