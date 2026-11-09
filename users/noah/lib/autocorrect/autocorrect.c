// ────────────────────────────────────────────────────────────────────────────
// Autocorrect
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "autocorrect.h"
#include "autocorrect_switch.h"

#include <string.h>

#include "../compat/qmk_autocorrect_setting.h"
#include "../macro/host_layout.h"
#include "../macro/macro_payload.h"
#include "../macro/macro_slot_provider.h"
#include "../macro/profile_macro_provider.h"
#include "../macro/typing_stage.h"
#include "../profile/runtime/active_profile_autocorrect_runtime.h"

// What the stage is waiting for. Anything but idle holds every later key back.
typedef enum {
    AUTOCORRECT_IDLE = 0,
    AUTOCORRECT_LOOKUP,       // The key at the head of the queue ended a word.
    AUTOCORRECT_UNDO_START,   // The Backspace at the head may undo a correction.
    AUTOCORRECT_MACRO_START,  // A macro key's turn has come.
    AUTOCORRECT_PLAYING,      // A correction or its undo is being typed.
    AUTOCORRECT_MACRO_PLAYING // The macro whose turn came is playing.
} autocorrect_state_t;

typedef enum {
    AUTOCORRECT_EDGE_UP = 0,
    AUTOCORRECT_EDGE_MODS,
    AUTOCORRECT_EDGE_DOWN,
    AUTOCORRECT_EDGE_MACRO,
} autocorrect_edge_kind_t;

// One change the host has yet to see: a usage up or down, the modifiers, or a
// macro slot to start.
typedef struct {
    uint8_t kind;
    uint8_t value;
} autocorrect_edge_t;

typedef enum {
    AUTOCORRECT_KEY_NOTHING = 0, // A lock key, or a dead key waiting for its second stroke.
    AUTOCORRECT_KEY_CHARACTER,
    AUTOCORRECT_KEY_BACKSPACE,
    AUTOCORRECT_KEY_ENTER, // Enter or Tab: a word end that prints nothing to take back.
    AUTOCORRECT_KEY_FRESH_START,
    AUTOCORRECT_KEY_SUSPEND,
} autocorrect_key_t;

typedef enum {
    AUTOCORRECT_FORM_AS_WRITTEN = 0,
    AUTOCORRECT_FORM_FIRST_CAPITAL,
    AUTOCORRECT_FORM_CAPITALS,
} autocorrect_form_t;

// The typo found in the word and what replaces it: the matched part of the
// word in characters, and the correction's row.
typedef struct {
    uint16_t correction;
    uint8_t  start;
    uint8_t  length;
    bool     found;
    bool     unchanged; // The word already reads as the correction would leave it.
} autocorrect_choice_t;

enum { AUTOCORRECT_TEXT_MAX = NOAH_AUTOCORRECT_WORD_MAX + NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX };

typedef struct {
    // Usages and modifiers as live ownership last had them, and as the host
    // has been shown them. They differ by the queue and by keys that were
    // down when a correction began, which stay out until released.
    uint8_t            live[32], shown[32];
    uint8_t            live_mods, shown_mods;
    autocorrect_edge_t queue[NOAH_AUTOCORRECT_QUEUE_SIZE];
    uint8_t            head, count;
    // The word being typed, as the host shows it. After a correction it still
    // holds the word as typed, for the undo.
    uint16_t word[NOAH_AUTOCORRECT_WORD_MAX];
    uint8_t  length;
    uint8_t  state;
    uint16_t dead;            // A dead key's stroke, waiting for its second.
    uint16_t lookup_position; // Part-of-word typos compared so far.
    autocorrect_choice_t choice;
    uint8_t  kept, typed;     // Of the corrected word: characters unchanged, characters typed.
    uint8_t  watched;         // The usage whose hold is timed, zero for none.
    uint32_t watched_at;
    bool     synced : 1;      // live and shown describe the reports since the stage last ran.
    bool     resumed : 1;     // The stage has just started reading reports again.
    bool     on : 1;
    bool     allowed : 1;     // The override, as the stage last read it.
    bool     suspended : 1;   // Nothing is corrected until the next word end.
    bool     left_alone : 1;  // This word was put back by an undo.
    bool     lagging : 1;     // A change did not fit the queue and waits in the report.
    bool     head_decided : 1;   // The ending key at the head has had its lookup.
    bool     corrected : 1;      // That word was corrected.
    bool     undo_armed : 1;     // The next key may undo it.
    bool     playing_undo : 1;
    bool     watch_started : 1;
    bool     watched_end : 1;    // The watched key ended a word.
    bool     ending_enter : 1;   // The key that ended the word prints nothing.
} autocorrect_t;

static autocorrect_t autocorrect;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(autocorrect_t) <= NOAH_AUTOCORRECT_EMBEDDED_STATE_BUDGET, "autocorrect state exceeded its firmware regression policy");
#endif
_Static_assert(NOAH_AUTOCORRECT_QUEUE_SIZE <= UINT8_MAX && NOAH_AUTOCORRECT_QUEUE_START_MAX < NOAH_AUTOCORRECT_QUEUE_SIZE, "the queue is indexed in a byte");
_Static_assert(AUTOCORRECT_TEXT_MAX + 1u <= UINT8_MAX, "a corrected word and its ending are counted in a byte");

// ── Characters ──────────────────────────────────────────────────────────────

static bool autocorrect_is_capital(uint16_t character) {
    uint32_t folded;
    return noah_profile_autocorrect_v1_fold(character, &folded) && folded != character;
}

static uint16_t autocorrect_folded(uint16_t character) {
    uint32_t folded;
    return noah_profile_autocorrect_v1_fold(character, &folded) ? (uint16_t)folded : character;
}

static bool autocorrect_capital(uint16_t character, uint16_t *capital) {
    uint32_t found;
    if (!noah_profile_autocorrect_v1_capital(character, &found)) return false;
    *capital = (uint16_t)found;
    return true;
}

static bool autocorrect_is_word_character(uint16_t character) {
    return character == NOAH_PROFILE_AUTOCORRECT_V1_APOSTROPHE || noah_profile_autocorrect_v1_fold(character, NULL);
}

// Caps Lock changes letters only. macOS shows a capital whatever Shift does;
// every other host exchanges the two cases.
static uint16_t autocorrect_under_caps_lock(uint16_t character, bool macos) {
    uint16_t capital;
    if (autocorrect_capital(character, &capital)) return capital;
    return macos ? character : autocorrect_folded(character);
}

// The character whose strokes show `character` on this host now, and how
// many strokes that is; zero when nothing the layout types shows it.
static uint8_t autocorrect_strokes_for(const macro_payload_typing_host_t *host, bool caps_lock, uint16_t character, uint16_t *typed) {
    uint16_t strokes[2];
    uint16_t capital;

    *typed = character;
    if (caps_lock) {
        if (autocorrect_capital(character, &capital)) {
            // A small letter: the other case's strokes, which macOS cannot give.
            if (host->macos) return 0u;
            *typed = capital;
        } else if (autocorrect_is_capital(character) && !host->macos) {
            *typed = autocorrect_folded(character);
        }
    }
    if (!host_layout_lookup((const host_layout_t *)host->layout, *typed, strokes)) return 0u;
    return strokes[1] ? 2u : 1u;
}

// ── The word ────────────────────────────────────────────────────────────────

static void autocorrect_forget(bool suspend) {
    autocorrect.length     = 0u;
    autocorrect.dead       = 0u;
    autocorrect.suspended  = suspend;
    autocorrect.left_alone = false;
    autocorrect.corrected  = false;
    autocorrect.undo_armed = false;
}

// The word without the apostrophes before and after it: what a typo matches.
static uint8_t autocorrect_core(uint8_t *start) {
    uint8_t first = 0u, end = autocorrect.length;
    while (first < end && autocorrect.word[first] == NOAH_PROFILE_AUTOCORRECT_V1_APOSTROPHE)
        first++;
    while (end > first && autocorrect.word[end - 1u] == NOAH_PROFILE_AUTOCORRECT_V1_APOSTROPHE)
        end--;
    *start = first;
    return (uint8_t)(end - first);
}

static bool autocorrect_has_list(void) {
    return noah_active_profile_autocorrect_list()->typos != 0u;
}

// ── Reading keys ────────────────────────────────────────────────────────────

static bool autocorrect_bit(const uint8_t keys[32], uint8_t usage) {
    return (keys[usage / 8u] & (1u << (usage % 8u))) != 0u;
}

static void autocorrect_set_bit(uint8_t keys[32], uint8_t usage, bool on) {
    if (on) keys[usage / 8u] |= (uint8_t)(1u << (usage % 8u));
    else keys[usage / 8u] &= (uint8_t)~(1u << (usage % 8u));
}

// What a key going down types, from the host layout and the modifiers the
// host sees with it.
static autocorrect_key_t autocorrect_read_key(uint8_t usage, uint8_t mods, uint16_t *character) {
    const macro_payload_typing_host_t host   = macro_payload_typing_host();
    const host_layout_t              *layout = (const host_layout_t *)host.layout;
    // Option types characters on macOS from either side; elsewhere only Right
    // Alt is AltGr, and Left Alt makes a shortcut as Ctrl and GUI do.
    uint8_t  altgr    = host.macos ? MOD_BIT(KC_LEFT_ALT) | MOD_BIT(KC_RIGHT_ALT) : MOD_BIT(KC_RIGHT_ALT);
    bool     shortcut = (mods & (uint8_t)~(MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_RIGHT_SHIFT) | altgr)) != 0u;
    uint16_t dead     = autocorrect.dead;
    uint16_t stroke;
    uint8_t  key = usage;

    if (usage == KC_CAPS_LOCK || usage == KC_NUM_LOCK || usage == KC_SCROLL_LOCK) return AUTOCORRECT_KEY_NOTHING;
    autocorrect.dead = 0u;
    if (shortcut) return AUTOCORRECT_KEY_FRESH_START;
    if (usage == KC_BACKSPACE) {
        if (dead) return AUTOCORRECT_KEY_SUSPEND;
        // With Option or AltGr it deletes more than a character.
        return (mods & altgr) ? AUTOCORRECT_KEY_FRESH_START : AUTOCORRECT_KEY_BACKSPACE;
    }
    if (usage == KC_ENTER || usage == KC_KP_ENTER || usage == KC_TAB) return dead ? AUTOCORRECT_KEY_SUSPEND : AUTOCORRECT_KEY_ENTER;

    if (host.iso && key == KC_GRAVE) key = KC_NONUS_BACKSLASH;
    else if (host.iso && key == KC_NONUS_BACKSLASH) key = KC_GRAVE;
    stroke = (uint16_t)(key | ((mods & (MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_RIGHT_SHIFT))) ? HOST_LAYOUT_STROKE_SHIFT : 0u) | ((mods & altgr) ? HOST_LAYOUT_STROKE_ALTGR : 0u));
    if (dead) {
        // A dead key the layout does not resolve leaves the word unknown.
        if (!host_layout_compose(layout, dead, stroke, character)) return AUTOCORRECT_KEY_SUSPEND;
    } else {
        switch (host_layout_stroke_types(layout, stroke, character)) {
            case HOST_LAYOUT_TYPES_CHARACTER:
                break;
            case HOST_LAYOUT_TYPES_DEAD_KEY:
                autocorrect.dead = stroke;
                return AUTOCORRECT_KEY_NOTHING;
            default:
                // The keypad types characters the layouts do not record; any
                // other key moves the cursor, deletes or types nothing.
                return (usage >= KC_KP_SLASH && usage <= KC_KP_DOT) || usage == KC_KP_EQUAL || usage == KC_KP_COMMA ? AUTOCORRECT_KEY_SUSPEND : AUTOCORRECT_KEY_FRESH_START;
        }
    }
    if (macro_payload_unicode_caps_lock()) *character = autocorrect_under_caps_lock(*character, host.macos);
    return AUTOCORRECT_KEY_CHARACTER;
}

static void autocorrect_watch(uint8_t usage, bool ends_word) {
    autocorrect.watched       = usage;
    autocorrect.watch_started = false;
    autocorrect.watched_end   = ends_word;
}

// A word end with nothing left to decide: after a correction that a printing
// key ended, the next key may undo it; otherwise the next word starts fresh.
static void autocorrect_end_word(uint8_t usage) {
    if (autocorrect.corrected && !autocorrect.ending_enter) {
        autocorrect.corrected  = false;
        autocorrect.undo_armed = true;
    } else {
        autocorrect_forget(false);
    }
    autocorrect_watch(usage, true);
}

// A key going down as the host is about to see it. False when it has to wait
// at the head of the queue: it ended a word that may be a typo, or it may undo
// a correction.
static bool autocorrect_key_down(uint8_t usage) {
    uint16_t          character = 0u;
    autocorrect_key_t key;
    uint8_t           start;

    if (!autocorrect.on) {
        autocorrect.head_decided = false;
        return true;
    }
    key = autocorrect_read_key(usage, autocorrect.shown_mods, &character);
    if (autocorrect.undo_armed) {
        if (key == AUTOCORRECT_KEY_BACKSPACE) {
            autocorrect.state = AUTOCORRECT_UNDO_START;
            return false;
        }
        autocorrect_forget(false);
    }
    switch (key) {
        case AUTOCORRECT_KEY_NOTHING:
            return true;
        case AUTOCORRECT_KEY_FRESH_START:
            autocorrect_forget(false);
            return true;
        case AUTOCORRECT_KEY_SUSPEND:
            autocorrect_forget(true);
            return true;
        case AUTOCORRECT_KEY_BACKSPACE:
            // With no character of the word left, it deletes into text the
            // keyboard no longer knows.
            if (!autocorrect.suspended) {
                if (autocorrect.length) autocorrect.length--;
                else autocorrect_forget(true);
            }
            autocorrect_watch(usage, false);
            return true;
        case AUTOCORRECT_KEY_CHARACTER:
            if (autocorrect_is_word_character(character)) {
                if (!autocorrect.suspended) {
                    if (autocorrect.length == NOAH_AUTOCORRECT_WORD_MAX) autocorrect_forget(true);
                    else autocorrect.word[autocorrect.length++] = character;
                }
                autocorrect_watch(usage, false);
                return true;
            }
            break;
        case AUTOCORRECT_KEY_ENTER:
        default:
            break;
    }
    // The word ended.
    if (autocorrect.head_decided) {
        autocorrect.head_decided = false;
    } else if (autocorrect.allowed && !autocorrect.suspended && !autocorrect.left_alone && autocorrect_core(&start) != 0u) {
        autocorrect.ending_enter    = key == AUTOCORRECT_KEY_ENTER;
        autocorrect.lookup_position = 0u;
        autocorrect.choice.found    = false;
        autocorrect.state           = AUTOCORRECT_LOOKUP;
        return false;
    }
    autocorrect_end_word(usage);
    return true;
}

// ── The queue ───────────────────────────────────────────────────────────────

static bool autocorrect_push(uint8_t kind, uint8_t value) {
    if (autocorrect.count == NOAH_AUTOCORRECT_QUEUE_SIZE) return false;
    autocorrect.queue[(uint8_t)(autocorrect.head + autocorrect.count) % NOAH_AUTOCORRECT_QUEUE_SIZE] = (autocorrect_edge_t){.kind = kind, .value = value};
    autocorrect.count++;
    return true;
}

static void autocorrect_pop(void) {
    autocorrect.head = (uint8_t)((autocorrect.head + 1u) % NOAH_AUTOCORRECT_QUEUE_SIZE);
    autocorrect.count--;
}

// Shows the host the next change at the head of the queue, unless it has to
// wait: one key change a scan, with the modifier changes before it, so a
// press and its release never share a report. True when what the host is
// shown changed.
static __attribute__((noinline)) bool autocorrect_pump(void) {
    bool changed = false;

    while (autocorrect.count && autocorrect.state == AUTOCORRECT_IDLE) {
        autocorrect_edge_t edge = autocorrect.queue[autocorrect.head];

        switch (edge.kind) {
            case AUTOCORRECT_EDGE_MODS:
                autocorrect.shown_mods = edge.value;
                break;
            case AUTOCORRECT_EDGE_UP:
                autocorrect_set_bit(autocorrect.shown, edge.value, false);
                break;
            case AUTOCORRECT_EDGE_DOWN:
                if (!autocorrect_key_down(edge.value)) return changed;
                autocorrect_set_bit(autocorrect.shown, edge.value, true);
                break;
            default:
                autocorrect.state = AUTOCORRECT_MACRO_START;
                return changed;
        }
        autocorrect_pop();
        changed = true;
        if (edge.kind != AUTOCORRECT_EDGE_MODS) break;
    }
    return changed;
}

// A change that does not fit stays in the report and is read again at the
// next one, so it is late and never lost; a correction being typed stops, so
// the queue can empty.
static bool autocorrect_queue_change(uint8_t kind, uint8_t value) {
    if (autocorrect_push(kind, value)) return true;
    autocorrect.lagging = true;
    if (autocorrect.state == AUTOCORRECT_PLAYING) (void)macro_payload_engine_cancel();
    return false;
}

// ── The typing stage ────────────────────────────────────────────────────────

// The stage follows words whenever the profile has autocorrect on and a list,
// whatever the override says, so switching back on by key finds it knowing
// where the next word starts.
static bool autocorrect_wanted(void) {
    return noah_qmk_autocorrect_stored() && autocorrect_has_list();
}

static bool autocorrect_holding(void) {
    return autocorrect.state != AUTOCORRECT_IDLE || autocorrect.count != 0u;
}

static bool autocorrect_settled(void) {
    return !autocorrect_holding() && !autocorrect.lagging && autocorrect.live_mods == autocorrect.shown_mods && memcmp(autocorrect.live, autocorrect.shown, sizeof(autocorrect.live)) == 0;
}

bool noah_typing_stage_active(void) {
    if (autocorrect_wanted() || (autocorrect.synced && !autocorrect_settled())) return true;
    // This is asked for every report: one is passing unread, so what the
    // stage holds no longer describes what the host sees.
    autocorrect.synced = false;
    autocorrect.on     = false;
    return false;
}

// The report hook only records: it reads what changed since the report
// before, queues it and answers with what the host has been shown so far.
// The scan reads the queue as typing and shows it, a change at a time. This
// runs deep inside key processing, on the reviewed stack paths, so it calls
// nothing that looks anything up.
void noah_typing_stage_keys(uint8_t keys[32], const uint8_t macro_keys[32], uint8_t macro_mods) {
    uint8_t mods   = (uint8_t)(get_mods() | get_weak_mods() | get_oneshot_mods());
    bool    typing = false;

    // The person's own keys: what a macro holds is not theirs, and while it
    // holds a usage or modifier theirs stays as it last was.
    mods = (uint8_t)((mods & ~macro_mods) | (autocorrect.live_mods & macro_mods));
    if (!autocorrect.synced) {
        // Reports went out unread: the keyboard does not know where the word
        // on screen began. Every key now down is read as going down, which
        // shows the host nothing new and misses no key that ends a word.
        memset(autocorrect.live, 0, sizeof(autocorrect.live));
        memset(autocorrect.shown, 0, sizeof(autocorrect.shown));
        autocorrect.live_mods = autocorrect.shown_mods = mods;
        autocorrect.synced                             = true;
        autocorrect.resumed                            = true;
    }
    autocorrect.lagging = false;
    for (uint8_t index = 0u; index < 32u; index++) {
        keys[index] = (uint8_t)((keys[index] & ~macro_keys[index]) | (autocorrect.live[index] & macro_keys[index]));
        typing      = typing || keys[index] != 0u;
        for (uint8_t released = (uint8_t)(autocorrect.live[index] & ~keys[index]), bit = 0u; released && bit < 8u && !autocorrect.lagging; bit++) {
            if ((released & (1u << bit)) && autocorrect_queue_change(AUTOCORRECT_EDGE_UP, (uint8_t)(index * 8u + bit))) autocorrect.live[index] &= (uint8_t)~(1u << bit);
        }
    }
    if (!autocorrect.lagging && mods != autocorrect.live_mods && autocorrect_queue_change(AUTOCORRECT_EDGE_MODS, mods)) autocorrect.live_mods = mods;
    for (uint8_t index = 0u; index < 32u && !autocorrect.lagging; index++) {
        for (uint8_t pressed = (uint8_t)(keys[index] & ~autocorrect.live[index]), bit = 0u; pressed && bit < 8u && !autocorrect.lagging; bit++) {
            if ((pressed & (1u << bit)) && autocorrect_queue_change(AUTOCORRECT_EDGE_DOWN, (uint8_t)(index * 8u + bit))) autocorrect.live[index] |= (uint8_t)(1u << bit);
        }
    }
    memcpy(keys, autocorrect.shown, sizeof(autocorrect.shown));
    // QMK spends a one-shot modifier on the report that carries a key; while
    // the stage supplies the modifiers it spends it here, where the key was
    // read with it.
    if (typing && autocorrect_holding() && get_oneshot_mods()) clear_oneshot_mods();
}

bool noah_typing_stage_mods(uint8_t *mods) {
    if (!autocorrect.synced || !autocorrect_holding()) return false;
    *mods = autocorrect.shown_mods;
    return true;
}

bool noah_typing_stage_defer_macro(uint8_t slot) {
    return autocorrect.synced && autocorrect_holding() && autocorrect_push(AUTOCORRECT_EDGE_MACRO, slot);
}

void noah_typing_stage_note_click(void) {
    autocorrect_forget(false);
}

// ── Corrections ─────────────────────────────────────────────────────────────

// Held-back keys come out again, from the next pass of the scan.
static void autocorrect_release(void) {
    autocorrect.state = AUTOCORRECT_IDLE;
}

static void autocorrect_playback_finished(macro_payload_finish_result_t result, void *context) {
    (void)context;
    if (autocorrect.playing_undo) {
        // The word is on screen as it was typed, and stays that way.
        autocorrect.left_alone = true;
    } else {
        autocorrect.head_decided = true;
        // A correction that did not finish leaves a word nothing can undo.
        autocorrect.corrected = result == MACRO_PAYLOAD_FINISH_SUCCESS;
    }
    autocorrect.playing_undo = false;
    autocorrect_release();
}

// Types `count` characters after erasing `erase`. The keys the host saw down
// go up with the playback's neutral report, and stay out until released.
static bool autocorrect_play(uint8_t erase, const uint16_t *characters, uint8_t count) {
    if (macro_slot_provider_start_correction(erase, characters, count, NOAH_AUTOCORRECT_PACE_MS, autocorrect_playback_finished) != MACRO_PAYLOAD_START_STARTED) return false;
    memset(autocorrect.shown, 0, sizeof(autocorrect.shown));
    autocorrect.watched = 0u;
    autocorrect.state   = AUTOCORRECT_PLAYING;
    return true;
}

// The case the correction takes, judged on the typo as the host shows it.
static autocorrect_form_t autocorrect_form(const uint16_t *typed, uint8_t length, const uint16_t *correction, uint8_t correction_length) {
    uint8_t  capitals = 0u, smalls = 0u;
    uint16_t capital;

    for (uint8_t index = 0u; index < length; index++) {
        if (autocorrect_is_capital(typed[index])) capitals++;
        else if (autocorrect_capital(typed[index], &capital)) smalls++;
    }
    if (capitals >= 2u && smalls == 0u) return AUTOCORRECT_FORM_CAPITALS;
    if (capitals != 1u || !autocorrect_is_capital(typed[0])) return AUTOCORRECT_FORM_AS_WRITTEN;
    // A correction written with capitals keeps exactly those.
    for (uint8_t index = 0u; index < correction_length; index++)
        if (autocorrect_is_capital(correction[index])) return AUTOCORRECT_FORM_AS_WRITTEN;
    return AUTOCORRECT_FORM_FIRST_CAPITAL;
}

static uint8_t autocorrect_decode(const uint8_t *bytes, uint8_t length, uint16_t characters[NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX]) {
    uint8_t count = 0u;

    for (uint8_t index = 0u; index < length; count++) {
        uint8_t  first = bytes[index];
        uint8_t  width = first < 0x80u ? 1u : first < 0xE0u ? 2u : first < 0xF0u ? 3u : 4u;
        uint32_t value = first & (width == 1u ? 0x7Fu : width == 2u ? 0x1Fu : width == 3u ? 0x0Fu : 0x07u);
        for (uint8_t more = 1u; more < width && index + more < length; more++)
            value = (value << 6u) | (bytes[index + more] & 0x3Fu);
        // No layout types a character outside the Basic Multilingual Plane;
        // U+FFFF is none, so such a correction is one nothing can type.
        characters[count] = value > 0xFFFFu ? 0xFFFFu : (uint16_t)value;
        index             = (uint8_t)(index + width);
    }
    return count;
}

// The word as the correction leaves it: what stays before the typo, the
// correction in its case, what follows the typo. `kept` characters at its
// start are already on screen; `kept` equal to both lengths means the word
// already reads that way. False when this host cannot type the rest, or it is
// more strokes than a correction types.
static bool autocorrect_plan(const autocorrect_choice_t *choice, autocorrect_form_t form, const uint16_t *correction, uint8_t correction_length, uint16_t text[AUTOCORRECT_TEXT_MAX], uint8_t *length, uint8_t *kept) {
    const macro_payload_typing_host_t host      = macro_payload_typing_host();
    bool                              caps_lock = macro_payload_unicode_caps_lock();
    uint8_t                           count     = 0u, same = 0u, strokes;
    uint8_t                           after     = (uint8_t)(choice->start + choice->length);
    uint16_t                          capital, typed;

    for (uint8_t index = 0u; index < choice->start; index++)
        text[count++] = autocorrect.word[index];
    for (uint8_t index = 0u; index < correction_length; index++) {
        uint16_t character = correction[index];
        if ((form == AUTOCORRECT_FORM_CAPITALS || (form == AUTOCORRECT_FORM_FIRST_CAPITAL && index == 0u)) && autocorrect_capital(character, &capital)) character = capital;
        text[count++] = character;
    }
    for (uint8_t index = after; index < autocorrect.length; index++)
        text[count++] = autocorrect.word[index];

    while (same < count && same < autocorrect.length && text[same] == autocorrect.word[same])
        same++;
    strokes = (uint8_t)(autocorrect.length - same);
    for (uint8_t index = same; index < count; index++) {
        uint8_t needed = autocorrect_strokes_for(&host, caps_lock, text[index], &typed);
        if (!needed || strokes + needed > NOAH_AUTOCORRECT_STROKES_MAX) return false;
        strokes = (uint8_t)(strokes + needed);
    }
    *length = count;
    *kept   = same;
    return true;
}

// The characters to send for `characters` to show, under this host's Caps Lock.
static void autocorrect_to_strokes(uint16_t *characters, uint8_t count) {
    const macro_payload_typing_host_t host      = macro_payload_typing_host();
    bool                              caps_lock = macro_payload_unicode_caps_lock();

    for (uint8_t index = 0u; index < count; index++)
        (void)autocorrect_strokes_for(&host, caps_lock, characters[index], &characters[index]);
}

// Whether the chosen typo's correction can be typed, in the case the typo
// was typed in or else as written, and the text it makes.
static bool autocorrect_correction_text(const autocorrect_choice_t *choice, uint16_t text[AUTOCORRECT_TEXT_MAX], uint8_t *length, uint8_t *kept) {
    uint8_t            bytes[NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX];
    uint16_t           correction[NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX];
    uint8_t            size = 0u, count;
    autocorrect_form_t form;

    if (!noah_profile_autocorrect_v1_correction(noah_active_profile_autocorrect_list(), choice->correction, bytes, &size)) return false;
    count = autocorrect_decode(bytes, size, correction);
    form  = autocorrect_form(&autocorrect.word[choice->start], choice->length, correction, count);
    if (autocorrect_plan(choice, form, correction, count, text, length, kept)) return true;
    return form != AUTOCORRECT_FORM_AS_WRITTEN && autocorrect_plan(choice, AUTOCORRECT_FORM_AS_WRITTEN, correction, count, text, length, kept);
}

static __attribute__((noinline)) void autocorrect_start_correction(void) {
    uint16_t text[AUTOCORRECT_TEXT_MAX];
    uint8_t  length = 0u, kept = 0u;

    // With too much already waiting, the typo is left as typed rather than
    // risking a key that does not fit the queue.
    if (autocorrect.choice.found && !autocorrect.choice.unchanged && autocorrect.count <= NOAH_AUTOCORRECT_QUEUE_START_MAX && autocorrect_correction_text(&autocorrect.choice, text, &length, &kept)) {
        autocorrect_to_strokes(&text[kept], (uint8_t)(length - kept));
        if (autocorrect_play((uint8_t)(autocorrect.length - kept), &text[kept], (uint8_t)(length - kept))) {
            autocorrect.kept  = kept;
            autocorrect.typed = (uint8_t)(length - kept);
            return;
        }
    }
    autocorrect.head_decided = true;
    autocorrect_release();
}

// A candidate takes the word's choice when the keyboard can type it.
static void autocorrect_consider(uint16_t correction, uint8_t start, uint8_t length) {
    autocorrect_choice_t candidate = {.correction = correction, .start = start, .length = length, .found = true};
    uint16_t             text[AUTOCORRECT_TEXT_MAX];
    uint8_t              count, kept;

    if (!autocorrect_correction_text(&candidate, text, &count, &kept)) return;
    candidate.unchanged = kept == count && kept == autocorrect.length;
    autocorrect.choice  = candidate;
}

// Looks the finished word up: the whole word first, then the part-of-word
// typos a bounded number a scan. The longest typo wins, and of equal ones the
// first in the list; a whole-word typo beats them all. Its workspace, like
// the other steps' below, stays out of the scan's own frame, which every
// path through the matrix scan stands on.
static __attribute__((noinline)) void autocorrect_lookup(void) {
    const noah_profile_autocorrect_v1_list_t *list = noah_active_profile_autocorrect_list();
    noah_profile_autocorrect_v1_typo_t        typo;
    uint16_t                                  folded[NOAH_AUTOCORRECT_WORD_MAX], part[NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX];
    uint8_t                                   bytes[NOAH_PROFILE_AUTOCORRECT_V1_TEXT_MAX];
    uint8_t                                   start, length = autocorrect_core(&start);
    uint16_t                                  end;

    if (!length || autocorrect.suspended) {
        autocorrect.choice.found = false;
        autocorrect_start_correction();
        return;
    }
    for (uint8_t index = 0u; index < length; index++)
        folded[index] = autocorrect_folded(autocorrect.word[start + index]);

    if (autocorrect.lookup_position == 0u) {
        uint8_t size = 0u;
        for (uint8_t index = 0u; index < length && size <= sizeof(bytes); index++) {
            uint16_t character = folded[index];
            uint8_t  width     = character < 0x80u ? 1u : character < 0x800u ? 2u : 3u;
            if (size + width > sizeof(bytes)) {
                size = sizeof(bytes) + 1u;
                break;
            }
            if (width == 1u) {
                bytes[size++] = (uint8_t)character;
            } else if (width == 2u) {
                bytes[size++] = (uint8_t)(0xC0u | (character >> 6u));
                bytes[size++] = (uint8_t)(0x80u | (character & 0x3Fu));
            } else {
                bytes[size++] = (uint8_t)(0xE0u | (character >> 12u));
                bytes[size++] = (uint8_t)(0x80u | ((character >> 6u) & 0x3Fu));
                bytes[size++] = (uint8_t)(0x80u | (character & 0x3Fu));
            }
        }
        if (size <= sizeof(bytes) && noah_profile_autocorrect_v1_find(list, bytes, size, &typo) && typo.match == NOAH_PROFILE_AUTOCORRECT_V1_MATCH_WHOLE_WORD) {
            autocorrect_consider(typo.correction, start, length);
            if (autocorrect.choice.found) {
                autocorrect_start_correction();
                return;
            }
        }
    }

    end = list->part_typos - autocorrect.lookup_position > NOAH_AUTOCORRECT_LOOKUP_STEP ? (uint16_t)(autocorrect.lookup_position + NOAH_AUTOCORRECT_LOOKUP_STEP) : list->part_typos;
    for (; autocorrect.lookup_position < end; autocorrect.lookup_position++) {
        uint8_t count, at;

        if (!noah_profile_autocorrect_v1_part_at(list, autocorrect.lookup_position, &typo, bytes)) continue;
        count = autocorrect_decode(bytes, typo.length, part);
        if (count > length || (autocorrect.choice.found && count <= autocorrect.choice.length)) continue;
        // Where in the word this typo may sit: its start, its end, or the
        // first place it is found.
        at = typo.match == NOAH_PROFILE_AUTOCORRECT_V1_MATCH_END ? (uint8_t)(length - count) : 0u;
        for (uint8_t last = typo.match == NOAH_PROFILE_AUTOCORRECT_V1_MATCH_ANYWHERE ? (uint8_t)(length - count) : at; at <= last; at++) {
            if (memcmp(&folded[at], part, count * sizeof(part[0])) != 0) continue;
            autocorrect_consider(typo.correction, (uint8_t)(start + at), count);
            break;
        }
    }
    if (autocorrect.lookup_position >= list->part_typos) autocorrect_start_correction();
}

// Backspace right after a correction: the ending character and what the
// correction typed go, and the word comes back as it was typed. The
// Backspace itself never reaches the host. When this host cannot type the
// word back, it is an ordinary Backspace.
static __attribute__((noinline)) void autocorrect_start_undo(void) {
    const macro_payload_typing_host_t host      = macro_payload_typing_host();
    bool                              caps_lock = macro_payload_unicode_caps_lock();
    uint16_t                          text[NOAH_AUTOCORRECT_WORD_MAX];
    uint8_t                           count = (uint8_t)(autocorrect.length - autocorrect.kept);
    bool                              typeable = true;

    for (uint8_t index = 0u; index < count; index++)
        typeable = typeable && autocorrect_strokes_for(&host, caps_lock, autocorrect.word[autocorrect.kept + index], &text[index]) != 0u;
    autocorrect.undo_armed = false;
    if (typeable && autocorrect_play((uint8_t)(autocorrect.typed + 1u), text, count)) {
        autocorrect.playing_undo = true;
        autocorrect_pop();
        return;
    }
    autocorrect_forget(false);
    autocorrect_release();
}

static void autocorrect_start_macro(void) {
    uint8_t slot = autocorrect.queue[autocorrect.head].value;

    autocorrect_pop();
    autocorrect_forget(false);
    if (noah_profile_macro_start((uint16_t)(QK_MACRO + slot))) autocorrect.state = AUTOCORRECT_MACRO_PLAYING;
    else autocorrect_release();
}

// A key held until the host repeats it types characters the keyboard does not
// see. After a word end that only costs the undo; inside a word, the word.
static void autocorrect_watch_hold(uint32_t now) {
    if (!autocorrect_bit(autocorrect.shown, autocorrect.watched)) {
        autocorrect.watched = 0u;
    } else if (!autocorrect.watch_started) {
        autocorrect.watch_started = true;
        autocorrect.watched_at    = now;
    } else if (now - autocorrect.watched_at >= NOAH_AUTOCORRECT_HOLD_MS) {
        autocorrect.watched = 0u;
        autocorrect_forget(!autocorrect.watched_end);
    }
}

// What the switches say now, read from the scan so a change takes effect
// between two keys and never in the middle of one.
static void autocorrect_read_switches(void) {
    bool wanted = autocorrect_wanted();

    if (autocorrect.resumed) {
        autocorrect.resumed = false;
        autocorrect_forget(true);
    }
    if (wanted != autocorrect.on) {
        autocorrect.on = wanted;
        if (!wanted) autocorrect_forget(false);
    }
    if (autocorrect_switch_override() != autocorrect.allowed) {
        // Switched by key in the middle of a word, that word is left alone.
        autocorrect.allowed = !autocorrect.allowed;
        if (autocorrect.length) autocorrect_forget(true);
        autocorrect.undo_armed = false;
    }
    // A macro's text is never typing: whatever it types, the next word
    // starts fresh.
    if (autocorrect.state == AUTOCORRECT_IDLE && !macro_payload_engine_idle()) autocorrect_forget(false);
}

void noah_autocorrect_scan(void) {
    bool changed = false;

    if (!autocorrect.synced) return;
    autocorrect_read_switches();
    switch (autocorrect.state) {
        case AUTOCORRECT_LOOKUP:
            autocorrect_lookup();
            break;
        case AUTOCORRECT_UNDO_START:
            autocorrect_start_undo();
            break;
        case AUTOCORRECT_MACRO_START:
            autocorrect_start_macro();
            break;
        case AUTOCORRECT_MACRO_PLAYING:
            if (macro_payload_engine_idle()) autocorrect_release();
            break;
        default:
            break;
    }
    if (autocorrect.state != AUTOCORRECT_IDLE) return;
    if (autocorrect.watched) autocorrect_watch_hold(timer_read32());
    changed = autocorrect_pump();
    // A change waiting for room in the queue is read again from the report.
    if (changed || autocorrect.lagging) send_keyboard_report();
}

void noah_autocorrect_init(void) {
    // No report has gone out: the host has no key down and no word begun.
    autocorrect = (autocorrect_t){.synced = true, .allowed = true};
}
