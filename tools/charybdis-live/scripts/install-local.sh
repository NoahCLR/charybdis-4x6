#!/bin/sh
set -eu

# Symlinks this extension into the local VS Code extension directory, the same
# way Profile Studio installs. Unlike Profile Studio this app has a native
# dependency (node-hid), so it also makes sure node_modules is present before
# VS Code tries to load it.

extension_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
extension_id="noah.charybdis-live-0.0.1"
target_root="${VSCODE_EXTENSIONS_DIR:-$HOME/.vscode/extensions}"
target="$target_root/$extension_id"

if [ ! -d "$extension_dir/node_modules/node-hid" ]; then
    echo "Installing dependencies (node-hid)..."
    (cd "$extension_dir" && npm install --no-audit --no-fund)
fi

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

echo "Installed Charybdis Live as:"
echo "  $target"
echo
echo "Reload VS Code, then use the status bar item or run:"
echo "  Charybdis: Open Charybdis Live"
echo
echo "If the panel reports that node-hid failed to load, it was built for a"
echo "different Node ABI than the one VS Code runs. Rebuild it for Electron:"
echo "  cd $extension_dir && npx @electron/rebuild -v \"\$(code --version | sed -n 2p)\" -f -w node-hid"
