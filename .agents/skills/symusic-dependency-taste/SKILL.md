---
name: symusic-dependency-taste
description: Decision framework for selecting and evolving third-party dependencies in symusic across C++, Python packaging, bindings, testing, and docs.
metadata:
  domain: symusic
  role: dependency-architecture-advisor
  scope: dependency-selection-and-governance
  output-format: decision-record-and-checklist
---

# Symusic Dependency Taste and Selection Framework

Use this skill when adding, replacing, or auditing dependencies.

## High-Level Dependency Philosophy

1. **Prefer focused libraries with clear performance or ergonomics wins.**
   - Symusic favors small, purpose-built components (parser, serializer, binding layer) over heavy frameworks.

2. **Keep the dependency surface narrow and intentional.**
   - Every added dependency increases wheel complexity, ABI risk, cross-platform CI cost, and maintenance burden.

3. **Keep core competency in-repo; outsource commodity building blocks.**
   - MIDI-domain semantics and transformation logic should stay first-party.
   - Generic concerns (formatting, UTF-8 helpers, serialization primitives) can be external.

4. **Choose libraries that align with cross-platform wheels and modern toolchains.**
   - Compatibility with C++20, Python 3.9+ wheels, and reproducible CI builds is non-negotiable.

## Existing Stack Taste Analysis

## C++/Core Dependencies

- **`minimidi`**: chosen for fast and lightweight MIDI parsing; tightly aligned with symusic's core mission.
- **`prestosynth`**: dedicated synthesis backend for SoundFont rendering, consistent with a focused audio-rendering layer.
- **`pyvec`**: optimized container bridge that complements shared-pointer semantics and Python interop patterns.
- **`nanobind`**: modern, efficient C++/Python binding layer; matches performance-first and typed API goals.
- **`fmt`**: pragmatic formatting utility that improves clarity and maintainability over manual string assembly.
- **`zpp_bits`** (header-only include): lightweight serialization building block for pickle support.
- **`utfcpp` + small utility headers (`unordered_dense`, macros)**: low-level helpers kept lean and local where practical.

**Taste signal:** this is a deliberately modular high-performance stack with minimal overlap between libraries.

## Python Runtime Dependencies

- **`numpy`**: foundational numerical ecosystem choice; necessary for array-oriented conversions and interoperability.
- **`pySmartDL`**: targeted utility for managed downloads in soundfont workflows.
- **`platformdirs`**: minimal, standard choice for per-platform cache/config path correctness.

**Taste signal:** runtime Python dependencies are intentionally few, stable, and functionally justified.

## Test/Dev Dependencies

- **`pytest`, `pytest-cov`, `pytest-xdist`**: standard, scalable Python testing stack.
- **`mypy`, `ruff` configuration in `pyproject.toml`**: static and style guardrails with pragmatic exceptions.
- **`pretty_midi` (optional test extra)**: used as ecosystem interop/reference check rather than core runtime requirement.

**Taste signal:** strong quality automation, but optional extras remain clearly separated from runtime essentials.

## Dependency Decision Rubric (Use Before Adding Anything New)

A candidate dependency should pass all gates:

1. **Problem clarity**: Is there a repeated, non-trivial problem not already solved in-repo?
2. **Scope fit**: Does the library solve exactly that problem without pulling broad framework baggage?
3. **Performance profile**: Any predictable hot-path cost, memory overhead, or startup regression?
4. **Build/distribution impact**: Does it complicate wheels, ABI compatibility, or CI matrix reliability?
5. **Maintenance health**: Active releases, clear license, issue responsiveness, and transparent governance?
6. **Fallback plan**: Can we pin, patch, or replace it safely if upstream breaks?

If any gate fails, prefer a smaller alternative or an internal implementation.

## Preferred Choices by Category

- **Bindings:** stay with nanobind unless there is a compelling ABI/performance blocker.
- **Numerics interop:** keep NumPy as the canonical Python array surface.
- **Paths/platform behavior:** prefer tiny standard helpers like `platformdirs` over custom OS branching.
- **Formatting/logging in C++:** keep `fmt` style consistency.
- **Serialization:** keep lightweight binary serialization approaches; avoid monolithic runtime frameworks.

## Red Flags (Usually Reject)

- Dependencies that duplicate existing stack capabilities with no measurable win.
- Libraries with unstable release cadence or unclear licensing.
- New runtime dependencies that are only needed for niche workflows (move those to optional extras).
- Python-only workarounds for problems best solved once in C++ core.
- Heavy frameworks that increase import/build time for marginal convenience.

## How to Document a Dependency Change

For every addition/removal, include a short dependency decision record:

- **Context:** what limitation or pain point triggered the change?
- **Decision:** chosen library/version and integration boundary.
- **Alternatives considered:** at least one rejected option and why.
- **Consequences:** performance, binary size, CI, maintenance, and user-facing impact.
- **Rollback strategy:** how to safely revert or swap in future.

## Minimal PR Checklist for Dependency Changes

- Runtime vs optional extra placement is correct.
- Lock/build metadata updated consistently.
- Cross-platform build path is validated.
- Tests cover new integration boundary.
- Docs mention user-visible installation/runtime implications.
