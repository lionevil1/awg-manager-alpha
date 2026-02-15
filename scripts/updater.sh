#!/bin/sh
set -eu

APP_NAME="awg-manager-alpha"
ARCH_TRACK="aarch64-3.10"
DEFAULT_MANIFEST_URL="https://raw.githubusercontent.com/lionevil1/awg-manager-alpha/master/update/latest-aarch64-3.10.txt"

ENV_FILE="/opt/etc/awg-manager-alpha.env"
MANIFEST_URL="$DEFAULT_MANIFEST_URL"

STATE_FILE="/opt/var/run/awg-manager-alpha-update.state"
LOCK_DIR="/opt/var/run/awg-manager-alpha-update.lock"
LOG_FILE="/opt/var/log/awg-manager-alpha-update.log"
TMP_MANIFEST="/opt/var/run/awg-manager-alpha-update.manifest"
TMP_PKG="/opt/tmp/awg-manager-alpha-update.ipk"

sanitize_value() {
    printf "%s" "$1" | tr '\n\r' '  '
}

log_msg() {
    ts="$(date '+%Y-%m-%d %H:%M:%S' 2>/dev/null || date)"
    printf "%s %s\n" "$ts" "$1" >>"$LOG_FILE"
}

ensure_dirs() {
    mkdir -p /opt/var/run /opt/var/log /opt/tmp
}

load_env() {
    if [ -f "$ENV_FILE" ]; then
        set -a
        . "$ENV_FILE"
        set +a
    fi

    if [ -n "${AWG_UPDATE_MANIFEST_URL:-}" ]; then
        MANIFEST_URL="$AWG_UPDATE_MANIFEST_URL"
    fi
}

write_state() {
    state="$(sanitize_value "$1")"
    current="$(sanitize_value "$2")"
    latest="$(sanitize_value "$3")"
    update_available="$(sanitize_value "$4")"
    message="$(sanitize_value "$5")"
    ipk_url="$(sanitize_value "$6")"
    sha256="$(sanitize_value "$7")"
    checked_at="$(date '+%Y-%m-%dT%H:%M:%S%z' 2>/dev/null || date)"

    cat >"$STATE_FILE" <<EOF
state=$state
current_version=$current
latest_version=$latest
update_available=$update_available
message=$message
manifest_url=$MANIFEST_URL
arch=$ARCH_TRACK
ipk_url=$ipk_url
sha256=$sha256
checked_at=$checked_at
EOF
}

read_state_value() {
    key="$1"
    if [ ! -f "$STATE_FILE" ]; then
        return 1
    fi
    awk -F'=' -v k="$key" '$1==k {print substr($0, index($0, "=")+1); exit}' "$STATE_FILE"
}

acquire_lock() {
    if mkdir "$LOCK_DIR" 2>/dev/null; then
        return 0
    fi
    log_msg "lock busy, skip command"
    return 1
}

release_lock() {
    rmdir "$LOCK_DIR" 2>/dev/null || true
}

fetch_url() {
    url="$1"
    out="$2"

    if command -v wget >/dev/null 2>&1; then
        wget -q -T 20 -O "$out" "$url"
        return 0
    fi
    if command -v uclient-fetch >/dev/null 2>&1; then
        uclient-fetch -q -T 20 -O "$out" "$url"
        return 0
    fi
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL --max-time 20 -o "$out" "$url"
        return 0
    fi

    return 1
}

get_current_version() {
    v="$(opkg status "$APP_NAME" 2>/dev/null | awk -F': ' '/^Version: /{print $2; exit}')"
    if [ -z "$v" ]; then
        printf "%s" "unknown"
    else
        printf "%s" "$v"
    fi
}

manifest_value() {
    key="$1"
    awk -F'=' -v k="$key" '$1==k {print substr($0, index($0, "=")+1); exit}' "$TMP_MANIFEST"
}

version_gt() {
    latest="$1"
    current="$2"
    if [ -z "$latest" ]; then
        return 1
    fi
    if [ "$current" = "unknown" ]; then
        return 0
    fi
    if [ "$latest" = "$current" ]; then
        return 1
    fi
    latest_sorted="$(printf '%s\n%s\n' "$current" "$latest" | sort -V | tail -n1)"
    if [ "$latest_sorted" = "$latest" ]; then
        return 0
    fi
    return 1
}

has_newer_version() {
    current="$1"
    latest="$2"
    version_gt "$latest" "$current"
}

check_update_locked() {
    current="$(get_current_version)"
    latest=""
    ipk_url=""
    sha256=""

    write_state "checking" "$current" "" "0" "checking update" "" ""
    log_msg "check start: current=$current"

    if ! fetch_url "$MANIFEST_URL" "$TMP_MANIFEST"; then
        write_state "error" "$current" "" "0" "manifest download failed" "" ""
        log_msg "check failed: manifest download"
        return 1
    fi

    latest="$(manifest_value version || true)"
    ipk_url="$(manifest_value ipk_url || true)"
    sha256="$(manifest_value sha256 || true)"

    if [ -z "$latest" ]; then
        write_state "error" "$current" "" "0" "manifest has no version" "" ""
        log_msg "check failed: manifest version missing"
        return 1
    fi

    if has_newer_version "$current" "$latest"; then
        if [ -z "$ipk_url" ]; then
            write_state "error" "$current" "$latest" "0" "manifest has no ipk_url" "" ""
            log_msg "check failed: ipk_url missing"
            return 1
        fi
        write_state "available" "$current" "$latest" "1" "update available" "$ipk_url" "$sha256"
        log_msg "check done: update available latest=$latest"
        return 0
    fi

    write_state "up_to_date" "$current" "$latest" "0" "already up to date" "$ipk_url" "$sha256"
    log_msg "check done: up_to_date latest=$latest"
    return 0
}

apply_update_locked() {
    current="$(get_current_version)"
    latest=""
    ipk_url=""
    sha256=""

    log_msg "apply start: current=$current"

    if ! check_update_locked; then
        log_msg "apply aborted: check failed"
        return 1
    fi

    latest="$(read_state_value latest_version || true)"
    ipk_url="$(read_state_value ipk_url || true)"
    sha256="$(read_state_value sha256 || true)"

    if [ "$(read_state_value update_available || printf 0)" != "1" ]; then
        log_msg "apply skipped: no update"
        return 0
    fi

    write_state "downloading" "$current" "$latest" "1" "downloading package" "$ipk_url" "$sha256"
    if ! fetch_url "$ipk_url" "$TMP_PKG"; then
        write_state "error" "$current" "$latest" "1" "ipk download failed" "$ipk_url" "$sha256"
        log_msg "apply failed: ipk download"
        return 1
    fi

    if [ -n "$sha256" ]; then
        if ! command -v sha256sum >/dev/null 2>&1; then
            write_state "error" "$current" "$latest" "1" "sha256sum missing" "$ipk_url" "$sha256"
            log_msg "apply failed: sha256sum missing"
            return 1
        fi

        got_sha256="$(sha256sum "$TMP_PKG" | awk '{print $1}')"
        if [ "$(printf "%s" "$got_sha256" | tr 'A-Z' 'a-z')" != "$(printf "%s" "$sha256" | tr 'A-Z' 'a-z')" ]; then
            write_state "error" "$current" "$latest" "1" "sha256 mismatch" "$ipk_url" "$sha256"
            log_msg "apply failed: sha256 mismatch got=$got_sha256 expected=$sha256"
            return 1
        fi
    fi

    write_state "installing" "$current" "$latest" "1" "installing package" "$ipk_url" "$sha256"
    if opkg install "$TMP_PKG" >>"$LOG_FILE" 2>&1; then
        new_current="$(get_current_version)"
        write_state "done" "$new_current" "$latest" "0" "update installed" "$ipk_url" "$sha256"
        log_msg "apply done: current=$new_current"
        return 0
    fi

    write_state "error" "$current" "$latest" "1" "opkg install failed" "$ipk_url" "$sha256"
    log_msg "apply failed: opkg install"
    return 1
}

cmd_check() {
    if ! acquire_lock; then
        exit 0
    fi
    check_update_locked || true
    release_lock
}

cmd_apply() {
    if ! acquire_lock; then
        exit 0
    fi
    apply_update_locked || true
    release_lock
}

cmd_status() {
    if [ ! -f "$STATE_FILE" ]; then
        current="$(get_current_version)"
        write_state "idle" "$current" "" "0" "no checks yet" "" ""
    fi
    cat "$STATE_FILE"
}

main() {
    cmd="${1:-status}"

    ensure_dirs
    load_env

    case "$cmd" in
    check)
        cmd_check
        ;;
    apply)
        cmd_apply
        ;;
    status)
        cmd_status
        ;;
    *)
        echo "usage: $0 {check|apply|status}" >&2
        exit 1
        ;;
    esac
}

main "$@"
