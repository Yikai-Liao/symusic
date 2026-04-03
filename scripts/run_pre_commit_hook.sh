#!/usr/bin/env bash

set -euo pipefail

if command -v uvx >/dev/null 2>&1; then
    exec uvx pre-commit "$@"
fi

if command -v pre-commit >/dev/null 2>&1; then
    exec pre-commit "$@"
fi

echo "pre-commit is not installed. Install uv or pre-commit first." >&2
exit 1
