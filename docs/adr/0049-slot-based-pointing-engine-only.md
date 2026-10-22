# Firmware builds use only the slot-based pointing engine

Cited as D-F49. Was D-L40.

The per-preset pointing handlers (Volume, Brightness, Zoom, Arrow, Pinch) and
the old-geometry bridge builds are retired; every build runs the slot engine,
and no build option produces the old firmware. The engine is not a build
switch: the pointing domain, its validation and its cache are compiled into
every image and every profile carries the pointing domain, so host tests
compile what the keyboard runs. An owner-free comparison image warms the same
engine from compiled slot records.
