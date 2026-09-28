# VS Code workflow

Open the parent `charybdis.code-workspace` for the firmware, independent Live
app, upstream QMK checkout and build artifacts together. Folder tasks bind their
working directory explicitly to this firmware repository.

- **Build Firmware Pair (flashable)** is the default build task. It uses
  `tools/build-firmware-pair.sh`, which sets both transport role and durable
  physical-half identity for each half. The former shell-alias build task is
  retired; the alias itself is not managed here.
- **Build right half + compilation database** runs `qmk compile --compiledb`
  in the sibling QMK checkout with `QMK_USERSPACE` set to this repo,
  `FORCE_MASTER=yes` and `NOAH_PHYSICAL_HALF=right`. This generates QMK's
  `compile_commands.json` for a representative normal live-owner build, which
  the parent workspace's clangd configuration uses. It is a single-half editor
  build, not the numbered flashable pair. Run the host suite before compiling.
  QMK's compiledb option cleans build output; do not run it concurrently with
  another firmware build. Regenerate after changing compile-time configuration.
- **Run all firmware host tests** is the default test task.
- Layout overview and source formatting remain available through **Tasks: Run
  Task**.

Charybdis Live launch/check/catalog tasks belong to the sibling `charybdis-live`
repo. The shared status bar has **Build
Firmware**, **Build with DB** and **Format Repo** buttons. The installed Live
extension supplies the fourth button, **Charybdis Live**, and must point to the
independent repo. Test tasks remain available without status-bar buttons.

C and C++ use clang-format; Python uses Ruff; JavaScript, JSON, HTML and CSS
use VS Code's built-in formatters; YAML uses the YAML extension. Markdown is
not automatically reformatted on save. Repository configuration files remain
visible in the Explorer. These settings do not modify upstream QMK's tracked
editor configuration.
