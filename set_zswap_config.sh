#!/bin/bash
# A helper for writing zswap sysfs parameters from the command line.

set -euo pipefail

SYSFS_DIR="/sys/module/zswap/parameters"

VALID_ZPOOL=(zbud z3fold zsmalloc)
VALID_MAX_POOL_PERCENT=(10 20 30 40 50 60)
VALID_COMPRESSOR=(lzo deflate 842 lz4 lz4hc zstd)
VALID_SHRINKER=(Y N)

usage() {
    cat <<EOF
Usage: $0 [options]

Options:
  --zpool <zbud|z3fold|zsmalloc>
  --max-pool-percent <10|20|30|40|50|60>
  --compressor <lzo|deflate|842|lz4|lz4hc|zstd>
  --shrinker-enabled <Y|N|yes|no>
  --help

Only the parameters you specify are updated. Run as root to allow writes to
/sys/module/zswap/parameters/.
EOF
}

error() {
    echo "Error: $*" >&2
    usage >&2
    exit 1
}

require_root() {
    if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
        echo "This script must run as root (or via sudo) to modify sysfs." >&2
        exit 1
    fi
}

contains() {
    local needle="$1"; shift
    for item in "$@"; do
        if [[ "$item" == "$needle" ]]; then
            return 0
        fi
    done
    return 1
}

write_sysfs() {
    local param="$1"
    local value="$2"
    local path="$SYSFS_DIR/$param"

    if [[ ! -e "$path" ]]; then
        echo "Cannot find $path; zswap may not be enabled on this system." >&2
        exit 1
    fi

    printf '%s' "$value" > "$path"
    echo "Set $param=$value"
}

ZPOOL=""
MAX_POOL_PERCENT=""
COMPRESSOR=""
SHRINKER=""

if [[ $# -eq 0 ]]; then
    usage
    exit 0
fi

while [[ $# -gt 0 ]]; do
    case "$1" in
        --zpool)
            [[ $# -ge 2 ]] || error "--zpool requires a value"
            value=$(printf '%s' "$2" | tr '[:upper:]' '[:lower:]')
            contains "$value" "${VALID_ZPOOL[@]}" || error "Invalid zpool '$value'"
            ZPOOL="$value"
            shift 2
            ;;
        --max-pool-percent)
            [[ $# -ge 2 ]] || error "--max-pool-percent requires a value"
            value="$2"
            contains "$value" "${VALID_MAX_POOL_PERCENT[@]}" || error "Invalid max pool percent '$value'"
            MAX_POOL_PERCENT="$value"
            shift 2
            ;;
        --compressor)
            [[ $# -ge 2 ]] || error "--compressor requires a value"
            value=$(printf '%s' "$2" | tr '[:upper:]' '[:lower:]')
            contains "$value" "${VALID_COMPRESSOR[@]}" || error "Invalid compressor '$value'"
            COMPRESSOR="$value"
            shift 2
            ;;
        --shrinker-enabled)
            [[ $# -ge 2 ]] || error "--shrinker-enabled requires a value"
            value=$(printf '%s' "$2" | tr '[:lower:]' '[:upper:]')
            case "$value" in
                YES) value="Y" ;;
                NO) value="N" ;;
            esac
            contains "$value" "${VALID_SHRINKER[@]}" || error "Invalid shrinker flag '$2'"
            SHRINKER="$value"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            error "Unknown option '$1'"
            ;;
    esac
done

if [[ -z "$ZPOOL$MAX_POOL_PERCENT$COMPRESSOR$SHRINKER" ]]; then
    error "No parameters provided to update."
fi

require_root

[[ -n "$MAX_POOL_PERCENT" ]] && write_sysfs "max_pool_percent" "$MAX_POOL_PERCENT"
[[ -n "$COMPRESSOR" ]] && write_sysfs "compressor" "$COMPRESSOR"
[[ -n "$ZPOOL" ]] && write_sysfs "zpool" "$ZPOOL"
[[ -n "$SHRINKER" ]] && write_sysfs "shrinker_enabled" "$SHRINKER"
