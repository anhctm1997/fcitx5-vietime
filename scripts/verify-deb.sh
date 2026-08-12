#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 <package.deb>" >&2
    exit 2
fi

package="$1"
multiarch="$(dpkg-architecture -qDEB_HOST_MULTIARCH)"
contents="$(dpkg-deb --contents "$package")"

for path in \
    "./usr/lib/${multiarch}/fcitx5/vietime.so" \
    "./usr/share/fcitx5/addon/vietime.conf" \
    "./usr/share/fcitx5/inputmethod/vietime.conf"
do
    if ! grep -Fq "$path" <<<"$contents"; then
        echo "missing package path: $path" >&2
        exit 1
    fi
done

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT
dpkg-deb --extract "$package" "$tmp_dir"
addon="$tmp_dir/usr/lib/${multiarch}/fcitx5/vietime.so"

if readelf -d "$addon" | grep -Eq '(RPATH|RUNPATH)'; then
    echo "addon contains an RPATH/RUNPATH" >&2
    exit 1
fi

for symbol in vietime_engine_new vietime_process_key vietime_action_free; do
    if ! nm -D "$addon" | grep -Fq "$symbol"; then
        echo "missing Rust FFI symbol: $symbol" >&2
        exit 1
    fi
done

echo "verified $package"

