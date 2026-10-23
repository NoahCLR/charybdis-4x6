# Lighting

The stages that draw lighting. Where their truth comes from and how the slave
half gets it is [rgb-render.md](../../../../docs/architecture/rgb-render.md);
what they draw is [rgb-domain-v1.md](../../../../docs/spec/rgb-domain-v1.md#drawing).

## Layout

- `core/rgb_runtime.c`: the QMK hook's entry, the frame's captured lighting
  view and source snapshot, the stage order, the idle timeout.
- `core/rgb_effective_config.c`: the stages' only reader of the lighting
  view, one accessor per record kind, each gated on its stage bit.
- `core/rgb_validation.c`: checks the authored tables at post-init.
- `core/rgb_helpers.h`, `core/rgb_config_helpers.h`: the authoring types and
  macros `rgb_config.c` uses; `core/rgb_config_defaults.c`: weak empty group
  tables for a keymap that authors none.
- `stages/`: layer, layer preview, pointing mode, combo feedback, key
  feedback, one file each.
- `automouse/`: fade progress (`rgb_automouse.c`) and the fade stage, which
  blends two layer-stage renders and the captured background effect.

## A stage

- It renders into a caller-owned `rgb_runtime_frame_t` (a colour and a
  painted flag per LED) for one chunk `[led_min, led_max)`, given the
  frame's captured `noah_effective_rgb_frame_t`. It clears its range first.
- It reads lighting only through `rgb_effective_config_*`. A failed record
  read clears the frame and paints nothing, never part of a table.
- After painting it calls `rgb_effective_config_frame_current` and clears
  the frame if the view went stale.
- `rgb_runtime.c` applies the frame with `rgb_runtime_layer_stage_apply_frame`,
  which sets only painted LEDs, so what the stage leaves unpainted shows
  what is beneath.
- Group bitmaps index LED `n` as bit `n % 8` of byte `n / 8`; a black group
  colour is the stage's own (`rgb_hsv_is_inherit_color`).
- Runtime state comes from its owner's getter on the master half. On the
  slave half combo and key-feedback sources are read once per frame through
  `split_runtime_sync_remote_read_*` into `rgb_runtime_render_snapshot`, held
  to 96 bytes by a static assertion; the preview layer and fade progress are
  read straight from `split_runtime_sync_remote`, and pointing from its
  snapshot.
- Each stage keeps a `_render` wrapper that passes a `NULL` view, which reads
  the compiled lighting; host tests use it.

The scratch frames are static and LED-frame-sized (one in `rgb_runtime.c`,
three in the fade stage); a new stage reuses the shared one.

A new stage needs its compile flag, a stage bit and its records in
`profile/schema/profile_rgb_v1`, its accessors here, its call in
`rgb_runtime_render_range`, its source in `users/noah/source_manifest.mk`
and its place in the spec's drawing order.
