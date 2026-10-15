// Lighting render vectors: every per-entry render vector in
// tests/fixtures/lighting_vectors_v1.json, rendered by the firmware engine on
// the Charybdis 4x6 geometry. Each must match the reference model colour for
// colour. run_lighting_render_vectors_tests.sh flattens the JSON into lines.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"
#include "matrix.h"

#include "users/noah/lib/rgb/lighting/lighting_engine.h"
#include "users/noah/lib/rgb/lighting/lighting_occurrence.h"
#include "users/noah/lib/rgb/lighting/lighting_event_capture.h"
#include "users/noah/lib/rgb/lighting/lighting_batch.h"
#include "users/noah/lib/rgb/lighting/lighting_workspace.h"
#include "users/noah/lib/rgb/lighting/lighting_keymap.h"
#include "users/noah/lib/key/behavior/key_behavior_lookup.h"
#include "users/noah/lib/profile/runtime/effective_combo_runtime.h"
#include "users/noah/lib/profile/runtime/effective_key_behavior_runtime.h"
#include "users/noah/lib/pointing/defs/pd_mode_flags.h"
#include "users/noah/lib/profile/runtime/effective_settings_runtime.h"

uint16_t noah_lighting_test_live_clocks(void);
uint16_t noah_lighting_test_live_bound(void);

// ─── QMK and runtime stand-ins ──────────────────────────────────────────────

layer_state_t layer_state;
static int8_t  key_leds[MATRIX_ROWS * MATRIX_COLS];
static uint8_t mapped[LAYER_COUNT][KEY_ORIGIN_BITMAP_SIZE];
static uint16_t native_keys[LAYER_COUNT][NOAH_LIGHTING_MATRIX_KEYS];
static uint8_t native_behaviour[LAYER_COUNT][NOAH_LIGHTING_MATRIX_KEYS];
static combo_t query_combos[128];
static uint16_t query_inputs[128][17];
static uint32_t query_layers[128];
static uint8_t query_enabled[128];
static uint32_t keymap_generation, storage_reads;
noah_effective_key_behavior_result_t noah_effective_key_behavior_lookup(uint16_t code, noah_effective_key_behavior_row_t *row) {
    (void)code; (void)row;
    return NOAH_EFFECTIVE_KEY_BEHAVIOR_COMPILED_FALLBACK;
}
key_behavior_view_t key_behavior_lookup(uint16_t code) {
    storage_reads++;
    for (uint8_t layer=0;layer<LAYER_COUNT;layer++) for(uint8_t key=0;key<NOAH_LIGHTING_MATRIX_KEYS;key++)
        if(native_keys[layer][key]==code && native_behaviour[layer][key]) return (key_behavior_view_t){.lighting_identity=native_behaviour[layer][key]};
    return (key_behavior_view_t){0};
}
uint16_t noah_effective_combo_count(void) { return 128u; }
combo_t *noah_effective_combo_get(uint16_t row) { return row<128u ? &query_combos[row] : NULL; }
bool noah_effective_combo_enabled(uint16_t row) { return row<128u && query_enabled[row]; }
uint32_t noah_effective_combo_allowed_layers(uint16_t row) { return row<128u ? query_layers[row] : 0u; }

static rgb_t   effect[RGB_MATRIX_LED_COUNT];
static bool    effect_known[RGB_MATRIX_LED_COUNT];
static uint16_t fade_progress;
static uint32_t fade_timeout = 900u, fade_dead_time = 300u;
static uint8_t  fade_layer = 0xffu;
static uint32_t input_elapsed;
static uint32_t rgb_timeout = 900000u;

rgb_t hsv_to_rgb(hsv_t hsv) {
    rgb_t    rgb;
    uint8_t  region, remainder, p, q, t;
    uint16_t h = hsv.h, s = hsv.s, v = hsv.v;

    if (hsv.s == 0) return (rgb_t){.r = hsv.v, .g = hsv.v, .b = hsv.v};
    region    = (uint8_t)(h * 6 / 255);
    remainder = (uint8_t)((h * 2 - region * 85) * 3);
    p         = (uint8_t)((v * (255 - s)) >> 8);
    q         = (uint8_t)((v * (255 - ((s * remainder) >> 8))) >> 8);
    t         = (uint8_t)((v * (255 - ((s * (255 - remainder)) >> 8))) >> 8);
    switch (region) {
        case 6:
        case 0:
            rgb = (rgb_t){.r = (uint8_t)v, .g = t, .b = p};
            break;
        case 1:
            rgb = (rgb_t){.r = q, .g = (uint8_t)v, .b = p};
            break;
        case 2:
            rgb = (rgb_t){.r = p, .g = (uint8_t)v, .b = t};
            break;
        case 3:
            rgb = (rgb_t){.r = p, .g = q, .b = (uint8_t)v};
            break;
        case 4:
            rgb = (rgb_t){.r = t, .g = p, .b = (uint8_t)v};
            break;
        default:
            rgb = (rgb_t){.r = (uint8_t)v, .g = p, .b = q};
            break;
    }
    return rgb;
}

uint8_t rgb_matrix_map_row_column_to_led(uint8_t row, uint8_t column, uint8_t *led_i) {
    int8_t led = key_leds[row * MATRIX_COLS + column];
    if (led < 0) return 0u;
    led_i[0] = (uint8_t)led;
    return 1u;
}

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    storage_reads++;
    return native_keys[layer_num][row*MATRIX_COLS+column];
}

bool noah_lighting_effect_colour(uint8_t led, rgb_t *colour) {
    if (!effect_known[led]) return false;
    *colour = effect[led];
    return true;
}

uint32_t noah_setting(uint8_t id, uint32_t fallback) {
    if (id == NOAH_SETTING_RGB_TIMEOUT) return rgb_timeout;
    if (id == NOAH_SETTING_AUTO_MOUSE_TIMEOUT) return fade_timeout;
    if (id == NOAH_SETTING_AUTO_MOUSE_DEAD_TIME) return fade_dead_time;
    return fallback;
}

uint16_t automouse_rgb_current_progress(void) {
    uint16_t span = (uint16_t)(fade_timeout - fade_dead_time);
    return fade_progress > span ? span : fade_progress;
}

uint8_t get_auto_mouse_layer(void) {
    return fade_layer;
}

bool key_runtime_core_preview_owner_key_pos(keypos_t *key) {
    (void)key;
    return false;
}
bool pd_mode_local_owner_key_pos_snapshot(keypos_t *key) {
    (void)key;
    return false;
}
void layer_ownership_momentary_owner_bitmap(uint8_t *out) {
    key_origin_bitmap_clear(out);
}
bool noah_qmk_combo_origin_lighting_next(keypos_t preview, keypos_t mode, const uint8_t *quiet, uint16_t *cursor, uint8_t *id, uint8_t *role, uint8_t *keys) {
    (void)preview;
    (void)mode;
    (void)quiet;
    (void)cursor;
    (void)id;
    (void)role;
    (void)keys;
    return false;
}
void key_feedback_lighting_map(uint8_t *out) {
    memset(out, 0, KEY_FEEDBACK_LIGHTING_BYTES);
}

uint8_t get_highest_layer(layer_state_t state) {
    for (uint8_t layer = LAYER_COUNT; layer-- > 0u;) if (state & ((layer_state_t)1u << layer)) return layer;
    return 0u;
}
bool layer_state_cmp(layer_state_t state, uint8_t layer) { return (state & ((layer_state_t)1u << layer)) != 0u; }
uint32_t last_input_activity_elapsed(void) { return input_elapsed; }
matrix_row_t matrix_get_row(uint8_t row) { (void)row; return 0u; }
led_t host_keyboard_led_state(void) { return (led_t){.raw = 0u}; }
uint8_t get_mods(void) { return 0u; }
uint8_t get_weak_mods(void) { return 0u; }
uint8_t get_oneshot_mods(void) { return 0u; }
uint8_t get_oneshot_locked_mods(void) { return 0u; }
uint8_t macro_payload_playing_slot(void) { return 255u; }
void layer_ownership_lighting_projection(uint8_t *out) { memset(out, 0, 63u); out[62] = 255u; }
void noah_qmk_combo_origin_lighting_layers(uint8_t *out) { memset(out, 255, NOAH_LIGHTING_COMBO_SLICE_BYTES); }
void key_feedback_lighting_phases(uint8_t *out) { memset(out, 0, MATRIX_ROWS * MATRIX_COLS * 3u); }
bool get_auto_mouse_enable(void) { return false; }
bool pd_mode_auto_sniping_layer_active(void) { return false; }
bool charybdis_get_pointer_sniping_enabled(void) { return false; }
bool noah_pointing_sensor_motion_observed(void) { return false; }

// Collection is the USB half's job; these vectors set the facts directly.
void    key_feedback_semantic_map(uint8_t *out_map) { key_feedback_semantic_map_clear(out_map); }
void    key_feedback_tap_branch_map(uint8_t *out_map) { key_feedback_tap_branch_map_clear(out_map); }
void    key_feedback_broad_owner_map(uint8_t *out_map) { key_feedback_broad_owner_map_clear(out_map); }
void    key_feedback_flash_visibility_bitmap_for_semantic_map(const uint8_t *semantic_map, uint8_t *out_bitmap) { (void)semantic_map; key_origin_bitmap_clear(out_bitmap); }
uint8_t key_feedback_preview_layer(void) { return UINT8_MAX; }
void    combo_feedback_bitmaps(uint8_t *underlay, uint8_t *overlay) { key_origin_bitmap_clear(underlay); key_origin_bitmap_clear(overlay); }
pd_mode_snapshot_t pd_mode_snapshot_with_owner_bitmap(uint8_t *out_owner_bitmap, bool *out_has_owner_keys) {
    pd_mode_snapshot_t snapshot = {0};
    key_origin_bitmap_clear(out_owner_bitmap);
    *out_has_owner_keys        = false;
    snapshot.display.active_index = PD_MODE_COUNT;
    return snapshot;
}

uint32_t noah_lighting_keymap_generation(void) {
    return keymap_generation;
}

// ─── Vectors ────────────────────────────────────────────────────────────────

static void read_hex(FILE *file, uint8_t *out, size_t length) {
    for (size_t i = 0; i < length; i++) {
        unsigned value;
        assert(fscanf(file, "%2x", &value) == 1);
        out[i] = (uint8_t)value;
    }
}

static keypos_t key_at(unsigned index) {
    return (keypos_t){.row = (uint8_t)(index / MATRIX_COLS), .col = (uint8_t)(index % MATRIX_COLS)};
}

static noah_lighting_facts_t capture_source;
static noah_lighting_event_group_t capture_retained;
static unsigned capture_collects, capture_retains;
static uint32_t capture_time;
static bool capture_shared;
static void capture_collect(noah_lighting_facts_t *facts) { *facts = capture_source; capture_collects++; }
static void capture_retain(void *context, const noah_lighting_event_group_t *group, uint32_t now) {
    assert(context == &capture_retained && now == capture_time);
    capture_retained = *group;
    if (capture_shared) {
        assert(group == &noah_lighting_workspace.capture.group);
        noah_lighting_subject_t subject;
        noah_lighting_subject_init(&subject, group->thing_kind, group->thing, group->slot);
        subject.outcome = group->outcome; subject.sides = group->sides; subject.has_keys = group->has_keys;
        memcpy(subject.keys, group->owners, sizeof subject.keys);
        noah_lighting_batch_writer_t writer;
        noah_lighting_batch_write_begin(&writer, noah_lighting_workspace.capture.encoded, sizeof noah_lighting_workspace.capture.encoded);
        assert(noah_lighting_batch_write(&writer, &subject));
        // Qualification facts and output coexist, including max owners. No
        // dead-facts assumption or later filter requalification is required.
        assert(!memcmp(&capture_source, &noah_lighting_workspace.capture.facts, sizeof capture_source));
        assert(!memcmp(&capture_retained, group, sizeof capture_retained));
        noah_lighting_batch_reader_t reader;
        noah_lighting_subject_t decoded;
        assert(noah_lighting_batch_read_begin(&reader, writer.bytes, writer.used, group->thing_kind, group->thing));
        assert(noah_lighting_batch_read(&reader, &decoded));
        assert(!memcmp(&decoded, &subject, sizeof decoded));
    }
    capture_retains++;
}
static void check_occurrence(FILE *file, const char *name, const noah_profile_lighting_v1_program_t *program, noah_lighting_facts_t *facts) {
    unsigned long long order;
    unsigned kind, thing, branch, slot, outcome, key, has_owners, layers, locks, macro, mods, armed;
    uint8_t owners[KEY_ORIGIN_BITMAP_SIZE];
    char expected[2u * NOAH_LIGHTING_EVENT_GROUP_BYTES + 1u];
    assert(fscanf(file, "%llu %u %u %u %u %u %u %u %u %u %u %u %u", &order, &kind, &thing, &branch, &slot, &outcome, &key, &has_owners, &layers, &locks, &macro, &mods, &armed) == 13);
    read_hex(file, owners, sizeof owners);
    memset(facts, 0, sizeof *facts);
    noah_lighting_facts_init_synced(facts);
    facts->layer_active[0] = layers;
    facts->layer_active[1] = layers >> 8u;
    facts->host_lock[0] = locks;
    facts->macro_playing[0] = macro;
    facts->modifier[0] = mods;
    facts->modifier[1] = armed;
    read_hex(file, facts->key_pressed, sizeof facts->key_pressed);
    assert(fscanf(file, "%108s", expected) == 1);
    noah_lighting_event_t event = {.kind=kind, .thing=thing, .branch=branch, .slot=slot, .outcome=outcome, .key=key_at(key), .owners=has_owners ? owners : NULL};
    noah_lighting_event_group_t group;
    noah_lighting_facts_t before = *facts;
    uint32_t reads = storage_reads;
    bool result = noah_lighting_event_qualify(program, facts, &event, (uint64_t)order, &group);
    assert(storage_reads == reads && !memcmp(&before, facts, sizeof before));
    // Exercise the actual collect->qualify->retain consumer pipeline as well
    // as the independent qualifier. The retained copy survives scratch reuse.
    capture_source = *facts;
    capture_time = 1234u;
    unsigned collected = capture_collects, retained = capture_retains;
    noah_lighting_facts_t scratch;
    noah_lighting_event_group_t scratch_group;
    bool captured = noah_lighting_event_capture(program, &event, (uint64_t)order, capture_time, &scratch, &scratch_group, capture_collect, capture_retain, &capture_retained);
    assert(captured == result && capture_collects == collected + 1u && capture_retains == retained + (result ? 1u : 0u));
    if (result) assert(!memcmp(&capture_retained, &group, sizeof group));
    memset(&scratch, 0x55, sizeof scratch);
    memset(&scratch_group, 0x55, sizeof scratch_group);
    if (result) assert(!memcmp(&capture_retained, &group, sizeof group));
    assert(storage_reads == reads);
    // The same engine workspace alternates synchronous render and capture.
    // Resolved caller facts/frame are independent and survive either use.
    noah_lighting_frame_t frame_before = {0}, frame_after = {0};
    noah_lighting_resolve_info_t info;
    noah_lighting_scene_t scene = {.program = program, .facts = facts, .keymap_generation = keymap_generation};
    noah_lighting_resolve(&scene, NOAH_LIGHTING_ALL_LEDS, capture_time, true, &frame_before, &info);
    collected = capture_collects; retained = capture_retains;
    capture_shared = true;
    captured = noah_lighting_event_capture_shared(program, &event, (uint64_t)order, capture_time, capture_collect, capture_retain, &capture_retained);
    capture_shared = false;
    assert(captured == result && capture_collects == collected + 1u && capture_retains == retained + (result ? 1u : 0u));
    if (result) assert(!memcmp(&capture_retained, &group, sizeof group));
    memset(&noah_lighting_workspace, 0xa5, sizeof noah_lighting_workspace);
    noah_lighting_resolve(&scene, NOAH_LIGHTING_ALL_LEDS, capture_time, false, &frame_after, &info);
    assert(!memcmp(&frame_before, &frame_after, sizeof frame_before));
    assert(!memcmp(&before, facts, sizeof before));

    if (!strcmp(expected, "-")) { assert(!result); return; }
    assert(result);
    memset(owners, 0x55, sizeof owners); // Borrowed producer data has expired.
    uint8_t wire[NOAH_LIGHTING_EVENT_GROUP_BYTES];
    assert(noah_lighting_event_group_encode(wire, sizeof wire, &group));
    for (unsigned i = 0; i < sizeof wire; i++) {
        unsigned value;
        assert(sscanf(expected + 2u*i, "%2x", &value) == 1);
        if (wire[i] != value) { fprintf(stderr, "occurrence %s byte%u: expected%02x got%02x\n",name,i,value,wire[i]); exit(1); }
    }
}

static void check_actual_fact_batches(const noah_profile_lighting_v1_program_t *program, const noah_lighting_facts_t *facts) {
    static noah_lighting_subject_t originals[64];
    uint8_t bytes[NOAH_LIGHTING_BATCH_BYTES];
    for (unsigned r=0;r<program->rule_count;r++) {
        const uint8_t *part=program->bytes+program->rules[r]+3u;
        if ((part[0]>>5u)!=NOAH_PROFILE_LIGHTING_V1_SLOT_CONDITION || noah_lighting_fact_role(part[1])==NOAH_LIGHTING_FACT_EVENT) continue;
        uint16_t cursor=0;
        unsigned count=0;
        while (count<64 && noah_lighting_fact_next(part[1],facts,part+2,&cursor,&originals[count])) count++;
        for(unsigned member=0;member<count;member++) {
            bool first=true;
            for(unsigned before=0;before<member;before++) if(originals[before].thing==originals[member].thing)first=false;
            if(!first)continue;
            noah_lighting_batch_writer_t writer;
            noah_lighting_batch_write_begin(&writer,bytes,sizeof bytes);
            for(unsigned j=member;j<count;j++) if(originals[j].thing==originals[member].thing) assert(noah_lighting_batch_write(&writer,&originals[j]));
            noah_lighting_batch_reader_t reader;
            assert(noah_lighting_batch_read_begin(&reader,bytes,writer.used,originals[member].thing_kind,originals[member].thing));
            for(unsigned j=member;j<count;j++) if(originals[j].thing==originals[member].thing) {
                noah_lighting_subject_t restored;assert(noah_lighting_batch_read(&reader,&restored));
                assert(!memcmp(&restored,&originals[j],sizeof restored));
            }
            assert(!reader.remaining && reader.offset==writer.used);
        }
    }
}

int main(int argc, char **argv) {
    uint8_t idle;
    input_elapsed = 900000u;
    noah_lighting_slice_idle_collect(&idle);
    assert(!idle); // existing sleep is strictly greater than timeout
    input_elapsed++;
    noah_lighting_slice_idle_collect(&idle);
    assert(idle);
    rgb_timeout = 0u;
    noah_lighting_slice_idle_collect(&idle);
    assert(!idle); // zero disables sleep
    rgb_timeout = 900000u;
    input_elapsed = 0u;
    static noah_profile_lighting_v1_program_t program;
    static noah_lighting_facts_t              facts;
    static noah_lighting_frame_t              frame;
    static char                               payload_hex[20000];
    static uint8_t                            payload[10000];
    char                                      word[96], name[96];
    size_t                                    cases = 0, steps = 0, occurrence_cases = 0, occurrence_steps = 0;
    bool occurrence_case = false;
    uint16_t peak_clocks = 0u;
    noah_profile_lighting_v1_limits_t         limits = noah_profile_lighting_v1_default_limits();

    assert(argc == 2);
    limits.maximum_brightness = 200u;
    FILE *file = fopen(argv[1], "r");
    assert(file);
    assert(fscanf(file, "%95s", word) == 1 && strcmp(word, "GEOMETRY") == 0);
    for (unsigned key = 0; key < MATRIX_ROWS * MATRIX_COLS; key++) {
        int led;
        assert(fscanf(file, "%d", &led) == 1);
        key_leds[key] = (int8_t)led;
    }
    while (fscanf(file, "%95s", word) == 1) {
        if (strcmp(word, "CASE") == 0 || strcmp(word, "OCCURRENCE") == 0) {
            occurrence_case = !strcmp(word, "OCCURRENCE");
            noah_profile_lighting_v1_view_t view;
            assert(fscanf(file, "%95s %19999s", name, payload_hex) == 2);
            size_t length = strlen(payload_hex) / 2u;
            for (size_t i = 0; i < length; i++) {
                unsigned value;
                assert(sscanf(payload_hex + 2 * i, "%2x", &value) == 1);
                payload[i] = (uint8_t)value;
            }
            assert(noah_profile_lighting_v1_decode(payload, length, &limits, &view, NULL) == NOAH_PROFILE_LIGHTING_V1_OK);
            assert(noah_profile_lighting_v1_program_load(&view, &program));
            noah_lighting_engine_program_changed();
            if (occurrence_case) occurrence_cases++; else cases++;
            continue;
        }
        if (!strcmp(word, "QUALIFY")) {
            assert(occurrence_case);
            check_occurrence(file, name, &program, &facts);
            occurrence_steps++;
            continue;
        }
        assert(!occurrence_case && strcmp(word, "STEP") == 0);
        unsigned long now;
        unsigned      layers, preview, mode, sides, keyed, count, owners[KEY_FEEDBACK_BROAD_OWNER_COUNT], fade_values[4];
        uint8_t       mode_keys[KEY_ORIGIN_BITMAP_SIZE];
        memset(&facts, 0, sizeof(facts));
        assert(fscanf(file, "%lu %u %u %u %u %u", &now, &layers, &preview, &mode, &sides, &keyed) == 6);
        read_hex(file, mode_keys, sizeof(mode_keys));
        facts.layer_active[0]     = (uint8_t)layers;
        facts.layer_active[1]     = (uint8_t)(layers >> 8u);
        facts.layer_previewing[0] = (uint8_t)preview;
        facts.pointing_mode_active[0] = (uint8_t)mode;
        facts.pointing_mode_active[1] = (uint8_t)sides;
        facts.pointing_mode_active[2] = (uint8_t)keyed;
        memcpy(&facts.pointing_mode_active[3], mode_keys, sizeof(mode_keys));
        read_hex(file, facts.combos_held, 2u * KEY_ORIGIN_BITMAP_SIZE);
        noah_lighting_slice_combo_held_idle(facts.combo_held);
        assert(fscanf(file, "%u", &count) == 1 && count <= COMBO_BUFFER_LENGTH);
        for (unsigned i = 0; i < count; i++) {
            unsigned id, role;
            assert(fscanf(file, "%u %u", &id, &role) == 2);
            uint8_t *record = &facts.combo_held[i * NOAH_LIGHTING_COMBO_RECORD_BYTES];
            record[0]       = id;
            record[1]       = role;
            read_hex(file, record + 2u, KEY_ORIGIN_BITMAP_SIZE);
        }
        for (unsigned layer = 0; layer < LAYER_COUNT; layer++) read_hex(file, mapped[layer], KEY_ORIGIN_BITMAP_SIZE);
        memset(native_behaviour,0,sizeof(native_behaviour));
        for(unsigned layer=0;layer<LAYER_COUNT;layer++) for(unsigned key=0;key<NOAH_LIGHTING_MATRIX_KEYS;key++)
            native_keys[layer][key] = key_origin_bitmap_has_keypos(mapped[layer],key_at(key)) ? KC_A : KC_NO;
        assert(fscanf(file,"%u",&count)==1);
        for(unsigned i=0;i<count;i++) {
            unsigned layer,key,code,identity;
            assert(fscanf(file,"%u %u %u %u",&layer,&key,&code,&identity)==4);
            assert(layer<LAYER_COUNT && key<NOAH_LIGHTING_MATRIX_KEYS);
            native_keys[layer][key]=code;native_behaviour[layer][key]=identity==255?0:identity+1;
        }
        memset(query_enabled,0,sizeof(query_enabled));
        assert(fscanf(file,"%u",&count)==1);
        for(unsigned i=0;i<count;i++) {
            unsigned row,enabled,layers,inputs;
            assert(fscanf(file,"%u %u %u %u",&row,&enabled,&layers,&inputs)==4);
            assert(row<128 && inputs<=16);
            query_enabled[row]=enabled;query_layers[row]=layers;query_combos[row].keys=query_inputs[row];
            for(unsigned j=0;j<inputs;j++) {unsigned code;assert(fscanf(file,"%u",&code)==1);query_inputs[row][j]=code;}
            query_inputs[row][inputs]=COMBO_END;
        }
        keymap_generation++;
        assert(noah_lighting_keymap_refresh(keymap_generation));

        {
            uint8_t *slice    = facts.behaviour_state;
            uint8_t *visible  = slice;
            uint8_t *semantic = visible + KEY_ORIGIN_BITMAP_SIZE;
            uint8_t *owner    = semantic + KEY_FEEDBACK_SEMANTIC_MAP_SIZE;
            uint8_t *branch   = owner + KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE;
            key_feedback_broad_owner_map_clear(owner);
            assert(fscanf(file, "%u", &count) == 1);
            for (unsigned entry = 0; entry < count; entry++) {
                unsigned key, state, branch_value, flashing, lit, identity, outcome;
                assert(fscanf(file, "%u %u %u %u %u %u %u", &key, &state, &branch_value, &flashing, &lit, &identity, &outcome) == 7);
                bool covered_branch = state == 0u && (key_origin_bitmap_has_keypos(facts.combos_held, key_at(key)) || key_origin_bitmap_has_keypos(facts.combos_held + KEY_ORIGIN_BITMAP_SIZE, key_at(key)));
                // The runtime collector suppresses selected branches covered
                // by a combo (key_feedback_lighting_map); emulate that input.
                if (identity < 128u && (!flashing || lit) && !covered_branch) {
                    uint8_t *record = &facts.behaviour[key * KEY_FEEDBACK_LIGHTING_RECORD_BYTES];
                    record[0]       = identity + 1u;
                    unsigned index  = branch_value <= 2u ? 0u : branch_value - 2u;
                    record[1]       = state == 0u ? 4u + (index > NOAH_PROFILE_TAP_DEPTH - 2u ? NOAH_PROFILE_TAP_DEPTH - 2u : index) : state == 1u ? 3u : state == 2u ? 1u : 2u;
                    record[2]       = outcome;
                }
                key_feedback_semantic_t value = state == 0 ? KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING : state == 1 ? KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED : state == 2 ? (flashing ? KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING : KEY_FEEDBACK_SEMANTIC_HOLD_PENDING) : (flashing ? KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING : KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY);
                key_feedback_semantic_map_set(semantic, key_at(key), value);
                if (state == 0) key_feedback_tap_branch_map_set(branch, key_at(key), (uint8_t)branch_value);
                if (lit) key_origin_bitmap_add_keypos(visible, key_at(key));
            }
            for (unsigned slot = 0; slot < KEY_FEEDBACK_BROAD_OWNER_COUNT; slot++) {
                assert(fscanf(file, "%u", &owners[slot]) == 1);
                if (owners[slot] != 255u) key_feedback_broad_owner_map_set(owner, (key_feedback_broad_owner_slot_t)slot, key_at(owners[slot]));
            }
        }
        memset(effect_known, 0, sizeof(effect_known));
        assert(fscanf(file, "%u", &count) == 1);
        for (unsigned entry = 0; entry < count; entry++) {
            unsigned led, colour;
            assert(fscanf(file, "%u %6x", &led, &colour) == 2);
            effect[led]       = (rgb_t){.r = (uint8_t)(colour >> 16), .g = (uint8_t)(colour >> 8), .b = (uint8_t)colour};
            effect_known[led] = true;
        }
        for (unsigned index = 0; index < 4; index++) assert(fscanf(file, "%u", &fade_values[index]) == 1);
        fade_progress  = (uint16_t)fade_values[0];
        fade_timeout   = fade_values[1];
        fade_dead_time = fade_values[2];
        fade_layer     = (uint8_t)fade_values[3];

        read_hex(file, facts.layer_status, sizeof(facts.layer_status));
        read_hex(file, facts.mode_status, sizeof(facts.mode_status));
        read_hex(file, facts.combo_layer, sizeof(facts.combo_layer));
        read_hex(file, facts.behaviour_phase, sizeof(facts.behaviour_phase));
        read_hex(file, facts.macro_playing, sizeof(facts.macro_playing));
        read_hex(file, facts.key_pressed, sizeof(facts.key_pressed));
        read_hex(file, facts.modifier, sizeof(facts.modifier));
        read_hex(file, facts.host_lock, sizeof(facts.host_lock));
        read_hex(file, facts.pointer, sizeof(facts.pointer));
        read_hex(file, facts.idle, sizeof(facts.idle));

        noah_lighting_scene_t        scene = {.program = &program, .facts = &facts, .keymap_generation = keymap_generation};
        noah_lighting_resolve_info_t info;
        memset(&frame, 0, sizeof(frame));
        check_actual_fact_batches(&program, &facts);
        noah_lighting_facts_t original_facts = facts;
        uint32_t reads_before_walk=storage_reads;
        noah_lighting_resolve(&scene, NOAH_LIGHTING_ALL_LEDS, (uint32_t)now, true, &frame, &info);
        assert(storage_reads == reads_before_walk);
        assert(memcmp(&original_facts, &facts, sizeof(facts)) == 0); // fade overlay always restored
        uint16_t live = noah_lighting_test_live_clocks();
        assert(live <= noah_lighting_test_live_bound());
        if (live > peak_clocks) peak_clocks = live;
        for (unsigned led = 0; led < RGB_MATRIX_LED_COUNT; led++) {
            char expected[16], actual[16];
            assert(fscanf(file, "%15s", expected) == 1);
            if (frame.painted & NOAH_LIGHTING_LED_BIT(led)) {
                snprintf(actual, sizeof(actual), "%02x%02x%02x", frame.colour[led].r, frame.colour[led].g, frame.colour[led].b);
            } else {
                snprintf(actual, sizeof(actual), "-");
            }
            if (strcmp(expected, actual) != 0) {
                fprintf(stderr, "lighting render vector %s step %zu LED %u: expected %s, got %s\n", name, steps, led, expected, actual);
                return 1;
            }
        }
        steps++;
    }
    fclose(file);
    assert(cases >= 30 && steps >= 60);
    assert(peak_clocks >= 1000u); // Adversarial distinct signatures, not only duplicate rules.
    printf("compact timing: peak %u, proved live bound %u\n", peak_clocks, noah_lighting_test_live_bound());
    assert(occurrence_cases >= 14 && occurrence_steps >= 30);
    printf("lighting coherent occurrence vectors: %zu cases, %zu steps passed\n", occurrence_cases, occurrence_steps);
    printf("lighting render vectors: %zu cases, %zu steps passed\n", cases, steps);
    return 0;
}
