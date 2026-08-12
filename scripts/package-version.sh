#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 <noble|resolute>" >&2
    exit 2
fi

case "$1" in
    noble|resolute) codename="$1" ;;
    *) echo "unsupported Ubuntu codename: $1" >&2; exit 2 ;;
esac

cargo_version="$(sed -n 's/^version = "\([^"]*\)"/\1/p' Cargo.toml | head -n1)"
addon_version="$(sed -n 's/^Version=//p' data/vietime-addon.conf.in | head -n1)"
if [[ -z "$cargo_version" || "$addon_version" != "$cargo_version" ]]; then
    echo "Cargo version '$cargo_version' does not match addon version '$addon_version'" >&2
    exit 1
fi
ref_name="${GITHUB_REF_NAME:-}"
tag_version="${ref_name#v}"
if [[ -n "$ref_name" && "$tag_version" != "$cargo_version" ]]; then
    echo "tag $ref_name does not match Cargo version $cargo_version" >&2
    exit 1
fi

dch --newversion "${cargo_version}-1~${codename}1" \
    --distribution "$codename" \
    "Build for Ubuntu ${codename}."
