#!/usr/bin/env bash

set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
git_dir="$(git rev-parse --git-dir)"
runner="$repo_root/scripts/run_pre_commit_hook.sh"

if [[ ! -x "$runner" ]]; then
    echo "Hook runner is not executable: $runner" >&2
    exit 1
fi

for hook_type in pre-commit pre-push; do
    hook_path="$git_dir/hooks/$hook_type"
    cat >"$hook_path" <<EOF
#!/usr/bin/env bash
set -euo pipefail
exec "$runner" hook-impl --config="$repo_root/.pre-commit-config.yaml" --hook-type="$hook_type" --hook-dir="$git_dir/hooks" -- "\$@"
EOF
    chmod +x "$hook_path"
done

echo "Installed git hooks into $git_dir/hooks"
