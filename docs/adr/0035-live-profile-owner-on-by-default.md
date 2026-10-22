# The live-profile owner is on by default

Cited as D-F35. Was D-L08.

The firmware anyone flashes owns a live profile, so Ark targets ordinary
firmware, not an engineering build. The owner must know its half, since a
profile's origin is side-specific, so the flashed pair is the side-specific
build of `tools/build-firmware-pair.sh`, and a release fails rather than
publish a generic image. A build with no half, such as a plain `qmk compile`
or one setting only `FORCE_MASTER`/`FORCE_SLAVE`, builds without the owner
instead of failing. `NOAH_LIVE_PROFILE_OWNER=no` stays as the lever for
comparing ordinary and live behaviour on identical source.
