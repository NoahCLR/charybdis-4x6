#include <assert.h>
#include <stdio.h>
#include "key_runtime_scenario_harness.h"
#include "users/noah/lib/rgb/lighting/lighting_events.h"
#include "users/noah/noah_keymap_ids.h"
#include "users/noah/lib/key/runtime/queue/pending_release_queue.h"
#include "users/noah/lib/key/runtime/deferred_release.h"
#include "users/noah/lib/key/runtime/transition.h"
#include "users/noah/lib/key/runtime/reducer/state_query.h"

static noah_lighting_event_t events[16];
static unsigned count;
void noah_lighting_event_observe(const noah_lighting_event_t *event) {
    assert(count < ARRAY_SIZE(events));
    assert(!event->owners); // authored behaviour subjects own their physical key
    events[count++] = *event;
}
static key_behavior_view_t define_key(uint16_t keycode) {
    key_behavior_view_t view = key_runtime_scenario_pressable_handled_key(keycode);
    view.lighting_identity = 128u; // actual cached authored row127, not native keycode
    view.tap_hold_term = 150;
    view.longer_hold_term = 350;
    view.multi_tap_term = 120;
    return view;
}
static void reset(void) {
    key_runtime_scenario_reset();
    key_runtime_scenario_set_tap_commit_mode(KEY_FEEDBACK_TAP_COMMIT_OFF);
    count = 0;
}
static void expect(unsigned at, uint8_t kind, uint8_t branch, uint8_t slot) {
    assert(at < count);
    assert(events[at].kind == kind && events[at].thing == 127u);
    assert(events[at].branch == branch && events[at].slot == slot);
    assert(events[at].key.row == 0u && events[at].key.col == 1u);
}
static void tap_without_visual_feedback(void) {
    reset();
    key_behavior_view_t view = define_key(CUSTOM_KEY_0 + 50u);
    view.single.tap = (tap_behavior_t)TAP_SENDS(KC_A);
    key_runtime_scenario_add_behavior_view(view);
    const key_runtime_scenario_step_t steps[] = {
        KEY_RUNTIME_SCENARIO_PRESS(CUSTOM_KEY_0 + 50u, 0, 1),
        KEY_RUNTIME_SCENARIO_RELEASE(CUSTOM_KEY_0 + 50u, 0, 1),
    };
    key_runtime_scenario_run(steps, ARRAY_SIZE(steps));
    assert(count == 1u);
    expect(0, NOAH_LIGHTING_EVENT_TAP_FIRED, 1u, 2u);
    assert(key_runtime_scenario_effect_count() == 1u); // actual dispatch, no Standard pulse
}
static void branch_two_commits_once(void) {
    reset();
    key_behavior_view_t view = define_key(CUSTOM_KEY_0 + 56u);
    view.single.tap = (tap_behavior_t)TAP_SENDS(KC_A);
    view.has_multi_tap = true;
    view.authored_tap_depth = 2u;
    const key_runtime_scenario_multi_tap_entry_t entry = {.tap_count = 2u, .step.tap = TAP_SENDS(KC_B)};
    key_runtime_scenario_add_pending_multi_tap_behavior(view, &entry, 1u);
    const key_runtime_scenario_step_t steps[] = {
        KEY_RUNTIME_SCENARIO_PRESS(CUSTOM_KEY_0 + 56u, 0, 1), KEY_RUNTIME_SCENARIO_RELEASE(CUSTOM_KEY_0 + 56u, 0, 1),
        KEY_RUNTIME_SCENARIO_ADVANCE(40),
        KEY_RUNTIME_SCENARIO_PRESS(CUSTOM_KEY_0 + 56u, 0, 1), KEY_RUNTIME_SCENARIO_RELEASE(CUSTOM_KEY_0 + 56u, 0, 1),
        KEY_RUNTIME_SCENARIO_ADVANCE(121), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_SCAN(),
    };
    key_runtime_scenario_run(steps, ARRAY_SIZE(steps));
    assert(count == 1u);
    expect(0, NOAH_LIGHTING_EVENT_TAP_FIRED, 2u, 2u);
}
static void unnumbered_intrinsic_gesture_has_no_authored_event(void) {
    reset();
    key_behavior_view_t view = define_key(CUSTOM_KEY_0 + 57u);
    view.lighting_identity = 0u;
    view.single.tap = (tap_behavior_t)TAP_SENDS(KC_A);
    key_runtime_scenario_add_behavior_view(view);
    const key_runtime_scenario_step_t steps[] = {
        KEY_RUNTIME_SCENARIO_PRESS(CUSTOM_KEY_0 + 57u, 0, 1), KEY_RUNTIME_SCENARIO_RELEASE(CUSTOM_KEY_0 + 57u, 0, 1),
    };
    key_runtime_scenario_run(steps, ARRAY_SIZE(steps));
    assert(count == 0u);
}
static void buffered_base_and_inherited_taps_are_real_commits(void) {
    for (uint8_t branch = 1u; branch <= 2u; branch++) {
        reset();
        key_behavior_view_t view = define_key(KC_LEFT_GUI);
        view.single.tap = (tap_behavior_t)TAP_SENDS(KC_A);
        view.has_multi_tap = true;
        view.authored_tap_depth = 2u;
        // Branch2 inherits the native modifier action (not branch1's
        // explicit KC_A). Its authored hold has no visual tap pulse.
        const key_runtime_scenario_multi_tap_entry_t entry = {.tap_count = 2u, .step.hold = TAP_AT_HOLD_THRESHOLD(KC_B)};
        key_runtime_scenario_add_pending_multi_tap_behavior(view, &entry, 1u);
        const key_runtime_scenario_step_t first[] = {KEY_RUNTIME_SCENARIO_PRESS(KC_LEFT_GUI, 0, 1), KEY_RUNTIME_SCENARIO_RELEASE(KC_LEFT_GUI, 0, 1)};
        key_runtime_scenario_run(first, ARRAY_SIZE(first));
        if (branch == 2u) {
            const key_runtime_scenario_step_t second[] = {KEY_RUNTIME_SCENARIO_ADVANCE(40), KEY_RUNTIME_SCENARIO_PRESS(KC_LEFT_GUI, 0, 1), KEY_RUNTIME_SCENARIO_RELEASE(KC_LEFT_GUI, 0, 1)};
            key_runtime_scenario_run(second, ARRAY_SIZE(second));
        }
        assert(count == 0u);
        const tap_series_t *series = key_runtime_core_tap_series_at((keypos_t){.row = 0u, .col = 1u});
        assert(series && series->active && series->lighting_identity == 128u);
        const key_runtime_scenario_step_t flush[] = {KEY_RUNTIME_SCENARIO_ADVANCE(121), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_SCAN()};
        key_runtime_scenario_run(flush, ARRAY_SIZE(flush));
        if (count != 1u) {
            fprintf(stderr, "buffered branch%u: events%u effects%u\n", branch, count, key_runtime_scenario_effect_count());
            for (unsigned i = 0; i < key_runtime_scenario_effect_count(); i++) fprintf(stderr, "effect%u kind%u action%04x\n", i, key_runtime_scenario_effect_at(i)->kind, key_runtime_scenario_effect_at(i)->data.action);
        }
        assert(count == 1u);
        expect(0, NOAH_LIGHTING_EVENT_TAP_FIRED, branch, 2u);
        for (unsigned i = 0; i < key_runtime_scenario_effect_count(); i++) assert(key_runtime_scenario_effect_at(i)->kind != KEY_RUNTIME_EFFECT_FEEDBACK_PULSE);
    }
}
static void hold_then_long_hold(void) {
    reset();
    key_behavior_view_t view = define_key(CUSTOM_KEY_0 + 51u);
    view.single.tap = (tap_behavior_t)TAP_SENDS(KC_A);
    view.single.hold = (hold_behavior_t)PRESS_AND_HOLD_UNTIL_RELEASE(KC_B);
    view.single.long_hold = (hold_behavior_t)TAP_AT_HOLD_THRESHOLD(KC_C);
    key_runtime_scenario_add_behavior_view(view);
    const key_runtime_scenario_step_t steps[] = {
        KEY_RUNTIME_SCENARIO_PRESS(CUSTOM_KEY_0 + 51u, 0, 1),
        KEY_RUNTIME_SCENARIO_ADVANCE(151), KEY_RUNTIME_SCENARIO_SCAN(),
        KEY_RUNTIME_SCENARIO_ADVANCE(200), KEY_RUNTIME_SCENARIO_SCAN(),
        KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_RELEASE(CUSTOM_KEY_0 + 51u, 0, 1),
    };
    key_runtime_scenario_run(steps, ARRAY_SIZE(steps));
    assert(count == 2u);
    expect(0, NOAH_LIGHTING_EVENT_HOLD_FIRED, 1u, 0u);
    expect(1, NOAH_LIGHTING_EVENT_LONG_HOLD_FIRED, 1u, 1u);
}
static void hold_fires_on_actual_release(void) {
    reset();
    key_behavior_view_t view = define_key(CUSTOM_KEY_0 + 52u);
    view.single.tap = (tap_behavior_t)TAP_SENDS(KC_A);
    view.single.hold = (hold_behavior_t)TAP_ON_RELEASE_AFTER_HOLD(KC_B);
    key_runtime_scenario_add_behavior_view(view);
    const key_runtime_scenario_step_t before[] = {
        KEY_RUNTIME_SCENARIO_PRESS(CUSTOM_KEY_0 + 52u, 0, 1),
        KEY_RUNTIME_SCENARIO_ADVANCE(151), KEY_RUNTIME_SCENARIO_SCAN(),
    };
    key_runtime_scenario_run(before, ARRAY_SIZE(before));
    assert(count == 0u); // pending hold is a state, not a fired action
    const key_runtime_scenario_step_t release[] = {KEY_RUNTIME_SCENARIO_RELEASE(CUSTOM_KEY_0 + 52u, 0, 1)};
    key_runtime_scenario_run(release, ARRAY_SIZE(release));
    assert(count == 1u);
    expect(0, NOAH_LIGHTING_EVENT_HOLD_FIRED, 1u, 0u);
}
static void layer_hold_captures_outcome_without_standard_pulse(void) {
    reset();
    key_behavior_view_t view = define_key(CUSTOM_KEY_0 + 58u);
    view.single.tap = (tap_behavior_t)TAP_SENDS(KC_A);
    view.single.hold = (hold_behavior_t)PRESS_AND_HOLD_UNTIL_RELEASE(MO(2));
    key_runtime_scenario_add_behavior_view(view);
    const key_runtime_scenario_step_t steps[] = {
        KEY_RUNTIME_SCENARIO_PRESS(CUSTOM_KEY_0 + 58u, 0, 1),
        KEY_RUNTIME_SCENARIO_ADVANCE(151), KEY_RUNTIME_SCENARIO_SCAN(), KEY_RUNTIME_SCENARIO_SCAN(),
    };
    key_runtime_scenario_run(steps, ARRAY_SIZE(steps));
    assert(count == 1u);
    expect(0, NOAH_LIGHTING_EVENT_HOLD_FIRED, 1u, 0u);
    assert(events[0].outcome == 3u); // actual resolved layer2 encoding
}
static void pending_commit_keeps_old_row_and_branch(void) {
    reset();
    keypos_t key = {.row = 0, .col = 1};
    assert(key_runtime_core_queue_pending_release_dispatch_with_commit(key, KC_A, (keyboard_mod_state_t){0}, false, 0u, 128u, KEY_BEHAVIOR_MAX_TAP_COUNT));
    pending_release_t pending;
    assert(key_runtime_core_pending_release_at_order(0u, &pending));
    assert(pending.lighting_identity == 128u && pending.lighting_branch == KEY_BEHAVIOR_MAX_TAP_COUNT);
    // Current authored identity is deliberately different. The queue's data
    // comes from the commit, rather than looking up the current key binding.
    key_behavior_view_t view = define_key(CUSTOM_KEY_0 + 53u);
    view.lighting_identity = 5u;
    view.single.tap = (tap_behavior_t)TAP_SENDS(KC_B);
    key_runtime_scenario_add_behavior_view(view);
    key_runtime_deferred_release_drain_dispatches();
    assert(count == 1u);
    expect(0, NOAH_LIGHTING_EVENT_TAP_FIRED, KEY_BEHAVIOR_MAX_TAP_COUNT, 2u);
    assert(key_runtime_core_pending_release_count() == 0u);
    key_runtime_deferred_release_drain_dispatches();
    assert(count == 1u);
}
static void old_series_commit_survives_streaming_boundary_and_new_row(void) {
    reset();
    keypos_t key = {.row = 0u, .col = 1u};
    key_behavior_view_t view = define_key(CUSTOM_KEY_0 + 54u);
    view.lighting_identity = 5u;
    view.single.tap = (tap_behavior_t)TAP_SENDS(KC_B);
    key_runtime_scenario_add_behavior_view(view);
    const key_runtime_scenario_step_t press[] = {KEY_RUNTIME_SCENARIO_PRESS(CUSTOM_KEY_0 + 54u, 0, 1)};
    key_runtime_scenario_run(press, ARRAY_SIZE(press));
    // The reducer's old series and new press are distinct captured sources.
    // Exercise their actual streaming planner/projector seam at the last slot.
    key_runtime_core_state_t *state = key_runtime_core_state();
    state->tap_series[1] = (tap_series_t){
        .active = true, .keycode = CUSTOM_KEY_0 + 55u, .tap_count = 2u,
        .tap_action = KC_A, .tap_repeat_count = 1u,
        .tap_branch_has_authored_tap = true, .lighting_identity = 128u,
    };
    key_runtime_transition_plan_t plan;
    key_runtime_transition_plan_init(&plan);
    plan.count = KEY_RUNTIME_TRANSITION_PLAN_CAPACITY - 1u;
    assert(key_runtime_transition_handled_key_press(CUSTOM_KEY_0 + 54u, key, &plan));
    assert(!plan.overflowed && plan.count == 2u);
    assert(plan.items[0].kind == KEY_RUNTIME_EFFECT_DELAYED_ACTION);
    assert(plan.items[1].data.feedback_pulse.lighting_identity == 128u);
    assert(plan.items[1].data.feedback_pulse.lighting_branch == 2u);
    key_runtime_transition_execute_plan(&plan);
    assert(count == 0u && key_runtime_core_pending_release_count() == 1u);
    pending_release_t pending;
    assert(key_runtime_core_pending_release_at_order(0, &pending));
    assert(pending.lighting_identity == 128u && pending.lighting_branch == 2u);
    const key_runtime_scenario_step_t release[] = {KEY_RUNTIME_SCENARIO_RELEASE(CUSTOM_KEY_0 + 54u, 0, 1)};
    key_runtime_scenario_run(release, ARRAY_SIZE(release));
    assert(count == 2u);
    assert(events[0].thing == 4u); // actual new-row dispatch precedes queue drain
    expect(1, NOAH_LIGHTING_EVENT_TAP_FIRED, 2u, 2u);
}
static void saturated_queue_keeps_actual_dispatch_occurrence(void) {
    reset();
    keypos_t key = {.row = 0u, .col = 1u};
    for (unsigned i = 0; i < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY; i++) {
        assert(key_runtime_core_queue_pending_release_dispatch_with_identity(key, KC_B, (keyboard_mod_state_t){0}, false, 0u, 0u));
    }
    key_runtime_transition_plan_t plan = {.count = 2u};
    plan.items[0] = (key_runtime_effect_t){.kind = KEY_RUNTIME_EFFECT_DELAYED_ACTION,
        .data.delayed_action = {.action = KC_A, .packed_key_pos = key_runtime_keypos_pack(key), .repeat_count = 1u | KEY_RUNTIME_DELAYED_ACTION_FLAG_DEFER_UNTIL_RELEASE}};
    plan.items[1] = (key_runtime_effect_t){.kind = KEY_RUNTIME_EFFECT_FEEDBACK_PULSE,
        .data.feedback_pulse = {.key_pos = key, .kind = KEY_FEEDBACK_PULSE_TAP_COMMITTED,
            .lighting_identity = 128u, .lighting_branch = 2u,
            .commit_flags = KEY_RUNTIME_COMMIT_QUIET | KEY_RUNTIME_COMMIT_DEFERRED}};
    key_runtime_transition_execute_plan(&plan);
    assert(count == 1u);
    expect(0, NOAH_LIGHTING_EVENT_TAP_FIRED, 2u, 2u);
    assert(key_runtime_core_pending_release_count() == KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY);
}
// The planner may invoke its sink with caller-owned items still pending.
// Force that drain while the new fallback effect is being constructed, then
// execute the retained remainder only after the planner has returned.
static void fallback_planning_drain_preserves_order_and_lifetimes(void) {
    reset();
    key_behavior_view_t view = define_key(KC_LEFT_GUI);
    view.lighting_identity = 0u;
    view.single.tap = (tap_behavior_t)TAP_SENDS(KC_A);
    key_runtime_scenario_add_behavior_view(view);
    const key_runtime_scenario_step_t press[] = {KEY_RUNTIME_SCENARIO_PRESS(KC_LEFT_GUI, 0, 1)};
    key_runtime_scenario_run(press, ARRAY_SIZE(press));
    key_runtime_transition_plan_t plan;
    key_runtime_transition_plan_init(&plan);
    plan.count = KEY_RUNTIME_TRANSITION_PLAN_CAPACITY;
    plan.items[0] = (key_runtime_effect_t){.kind = KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER,
        .data.held_action = {.key_pos = {.row = 0u, .col = 2u}, .action = KC_LEFT_SHIFT}};
    assert(key_runtime_scenario_slot_held_action_keycode((keypos_t){.row = 0u, .col = 2u}) == KC_NO);
    (void)key_runtime_transition_settle_pending_fallback_hold(&plan);
    assert(!plan.overflowed && plan.count == 1u);
    assert(plan.items[0].kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    assert(plan.items[0].data.held_action.action == KC_LEFT_GUI);
    assert(key_runtime_scenario_slot_held_action_keycode((keypos_t){.row = 0u, .col = 2u}) == KC_LEFT_SHIFT);
    assert(key_runtime_scenario_slot_held_action_keycode((keypos_t){.row = 0u, .col = 1u}) == KC_NO);
    key_runtime_transition_execute_plan(&plan);
    assert(key_runtime_scenario_slot_held_action_keycode((keypos_t){.row = 0u, .col = 1u}) == KC_LEFT_GUI);
    assert(count == 0u);
}
int main(void) {
    fallback_planning_drain_preserves_order_and_lifetimes();
    tap_without_visual_feedback();
    branch_two_commits_once();
    unnumbered_intrinsic_gesture_has_no_authored_event();
    buffered_base_and_inherited_taps_are_real_commits();
    hold_then_long_hold();
    hold_fires_on_actual_release();
    layer_hold_captures_outcome_without_standard_pulse();
    pending_commit_keeps_old_row_and_branch();
    old_series_commit_survives_streaming_boundary_and_new_row();
    saturated_queue_keeps_actual_dispatch_occurrence();
    puts("authored behaviour events are independent of visual pulse policy");
    return 0;
}
