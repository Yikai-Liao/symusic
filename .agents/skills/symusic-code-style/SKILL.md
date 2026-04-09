---
name: symusic-code-style
description: Encodes symusic's engineering taste across C++ core, nanobind bindings, and Python-facing APIs. Use when implementing or reviewing features to preserve performance-first architecture and ergonomic, predictable interfaces.
metadata:
  domain: symusic
  role: project-specific-style-guide
  scope: implementation-and-review
  output-format: checklist-and-rationale
---

# Symusic Code Style and Engineering Taste

Use this skill whenever a change touches `src/`, `include/`, `py_src/`, `python/symusic/`, or `tests/`.

## First-Principles Guidance

1. **Performance is a product feature, not an optimization pass.**
   - Keep hot paths in C++ (`src/`, `include/`) and expose them through thin nanobind layers.
   - Prefer data representations that avoid repeated conversion and minimize allocation churn.

2. **API symmetry beats cleverness.**
   - For transformations, preserve the `foo()` + `foo_inplace()` dual interface.
   - Preserve consistent behavior across time units (`Tick`, `Quarter`, `Second`) and container types (`Note`, `Track`, `Score`).

3. **Type-level correctness should be explicit at boundaries.**
   - Keep time-unit semantics explicit and dispatch through well-defined factories instead of ad-hoc runtime checks.
   - Ensure Python-visible typing helpers remain informative and fail loudly on invalid construction.

4. **Cross-language predictability is mandatory.**
   - Python APIs should feel ergonomic, but never hide semantic differences from the C++ core.
   - Prefer deterministic behavior and clear exceptions over silent coercion.

5. **Readable systems code over minimalist one-liners.**
   - Favor named helper functions and local structs when they clarify non-trivial timeline/algorithm logic.
   - Document subtle invariants (time tolerance, ownership expectations, conversion assumptions) near the implementation.

## Repository-Specific Style Patterns

## C++ Core (`include/`, `src/`)

- Target modern C++ with explicit compile settings (`C++20`, no compiler extensions).
- Use project aliases and containers (`i32`, `vec`, `shared`, `pyvec`) consistently instead of mixing raw STL usage styles.
- Prefer value-semantic operations (`copy`, `deepcopy`, non-inplace transforms) and keep deep-copy semantics explicit.
- Keep equality and ordering logic structural and deterministic, including pointer-aware equality fallbacks where needed.
- For numeric timeline logic:
  - centralize tolerances,
  - avoid duplicated floating-point comparisons,
  - isolate helper predicates (`close_enough`, `ge_or_close`) and step derivation logic.

## Nanobind Layer (`py_src/bindings/`)

- Keep module entry points compact and delegate registrations to smaller binding units.
- Prefer explicit, user-facing error messages when classes are typing placeholders or generic helpers.
- Ensure pickle/state methods and reprs are predictable and stable.
- Guard optional diagnostics (`MEM_LEAK_WARNING`) behind build flags rather than ad-hoc runtime branches.

## Python API Layer (`python/symusic/`)

- Use factories to centralize dispatch across time units and core class variants.
- Keep constructors and conversion helpers straightforward; avoid hidden expensive operations.
- Raise precise exceptions (`TypeError` for wrong object kind, `ValueError` for invalid value domain).
- Keep module import side-effects intentional, short, and validated (e.g., binary path checks).

## Testing and Quality Signals (`tests/` + `pyproject.toml`)

- Keep tests behavior-focused, with clear assertions for immutability contracts and I/O round-trips.
- Treat warnings as quality regressions where configured.
- Respect existing lint philosophy: broad Ruff coverage, targeted per-file exceptions, strict pytest options.

## Change Checklist Before Merge

- Did the change preserve `inplace` and non-`inplace` API symmetry where relevant?
- Are time-unit semantics (`tick/quarter/second`) consistent and explicitly handled?
- Are exceptions and error messages concrete and user-actionable?
- Did you avoid adding hidden work in Python wrappers that belongs in C++?
- Are tests updated for behavior and edge cases, not just happy paths?
- Are comments/docstrings in English and focused on invariants rather than restating code?

## Anti-Patterns to Avoid

- Introducing convenience wrappers that blur time-unit semantics.
- Adding dependencies to solve narrowly scoped problems already covered by existing internal utilities.
- Mixing ownership styles unpredictably (raw pointers vs shared/value semantics) in core data models.
- Creating Python-side fallback implementations for logic that should remain canonical in C++.
