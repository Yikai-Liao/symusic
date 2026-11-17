#!/usr/bin/env bash
# Install the pinned clice release for linux x86_64 into $1 (default: ~/.local)
# and print its bin directory. Bump CLICE_VERSION and CLICE_SHA256 together.
set -euo pipefail

CLICE_VERSION=1.0.0
CLICE_SHA256=9dd181aac9c81cae76b255e953da21c9e603fc5b406447eb26c95eaf67ed969a

prefix="${1:-$HOME/.local}"
archive="clice-${CLICE_VERSION}.x86_64-unknown-linux-gnu.tar.gz"
url="https://github.com/clice-io/clice/releases/download/v${CLICE_VERSION}/${archive}"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
curl -fsSL --retry 3 -o "$tmp/$archive" "$url"
echo "${CLICE_SHA256}  $tmp/$archive" | sha256sum --check --quiet
mkdir -p "$prefix"
tar -xzf "$tmp/$archive" -C "$prefix"
echo "$prefix/clice/bin"
