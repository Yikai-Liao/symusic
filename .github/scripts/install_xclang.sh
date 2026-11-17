#!/usr/bin/env bash
# Linux x64 builder used by the cross-compilation validation workflow.
set -euo pipefail

version=23.1.5.2
checksum=6eb5f493c1c2171fed2ece3da2d234d24208953e833cb042de23f269c6870579
prefix="${1:?Pass an installation directory}"
archive="xclang-${version}-x86_64-unknown-linux-gnu.tar.xz"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
curl -fsSL --retry 3 "https://github.com/clice-io/xclang/releases/download/${version}/${archive}" -o "$tmp/$archive"
echo "$checksum  $tmp/$archive" | sha256sum -c > /dev/null
mkdir -p "$prefix"
tar -xJf "$tmp/$archive" -C "$prefix"
echo "$prefix/xclang"
