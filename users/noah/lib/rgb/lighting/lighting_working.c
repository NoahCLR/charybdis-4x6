#include "lighting_working.h"
#include "lighting_history_index.h"
#include "lighting_history_work.h"
#include <string.h>

enum { LOWER = 0u, UPPER = 2u, PHASE = 4u, COUNT = 5u, ENTRY = 6u, FLAGS = 7u, LENGTH = 8u, HEADER = 12u, MEMBER = 5u + KEY_ORIGIN_BITMAP_SIZE };
static uint16_t get16(const uint8_t *b) {
    return (uint16_t)b[0] | (uint16_t)b[1] << 8u;
}
static void put16(uint8_t *b, uint16_t n) {
    b[0] = (uint8_t)n;
    b[1] = (uint8_t)(n >> 8u);
}
static uint16_t add16(uint16_t n, uint32_t delta) {
    return delta >= 65535u - n ? 65535u : (uint16_t)(n + delta);
}
static uint8_t *bank(noah_lighting_working_t *w, bool snapshot) {
    return w->bytes + (w->current ^ snapshot) * w->bank_bytes;
}
bool noah_lighting_working_rule(const noah_lighting_working_t *w, uint8_t ordinal, noah_lighting_working_rule_t *out) {
    if (!w || !out || ordinal >= w->rules) return false;
    memcpy(out, w->bytes + w->current * w->bank_bytes + (uint16_t)ordinal * 12u, sizeof *out);
    return true;
}
static bool selected(const noah_lighting_working_rule_t *r, uint16_t thing, uint16_t *index) {
    if (r->count == 1u) {
        if (r->selected != 255u && thing != r->selected) return false;
        *index = 0u;
        return true;
    }
    if (thing >= r->count) return false;
    *index = thing;
    return true;
}
const uint8_t *noah_lighting_working_slot(const noah_lighting_working_t *w, bool snapshot, const noah_lighting_working_rule_t *r, uint16_t thing) {
    uint16_t index;
    if (!w || !r || !selected(r, thing, &index)) return NULL;
    return w->bytes + (w->current ^ snapshot) * w->bank_bytes + r->offset + index * r->stride;
}
static uint8_t *slot(noah_lighting_working_t *w, const noah_lighting_working_rule_t *r, uint16_t index) {
    return bank(w, false) + r->offset + index * r->stride;
}
static uint8_t thing_at(const noah_lighting_working_rule_t *r, uint16_t index) {
    return r->count == 1u ? r->selected : (uint8_t)index;
}
static bool rule_info(const noah_lighting_working_t *w, const noah_lighting_working_rule_t *r, noah_lighting_history_rule_t *info) {
    return noah_lighting_history_rule(w->program, r->rule, info);
}
bool noah_lighting_working_begin(noah_lighting_working_t *w, const noah_profile_lighting_v1_program_t *p, uint8_t *bytes, uint32_t capacity, uint32_t generation) {
    if (!w || !p || !p->valid || !bytes) return false;
    noah_profile_lighting_v1_view_t   view;
    noah_profile_lighting_v1_limits_t limits = noah_profile_lighting_v1_default_limits();
    noah_lighting_working_cost_t      cost;
    if (noah_profile_lighting_v1_decode(p->bytes, p->length, &limits, &view, NULL) != NOAH_PROFILE_LIGHTING_V1_OK || noah_profile_lighting_v1_working_admit(&view, capacity, &cost) != NOAH_PROFILE_LIGHTING_V1_OK || cost.bank_bytes > UINT16_MAX) return false;
    // Descriptor preparation is deterministic from this already-owned program.
    // Check the complete layout before discarding any prior generation.
    uint32_t verified       = 12u * cost.rules;
    uint16_t verified_rules = 0u;
    for (uint16_t i = 0u; i < p->rule_count; i++) {
        noah_lighting_history_rule_t info;
        if (!noah_lighting_history_rule(p, (uint8_t)i, &info)) return false;
        if (!info.retained) continue;
        const uint8_t *rule    = p->bytes + p->rules[i];
        uint8_t        members = (rule[3] >> 5u) == NOAH_PROFILE_LIGHTING_V1_SLOT_CONDITION ? noah_lighting_history_members(rule[4]) : 1u;
        verified += (info.selected == 255u ? info.domain : 1u) * ((12u + members * MEMBER + 3u) & ~3u);
        verified_rules++;
    }
    if (verified != cost.bank_bytes || verified_rules != cost.rules) return false;
    *w = (noah_lighting_working_t){.program = p, .bytes = bytes, .bank_bytes = (uint16_t)cost.bank_bytes, .scratch_bytes = cost.max_slot, .rules = cost.rules, .generation = generation};
    memset(bytes, 0, cost.required);
    uint32_t offset  = 12u * cost.rules;
    uint8_t  ordinal = 0;
    for (uint16_t i = 0; i < p->rule_count; i++) {
        noah_lighting_history_rule_t info;
        if (!noah_lighting_history_rule(p, (uint8_t)i, &info) || !info.retained) continue;
        const uint8_t *rule    = p->bytes + p->rules[i];
        uint8_t        members = 1u;
        if ((rule[3] >> 5u) == NOAH_PROFILE_LIGHTING_V1_SLOT_CONDITION) members = noah_lighting_history_members(rule[4]);
        uint16_t                     n      = info.selected == 255u ? info.domain : 1u;
        uint16_t                     stride = (uint16_t)((12u + members * MEMBER + 3u) & ~3u);
        noah_lighting_working_rule_t r      = {.offset = offset, .stride = stride, .count = n, .rule = (uint8_t)i, .selected = info.selected, .members = members, .flags = info.event};
        memcpy(bytes + ordinal++ * 12u, &r, sizeof r);
        offset += (uint32_t)n * stride;
    }
    // Decoder/cost and descriptor preparation share the same registry bounds.
    return true;
}
static void advance_slot(uint8_t *c, const noah_lighting_lifetime_parameters_t *p, uint32_t lower, uint32_t upper) {
    if (!c[PHASE]) return;
    put16(c + LOWER, add16(get16(c + LOWER), lower));
    put16(c + UPPER, add16(get16(c + UPPER), upper));
    if ((c[PHASE] == NOAH_LIGHTING_WORKING_FLASH && get16(c + UPPER) >= p->flash) || (c[PHASE] == NOAH_LIGHTING_WORKING_TAIL && get16(c + UPPER) >= p->linger)) {
        memset(c, 0, HEADER);
        return;
    }
}
void noah_lighting_working_advance(noah_lighting_working_t *w, uint32_t now) {
    if (!w || !w->program) return;
    uint32_t elapsed       = w->clock_ready ? now - w->last_time : 0u;
    bool     discontinuity = elapsed > INT32_MAX;
    if (discontinuity) elapsed = 0u;
    w->last_time   = now;
    w->clock_ready = true;
    for (uint8_t i = 0; i < w->rules; i++) {
        noah_lighting_working_rule_t r;
        noah_lighting_history_rule_t info;
        (void)noah_lighting_working_rule(w, i, &r);
        (void)rule_info(w, &r, &info);
        for (uint16_t t = 0; t < r.count; t++) {
            uint8_t *c = slot(w, &r, t);
            if (discontinuity && c[PHASE] != NOAH_LIGHTING_WORKING_MATCH)
                memset(c, 0, HEADER);
            else
                advance_slot(c, &info.timing, elapsed, elapsed);
        }
    }
}
// Canonical ranks are owned relations, not event or RPC identities. Empty and
// replaced entries are excluded before assigning new rank0, preserving ties.
static void entries(noah_lighting_working_t *w, const noah_lighting_working_rule_t *r) {
    bool any = false;
    for (uint16_t t = 0; t < r->count; t++)
        any |= !!(slot(w, r, t)[FLAGS] & NOAH_LIGHTING_WORKING_NEW_ENTRY);
    if (!any) return;
    uint8_t next = 1u;
    for (uint16_t rank = 0; rank < 128u; rank++) {
        bool found = false;
        for (uint16_t t = 0; t < r->count; t++) {
            uint8_t *c = slot(w, r, t);
            if (c[PHASE] && !(c[FLAGS] & NOAH_LIGHTING_WORKING_NEW_ENTRY) && c[ENTRY] == rank) {
                c[10] = next;
                found = true;
            }
        }
        if (found) next++;
    }
    for (uint16_t t = 0; t < r->count; t++) {
        uint8_t *c = slot(w, r, t);
        if (c[FLAGS] & NOAH_LIGHTING_WORKING_NEW_ENTRY) {
            c[ENTRY] = 0;
            c[FLAGS] &= (uint8_t)~NOAH_LIGHTING_WORKING_NEW_ENTRY;
        } else if (c[PHASE])
            c[ENTRY] = c[10];
        c[10] = 0;
    }
}
static void write_member(uint8_t *bytes, const noah_lighting_subject_t *s, uint16_t natural) {
    bytes[0] = s->slot;
    bytes[1] = s->outcome;
    bytes[2] = (uint8_t)(s->sides | (s->has_keys ? 4u : 0u));
    memcpy(bytes + 3u, s->keys, KEY_ORIGIN_BITMAP_SIZE);
    put16(bytes + 3u + KEY_ORIGIN_BITMAP_SIZE, natural);
}
void noah_lighting_working_member(const uint8_t *bytes, uint8_t kind, uint8_t thing, noah_lighting_subject_t *s, uint16_t *natural) {
    noah_lighting_subject_init(s, kind, thing, bytes[0]);
    s->outcome  = bytes[1];
    s->sides    = bytes[2] & 3u;
    s->has_keys = !!(bytes[2] & 4u);
    memcpy(s->keys, bytes + 3u, KEY_ORIGIN_BITMAP_SIZE);
    if (natural) *natural = get16(bytes + 3u + KEY_ORIGIN_BITMAP_SIZE);
}
bool noah_lighting_working_event(noah_lighting_working_t *w, const noah_lighting_event_group_t *event) {
    if (!w || !event || event->qualified[31] & 0x80u) return false;
    // Reject invalid group before changing ANY member. Source occurrences are
    // applied once by the producer; receiver sequence dedup precedes commit.
    for (uint16_t rule = 0; rule < 255u; rule++)
        if (event->qualified[rule / 8u] & (1u << (rule % 8u))) {
            bool found = false;
            for (uint8_t i = 0; i < w->rules; i++) {
                noah_lighting_working_rule_t r;
                noah_lighting_history_rule_t info;
                uint16_t                     t;
                (void)noah_lighting_working_rule(w, i, &r);
                if (r.rule != rule) continue;
                (void)rule_info(w, &r, &info);
                if (!info.event || info.thing_kind != event->thing_kind || !selected(&r, event->thing, &t)) return false;
                found = true;
                break;
            }
            if (!found) return false;
        }
    noah_lighting_subject_t s;
    noah_lighting_subject_init(&s, event->thing_kind, event->thing, event->slot);
    s.outcome  = event->outcome;
    s.sides    = event->sides;
    s.has_keys = event->has_keys;
    memcpy(s.keys, event->owners, KEY_ORIGIN_BITMAP_SIZE);
    for (uint8_t i = 0; i < w->rules; i++) {
        noah_lighting_working_rule_t r;
        (void)noah_lighting_working_rule(w, i, &r);
        if (!(event->qualified[r.rule / 8u] & (1u << (r.rule % 8u)))) continue;
        uint16_t t;
        (void)selected(&r, event->thing, &t);
        uint8_t *c = slot(w, &r, t);
        memset(c, 0, HEADER);
        c[PHASE] = NOAH_LIGHTING_WORKING_FLASH;
        c[COUNT] = 1;
        c[FLAGS] = NOAH_LIGHTING_WORKING_NEW_ENTRY;
        put16(c + LENGTH, MEMBER);
        write_member(c + HEADER, &s, 0u);
        entries(w, &r);
    }
    return true;
}
static bool holds(const noah_lighting_facts_t *f, const uint8_t *part) {
    noah_lighting_subject_t s;
    uint16_t                cursor = 0u;
    bool                    any    = noah_lighting_fact_next(part[1], f, part + 2u, &cursor, &s);
    return part[0] >> 5u == NOAH_PROFILE_LIGHTING_V1_SLOT_NEGATED ? !any : any;
}
bool noah_lighting_working_observe(noah_lighting_working_t *w, const noah_lighting_facts_t *facts) {
    if (!w || !facts) return false;
    for (uint8_t i = 0u; i < w->rules; i++) {
        noah_lighting_working_rule_t r;
        noah_lighting_history_rule_t info;
        (void)noah_lighting_working_rule(w, i, &r);
        (void)rule_info(w, &r, &info);
        if (info.event) continue;
        const uint8_t *first = w->program->bytes + w->program->rules[r.rule] + 3u;
        if ((first[0] >> 5u) != NOAH_PROFILE_LIGHTING_V1_SLOT_CONDITION) continue;
        for (uint16_t t = 0u; t < r.count; t++) {
            uint16_t                cursor = 0u, count = 0u;
            noah_lighting_subject_t subject;
            while (noah_lighting_fact_next(first[1], facts, first + 2u, &cursor, &subject)) {
                if (info.domain != 1u && subject.thing != thing_at(&r, t)) continue;
                if (++count > r.members) return false;
            }
        }
    }
    for (uint8_t i = 0; i < w->rules; i++) {
        noah_lighting_working_rule_t r;
        noah_lighting_history_rule_t info;
        (void)noah_lighting_working_rule(w, i, &r);
        (void)rule_info(w, &r, &info);
        if (info.event) continue;
        const uint8_t *rule = w->program->bytes + w->program->rules[r.rule], *at = rule + 3u, *end = rule + 1u + rule[0], *first = NULL;
        if (at < end && at[0] >> 5u == NOAH_PROFILE_LIGHTING_V1_SLOT_CONDITION) {
            first = at;
            at += 2u + (at[0] & 31u);
        }
        bool match = true;
        while (at < end && at[0] >> 5u <= NOAH_PROFILE_LIGHTING_V1_SLOT_NEGATED) {
            match &= holds(facts, at);
            at += 2u + (at[0] & 31u);
        }
        for (uint16_t t = 0; t < r.count; t++) {
            uint8_t *old = slot(w, &r, t), *next = w->bytes + 2u * w->bank_bytes;
            memcpy(next, old, HEADER);
            next[COUNT] = 0;
            if (match && first) {
                uint16_t                cursor = 0;
                noah_lighting_subject_t s;
                while (noah_lighting_fact_next(first[1], facts, first + 2u, &cursor, &s)) {
                    if (info.domain != 1u && s.thing != thing_at(&r, t)) continue;
                    // Full preflight above proved the immutable frame bound.
                    write_member(next + HEADER + next[COUNT] * MEMBER, &s, noah_lighting_fact_natural_policy(first[1]) == NOAH_LIGHTING_NATURAL_LAYER ? (uint16_t)(255u - s.thing) : (uint16_t)(cursor - 1u));
                    next[COUNT]++;
                }
            } else if (match) {
                noah_lighting_subject_t s;
                noah_lighting_subject_init(&s, NOAH_LIGHTING_THING_BOARD, 255u, 0u);
                write_member(next + HEADER, &s, 0u);
                next[COUNT] = 1u;
            }
            if (next[COUNT]) {
                if (old[PHASE] != NOAH_LIGHTING_WORKING_MATCH) {
                    put16(next + LOWER, 0);
                    put16(next + UPPER, 0);
                    next[FLAGS] = NOAH_LIGHTING_WORKING_NEW_ENTRY;
                }
                next[PHASE] = NOAH_LIGHTING_WORKING_MATCH;
                if (get16(next + LOWER) >= info.timing.show_after) next[FLAGS] |= NOAH_LIGHTING_WORKING_ELIGIBLE;
                put16(next + LENGTH, (uint16_t)next[COUNT] * MEMBER);
                memcpy(old, next, HEADER + (uint16_t)next[COUNT] * MEMBER);
            } else if (old[PHASE] == NOAH_LIGHTING_WORKING_MATCH) {
                if ((old[FLAGS] & NOAH_LIGHTING_WORKING_ELIGIBLE) && info.timing.linger) {
                    old[PHASE] = NOAH_LIGHTING_WORKING_TAIL;
                    put16(old + LOWER, 0);
                    put16(old + UPPER, 0);
                } else
                    memset(old, 0, HEADER);
            }
        }
        entries(w, &r);
    }
    return true;
}
bool noah_lighting_working_visible(const uint8_t *c, const noah_lighting_lifetime_parameters_t *p) {
    if (!c || !c[COUNT]) return false;
    if (c[PHASE] == NOAH_LIGHTING_WORKING_MATCH) return !!(c[FLAGS] & NOAH_LIGHTING_WORKING_ELIGIBLE);
    if (c[PHASE] == NOAH_LIGHTING_WORKING_TAIL) return get16(c + UPPER) < p->linger;
    return c[PHASE] == NOAH_LIGHTING_WORKING_FLASH && get16(c + LOWER) >= p->show_after && get16(c + UPPER) < p->flash;
}
void noah_lighting_working_snapshot(noah_lighting_working_t *w) {
    if (w && !w->snapshot_ready) {
        memcpy(bank(w, true), bank(w, false), w->bank_bytes);
        w->snapshot_ready = true;
    }
}
void noah_lighting_working_release_snapshot(noah_lighting_working_t *w) {
    if (w) w->snapshot_ready = false;
}
bool noah_lighting_working_commit(noah_lighting_working_t *w, uint16_t lower, uint16_t upper) {
    if (!w || !w->snapshot_ready || lower > upper) return false;
    // Compare every descriptor and complete slot encoding BEFORE mutation.
    for (uint8_t i = 0; i < w->rules; i++) {
        noah_lighting_working_rule_t r;
        if (!noah_lighting_working_rule(w, i, &r) || memcmp(bank(w, true) + i * 12u, bank(w, false) + i * 12u, 12u)) return false;
        noah_lighting_history_rule_t info;
        if (!rule_info(w, &r, &info)) return false;
        for (uint16_t t = 0; t < r.count; t++) {
            const uint8_t *c = bank(w, true) + r.offset + t * r.stride;
            if (c[PHASE] > NOAH_LIGHTING_WORKING_FLASH || c[COUNT] > r.members || get16(c + LENGTH) != (uint16_t)c[COUNT] * MEMBER || get16(c + LOWER) > get16(c + UPPER)) return false;
            if (c[PHASE] == NOAH_LIGHTING_WORKING_EMPTY) {
                for (uint8_t b = 0; b < HEADER; b++)
                    if (c[b]) return false;
                continue;
            }
            if (!c[COUNT] || c[ENTRY] >= r.count || c[FLAGS] & ~NOAH_LIGHTING_WORKING_ELIGIBLE || c[10] || c[11]) return false;
            if ((c[PHASE] == NOAH_LIGHTING_WORKING_FLASH) != info.event || (info.event && c[COUNT] != 1u)) return false;
            if (c[PHASE] == NOAH_LIGHTING_WORKING_TAIL && (!info.timing.linger || !(c[FLAGS] & NOAH_LIGHTING_WORKING_ELIGIBLE))) return false;
            for (uint8_t m = 0; m < c[COUNT]; m++) {
                const uint8_t *member = c + HEADER + m * MEMBER;
                if (member[2] & ~7u) return false;
                bool any = false;
                for (uint8_t b = 0; b < KEY_ORIGIN_BITMAP_SIZE; b++)
                    any |= member[3u + b] != 0u;
                if (any != !!(member[2] & 4u)) return false;
                if ((MATRIX_ROWS * MATRIX_COLS) % 8u && (member[2u + KEY_ORIGIN_BITMAP_SIZE] >> ((MATRIX_ROWS * MATRIX_COLS) % 8u))) return false;
            }
        }
    }
    for (uint8_t i = 0; i < w->rules; i++) {
        noah_lighting_working_rule_t r;
        noah_lighting_history_rule_t info;
        (void)noah_lighting_working_rule(w, i, &r);
        (void)rule_info(w, &r, &info);
        for (uint16_t t = 0; t < r.count; t++) {
            uint8_t       *next = bank(w, true) + r.offset + t * r.stride;
            const uint8_t *old  = slot(w, &r, t);
            if (next[PHASE] == NOAH_LIGHTING_WORKING_MATCH) {
                // State show-after remains per-half observed-frame time.
                // A continuously observed thing keeps its local gate; a new
                // visible match starts locally, unlike source-aged flash/tail.
                put16(next + LOWER, old[PHASE] == NOAH_LIGHTING_WORKING_MATCH ? get16(old + LOWER) : 0u);
                put16(next + UPPER, old[PHASE] == NOAH_LIGHTING_WORKING_MATCH ? get16(old + UPPER) : 0u);
                next[FLAGS] = (uint8_t)((next[FLAGS] & ~NOAH_LIGHTING_WORKING_ELIGIBLE) | ((old[PHASE] == NOAH_LIGHTING_WORKING_MATCH && (old[FLAGS] & NOAH_LIGHTING_WORKING_ELIGIBLE)) || !info.timing.show_after ? NOAH_LIGHTING_WORKING_ELIGIBLE : 0u));
            } else
                advance_slot(next, &info.timing, lower, upper);
        }
    }
    w->current ^= 1u;
    w->snapshot_ready = false;
    return true;
}

bool noah_lighting_working_paint(const noah_lighting_working_t *w, uint8_t rule, noah_lighting_working_paint_fn paint, void *context) {
    if (!w || !paint) return false;
    noah_lighting_working_rule_t r;
    bool                         found = false;
    for (uint8_t i = 0u; i < w->rules; i++) {
        (void)noah_lighting_working_rule(w, i, &r);
        if (r.rule == rule) {
            found = true;
            break;
        }
    }
    if (!found) return false;
    noah_lighting_history_rule_t info;
    (void)rule_info(w, &r, &info);
    uint64_t before = UINT64_MAX;
    for (;;) {
        uint64_t       best   = 0u;
        const uint8_t *member = NULL;
        uint8_t        thing  = 255u;
        for (uint16_t t = 0u; t < r.count; t++) {
            const uint8_t *c = noah_lighting_working_slot(w, false, &r, thing_at(&r, t));
            if (!noah_lighting_working_visible(c, &info.timing)) continue;
            for (uint8_t m = 0u; m < c[COUNT]; m++) {
                NOAH_LIGHTING_HISTORY_WORK(NOAH_LIGHTING_WORK_SEARCH, 1u);
                const uint8_t *candidate = c + HEADER + m * MEMBER;
                uint64_t       key       = ((uint64_t)get16(candidate + 3u + KEY_ORIGIN_BITMAP_SIZE) << 24u) | ((uint32_t)c[ENTRY] << 16u) | ((uint16_t)thing_at(&r, t) << 8u) | m;
                // Add1: key0 is valid, UINT64_MAX remains the initial sentinel.
                key++;
                if (key < before && key > best) {
                    best   = key;
                    member = candidate;
                    thing  = thing_at(&r, t);
                }
            }
        }
        if (!member) break;
        noah_lighting_subject_t subject;
        noah_lighting_working_member(member, info.thing_kind, thing, &subject, NULL);
        paint(context, &subject);
        before = best;
    }
    return true;
}
uint32_t noah_lighting_working_wake(const noah_lighting_working_t *w) {
    uint32_t wake = UINT32_MAX;
    if (!w) return wake;
    for (uint8_t i = 0u; i < w->rules; i++) {
        noah_lighting_working_rule_t r;
        noah_lighting_history_rule_t info;
        (void)noah_lighting_working_rule(w, i, &r);
        (void)rule_info(w, &r, &info);
        for (uint16_t t = 0u; t < r.count; t++) {
            const uint8_t *c   = noah_lighting_working_slot(w, false, &r, thing_at(&r, t));
            uint32_t       due = UINT32_MAX;
            if (c[PHASE] == NOAH_LIGHTING_WORKING_MATCH && !(c[FLAGS] & NOAH_LIGHTING_WORKING_ELIGIBLE) && get16(c + LOWER) < info.timing.show_after) due = info.timing.show_after - get16(c + LOWER);
            if (c[PHASE] == NOAH_LIGHTING_WORKING_FLASH) {
                due = info.timing.flash - get16(c + UPPER);
                if (get16(c + LOWER) < info.timing.show_after && (uint32_t)(info.timing.show_after - get16(c + LOWER)) < due) due = info.timing.show_after - get16(c + LOWER);
            }
            if (c[PHASE] == NOAH_LIGHTING_WORKING_TAIL) due = info.timing.linger - get16(c + UPPER);
            if (due < wake) wake = due;
        }
    }
    return wake;
}
static uint8_t descriptor_byte(const noah_lighting_working_rule_t *r, uint8_t field) {
    if (field < 4u) return (uint8_t)(r->offset >> (8u * field));
    if (field < 6u) return (uint8_t)(r->stride >> (8u * (field - 4u)));
    if (field < 8u) return (uint8_t)(r->count >> (8u * (field - 6u)));
    if (field == 8u) return r->rule;
    if (field == 9u) return r->selected;
    if (field == 10u) return r->members;
    return r->flags;
}
bool noah_lighting_working_read_snapshot(const noah_lighting_working_t *w, uint16_t offset, uint8_t *out, uint8_t length) {
    if (!w || !w->snapshot_ready || !out || offset > w->bank_bytes || length > w->bank_bytes - offset) return false;
    for (uint16_t i = 0; i < length; i++) {
        uint16_t at = offset + i;
        if (at < 12u * w->rules) {
            noah_lighting_working_rule_t r;
            (void)noah_lighting_working_rule(w, at / 12u, &r);
            out[i] = descriptor_byte(&r, at % 12u);
        } else
            out[i] = w->bytes[(w->current ^ 1u) * w->bank_bytes + at];
    }
    return true;
}
bool noah_lighting_working_stage(noah_lighting_working_t *w, uint16_t offset, const uint8_t *bytes, uint8_t length) {
    if (!w || !bytes || offset > w->bank_bytes || length > w->bank_bytes - offset) return false;
    // Validate all descriptor bytes before changing the staging bank.
    for (uint16_t i = 0; i < length && offset + i < 12u * w->rules; i++) {
        noah_lighting_working_rule_t r;
        (void)noah_lighting_working_rule(w, (offset + i) / 12u, &r);
        if (bytes[i] != descriptor_byte(&r, (offset + i) % 12u)) return false;
    }
    for (uint16_t i = 0; i < length; i++)
        if (offset + i >= 12u * w->rules) bank(w, true)[offset + i] = bytes[i];
    // Native descriptors are local only, never copied from native wire padding.
    memcpy(bank(w, true), bank(w, false), 12u * w->rules);
    w->snapshot_ready = true;
    return true;
}

void noah_lighting_working_seen(noah_lighting_working_t *w) {
    if (!w) return;
    for (uint8_t i = 0; i < w->rules; i++) {
        noah_lighting_working_rule_t r;
        noah_lighting_history_rule_t info;
        (void)noah_lighting_working_rule(w, i, &r);
        (void)rule_info(w, &r, &info);
        for (uint16_t t = 0; t < r.count; t++) {
            uint8_t *c = slot(w, &r, t);
            if (c[PHASE] == NOAH_LIGHTING_WORKING_MATCH && get16(c + LOWER) >= info.timing.show_after) c[FLAGS] |= NOAH_LIGHTING_WORKING_ELIGIBLE;
        }
    }
}
