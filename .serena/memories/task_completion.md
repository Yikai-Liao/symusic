# Task Completion Checklist
- Re-run targeted validations: `pytest tests/` (or narrower modules) for Python changes; rebuild/execute `symusic_tests` when modifying C++ core logic.
- If bindings/types changed, re-run `./generate_stub.sh` and ensure `.pyi` is lint-clean via pre-commit.
- Always run `pre-commit run --all-files` (or the affected hooks) before sharing patches to keep Ruff formatting and whitespace consistent.
- When adjusting docs, rebuild/preview via `mdbook build docs` (until the new RTD stack replaces it) to catch broken links or SUMMARY mismatches.
- Verify packaging paths (`pip install -e .`) when touching build scripts or CMake options.
