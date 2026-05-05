#!/bin/sh
set -eu

extension_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
extension_id="noah.charybdis-profile-studio-0.0.1"
target_root="${VSCODE_EXTENSIONS_DIR:-$HOME/.vscode/extensions}"
target="$target_root/$extension_id"

mkdir -p "$target_root"

if [ -L "$target" ]; then
    current_target=$(readlink "$target")
    if [ "$current_target" != "$extension_dir" ]; then
        rm "$target"
        ln -s "$extension_dir" "$target"
    fi
elif [ -e "$target" ]; then
    echo "Refusing to overwrite non-symlink extension path: $target" >&2
    echo "Remove it manually or set VSCODE_EXTENSIONS_DIR to a different extension directory." >&2
    exit 1
else
    ln -s "$extension_dir" "$target"
fi

echo "Installed Charybdis Profile Studio as:"
echo "  $target"
echo
echo "Reload VS Code, then use the status bar item or run:"
echo "  Charybdis: Open Profile Studio"
