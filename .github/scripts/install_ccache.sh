#!/usr/bin/env bash
# Install the pinned static ccache binary for either Linux wheel architecture.
set -euo pipefail

version=4.14.3
arch="$(uname -m)"
case "$arch" in
    x86_64) checksum=e4583fd653b794a480b9915358679a75cdd7a7010dd865eaf4cbdc61e665a3f0 ;;
    aarch64) checksum=4b0ecece7929cbaf548ff1ab9a24074b240132ab8176bc3865a27d88c457a0d8 ;;
    *) echo "Unsupported ccache host architecture: $arch" >&2; exit 1 ;;
esac

prefix="${1:-$HOME/.local}"
name="ccache-${version}-linux-${arch}-musl-static"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
curl -fsSL --retry 3 "https://github.com/ccache/ccache/releases/download/v${version}/${name}.tar.gz" -o "$tmp/ccache.tar.gz"
echo "$checksum  $tmp/ccache.tar.gz" | sha256sum -c > /dev/null
tar -xzf "$tmp/ccache.tar.gz" -C "$tmp"
mkdir -p "$prefix/bin"
install -m 755 "$tmp/$name/ccache" "$prefix/bin/ccache"
echo "$prefix/bin"
