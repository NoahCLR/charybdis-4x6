#include "users/noah/lib/rgb/lighting/lighting_workspace.h"
#include "users/noah/lib/rgb/lighting/lighting_batch_arena.h"
#include "users/noah/lib/rgb/lighting/lighting_owned_batch.h"
#include "users/noah/lib/rgb/lighting/lighting_history_owner.h"
#include "users/noah/lib/rgb/lighting/lighting_history_bank.h"
#include "users/noah/lib/rgb/lighting/lighting_priority.h"
#include <stdio.h>

_Static_assert(sizeof(noah_lighting_placed_subject_t) == 16u, "existing placed subject geometry");
_Static_assert(sizeof(noah_lighting_event_group_t) == 56u, "explicit codec still avoids native group padding");
_Static_assert(sizeof noah_lighting_workspace.render == 1024u, "existing render reservation");
_Static_assert(sizeof noah_lighting_workspace >= sizeof noah_lighting_workspace.capture, "all enabled fact slices fit");
_Static_assert(sizeof noah_lighting_workspace >= NOAH_LIGHTING_BATCH_SNAPSHOT_BYTES, "complete ordered batch fits");
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(noah_lighting_history_bank_t) == 1044u, "accounted compact-bank control, arena storage separate");
_Static_assert(sizeof(noah_lighting_history_store_t) == 1036u, "accounted one-arena store control");
_Static_assert(sizeof(noah_lighting_history_cursor_t) == 4u, "logical-key cursor, no hidden index");
_Static_assert(sizeof(noah_lighting_history_owner_t) == 24u, "accounted sparse-owner control, not a bank");
_Static_assert(sizeof(noah_lighting_history_index_t) == 512u, "accounted logical prefix index");
_Static_assert(sizeof(noah_lighting_history_rule_t) == 12u, "bounded temporary rule descriptor");
_Static_assert(sizeof(noah_lighting_priority_t) == 24u, "bounded temporary priority key");
_Static_assert(sizeof(noah_lighting_batch_arena_t) == 16u, "accounted target control block");
_Static_assert(sizeof(noah_lighting_batch_writer_t) == 16u, "accounted target writer");
_Static_assert(sizeof(noah_lighting_batch_reader_t) == 32u, "accounted target reader");
_Static_assert(sizeof(noah_lighting_owned_batch_reader_t) == 24u, "accounted reference-based target reader");
#endif
#if MATRIX_ROWS * MATRIX_COLS == 60 && COMBO_BUFFER_LENGTH == 4
_Static_assert(sizeof(noah_lighting_facts_t) == 621u, "recorded full-module configuration");
_Static_assert(sizeof noah_lighting_workspace.capture == 696u, "facts/group/single-event output coexist");
_Static_assert(sizeof(noah_lighting_workspace_t) == 1024u, "no default scratch allocation increase");
#endif
int main(void) {
    printf("lighting scratch layout %ux%u combo%u: facts%zu capture%zu render%zu union%zu (configuration size, not linked acceptance)\n", MATRIX_ROWS, MATRIX_COLS, COMBO_BUFFER_LENGTH, sizeof(noah_lighting_facts_t), sizeof noah_lighting_workspace.capture, sizeof noah_lighting_workspace.render, sizeof(noah_lighting_workspace_t));
}
