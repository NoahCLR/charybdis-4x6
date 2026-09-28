# Ordinary firmware checks retain the in-tree default until extraction cleanup.
# An explicit selection must never silently fall back to that default.
noah_host_export_live_root() {
    if [ "${CHARYBDIS_LIVE_ROOT+x}" = x ]; then
        live_candidate="$CHARYBDIS_LIVE_ROOT"
    else
        live_candidate="$1/tools/charybdis-live"
    fi
    if [ ! -f "$live_candidate/package.json" ] || [ ! -d "$live_candidate/core/schema" ]; then
        echo "Missing Charybdis Live checkout: '$live_candidate'. Set CHARYBDIS_LIVE_ROOT to the app repository root." >&2
        return 1
    fi
    CHARYBDIS_LIVE_ROOT="$(CDPATH= cd -- "$live_candidate" && pwd -P)"
    export CHARYBDIS_LIVE_ROOT
}
