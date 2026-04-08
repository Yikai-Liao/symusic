# Changelog

This file tracks the current `v0.6.0` release line and backfills published releases from `v0.1.0`
forward. Historical entries are based on GitHub Releases, with adjacent tag diffs used only to fill
in obviously under-described user-facing changes.

## [0.6.0] - Upcoming

### Added

- Added `get_beats()` and `get_downbeats()` helpers for beat-grid and bar-start extraction.

### Changed

- Dropped Python 3.8 support. The supported baseline is now Python 3.9+.
- Improved source-build speed and Windows build routing for `pip install .` workflows.
- Reworked contributor-facing docs around local builds, testing, and documentation maintenance.

### Fixed

- Hardened ABC and MIDI loading, including stricter malformed-file handling and safer converter
  process invocation.
- Fixed Unicode and Windows path handling across MIDI and ABC workflows.
- Fixed beat / time-unit conversion regressions and quarter/second timing edge cases.
- Fixed pianoroll ownership handling and related cross-language lifetime issues.

## [0.5.9] - 2025-11-18

### Added

- Added optional `sanitize_data` handling to the MIDI parsing pipeline.
- Added Python 3.14 support.

### Changed

- Moved PyPy wheel support from `pp39` to `pp311`.
- Updated `nanobind`, `minimidi`, and `cibuildwheel`.

### Fixed

- Kept `sanitize_data` defaulting to `False`, so existing parsing workflows remain unchanged unless
  they explicitly opt in.

## [0.5.8] - 2025-05-18

### Added

- Added `strict_mode` parsing controls to the Python API.
- Added trimming operations on `Track` and `Score`.

### Changed

- Expanded test and coverage infrastructure around the new parsing and trimming paths.

### Fixed

- Fixed Windows UTF-8 path handling.
- Fixed ABC-to-MIDI filesystem path handling around strict parsing.

## [0.5.7] - 2025-03-03

### Performance

- Improved MIDI parsing performance by roughly 60%.

### Fixed

- Fixed `Score.clip` so ongoing tempo and time-signature events at the start boundary are retained.
- Fixed additional parsing bugs and pitch-bend range checks in the release line.

## [0.5.6] - 2025-01-14

### Added

- Added Python 3.13 wheel builds.

### Fixed

- Removed random noise at the end of rendered audio.
- Fixed `dump_wav` behavior when `stereo=True`.

## [0.5.5] - 2024-11-17

### Added

- Added source distribution publishing.
- Added mono audio support in the synthesis / WAV export line.

### Changed

- Updated to nanobind 2.2.0.
- Refreshed benchmark, citation, and demo-facing project materials.

### Fixed

- Fixed mono WAV dumping in `dump_wav`.

## [0.5.4] - 2024-10-03

### Changed

- Refreshed generated stub files in this release line.

### Fixed

- Fixed the Python binding for the render function.

## [0.5.3] - 2024-09-18

### Fixed

- Fixed filter behavior and added regression coverage for it.

## [0.5.2] - 2024-08-25

### Fixed

- Fixed pianoroll binding arguments.
- Fixed a pianoroll out-of-bounds bug.

## [0.5.1] - 2024-08-02

### Performance

- Significantly improved MIDI dump speed.

### Fixed

- Fixed the hash values for the General MIDI SF2 and SF3 soundfont assets.

## [0.5.0] - 2024-07-19

### Changed

- Introduced `pyvec` in place of `std::vector` to reduce dangling-pointer risk in bindings.
- Moved `lyrics` into tracks to improve lyric and note alignment.

### Fixed

- Fixed MIDI running-status parsing errors.

## [0.4.8] - 2024-05-30

### Changed

- Pinned `nanobind` to `v1.9.2` for the `0.4.x` line.

## [0.4.7] - 2024-04-29

### Fixed

- Fixed integer overflow cases in `resample`.
- Added `OverflowError` handling for other related overflow scenarios.

## [0.4.6] - 2024-04-26

### Fixed

- Fixed empty-note parsing.

## [0.4.5] - 2024-04-12

### Fixed

- Started preprocessing MIDI UTF-8 strings with `utfcpp` to address Unicode-related bugs.

## [0.4.4] - 2024-03-30

### Fixed

- Fixed the PyPI source distribution so releases with the synthesizer dependency tree could build
  correctly from source.

## [0.4.3] - 2024-03-27

### Changed

- Changed `clip` so the interval start keeps the active `Tempo`, `TimeSignature`, and
  `KeySignature` sentinel values.
- Made `to()` preserve event order when converting between time units.

## [0.4.2] - 2024-02-21

### Fixed

- Fixed a typo in the `Soundfont` API: `donwload` to `download`.

## [0.4.1] - 2024-02-21

### Fixed

- Fixed the Python synthesizer binding by removing the incorrect `worker_num` parameter.

## [0.4.0] - 2024-02-21

### Added

- Added SoundFont-based synthesis through the Prestosynth integration.
- Added `adjust_time`, aligned with PrettyMIDI-style semantics.

## [0.3.5] - 2024-02-02

### Fixed

- Fixed a macOS multiprocessing segmentation fault.
- Fixed pickle `__setstate__` errors.

## [0.3.4] - 2024-01-29

### Fixed

- Dumped multi-track MIDI through multiple channels instead of forcing channel 0 for all tracks
  except drums.
- Fixed the `encodeVelocity` argument in pianoroll generation.

## [0.3.3] - 2024-01-22

### Added

- Added experimental ABC support by packaging `abc2midi` and `midi2abc` with the Python package.
- Added `Score.from_abc(...)` and file-based ABC loading through the bundled CLI tools.

## [0.3.2] - 2024-01-17

### Fixed

- Fixed second-unit conversion when `tempos` is empty.
- Fixed `start` and `end` behavior when `tracks` is empty.

## [0.3.1] - 2024-01-12

### Changed

- Set the default `TrackList` sort key to `is_drum, program, name, note_num()`.

### Fixed

- Fixed errors in copy and sort methods.

## [0.3.0] - 2024-01-08

### Added

- Turned Symusic into a reusable C++ library instead of a single-header-style code dump.
- Added the struct-of-arrays `.numpy()` interface.
- Added a list-level `filter` method across Symusic containers.
- Added the first mdBook-based project documentation site.

### Changed

- Re-implemented pianoroll.
- Completed `second` time-unit support.
- Switched Python bindings from pybind11 to nanobind.

## [0.2.5] - 2024-01-04

### Performance

- Significantly improved MIDI parsing speed.

### Fixed

- Fixed Windows UTF-8 path handling.
- Fixed a dump error in the release line while pulling parser optimizations forward.

## [0.2.4] - 2023-12-28

### Changed

- Changed the default sort keys for notes and pedals.
- Sorted notes and pedals while parsing MIDI files.

### Fixed

- Fixed empty-score dump errors.

## [0.2.3] - 2023-12-25

### Added

- Added a first naive `Score.resample(tpq, min_dur)` implementation.

### Fixed

- Fixed `TempoFactory.__call__` so `Tempo` can be created from `qpm` or `mspq`.

## [0.2.2] - 2023-12-22

### Added

- Added Python test scripts and corresponding CI configuration.

### Changed

- Switched packaging from `setup.py` to `pyproject.toml`.

### Fixed

- Fixed note-order errors during dump operations.
- Fixed tempo precision loss.

## [0.2.1] - 2023-12-15

### Added

- Added the `Pedal.end` property.

## [0.2.0] - 2023-12-11

### Added

- Added writing MIDI files back to disk.
- Added score time-unit conversion through constructor and `.to(...)`.
- Added `KeySignature.degree` with PrettyMIDI / Miditoolkit-style semantics.

## [0.1.7] - 2023-12-11

### Changed

- Expanded `TrackFactory` argument support.

### Fixed

- Fixed most of the known parsing bugs tracked at the time.

## [0.1.6] - 2023-12-10

### Added

- Added `isinstance(...)` support for Python factory-created objects.
- Added `==` and `!=` support on core objects.
- Added Linux `aarch64` precompiled files.

## [0.1.5] - 2023-12-09

### Added

- Made all Symusic objects pickleable.
- Added `zpp_bits` as the pickle backend.
- Added the `ScoreFactory(tpq)` constructor path.
- Added `PedalFactory`.
- Added the read-only `ttype` property to classes.

## [0.1.4] - 2023-12-07

### Changed

- Added an `inplace` argument to most batch operations.
- Removed older dedicated `*_inplace` style interfaces such as `sort_inplace`.

## [0.1.3] - 2023-12-06

### Added

- Added `Pedal` support.

### Fixed

- Fixed `pathlib.Path` parsing when loading MIDI from files.
- Removed redundant `PitchBend` debug output.

## [0.1.2] - 2023-12-05

### Added

- Added `key` and `reverse` parameters for sort functions.
- Added Python-side factory classes to dispatch containers such as `Score` and `Note`.

### Changed

- Renamed `time_unit` to `ttype`.
- Allowed `ttype` to be passed as either strings or explicit time-unit singletons.

## [0.1.1] - 2023-12-02

### Added

- Added missing bindings such as `Track.program` and `Track.is_drum`.
- Added `sort` and `sort_inplace` across list types.
- Regenerated stub files for the release line.

### Changed

- Changed `Note.end()` back into the `Note.end` property while keeping `Note.end_time()`.
- Renamed `score.typing` to `score.unit`.

### Fixed

- Fixed duration decoding problems.

## [0.1.0] - 2023-12-02

### Added

- Introduced the template-based time-unit architecture for `Tick`, `Quarter`, and `Second`.
- Added high-level constructors such as `symusic.Note(...)` that dispatch to `symusic.core.*`
  specializations.

### Changed

- Established the split between user-facing factory call sites and concrete `symusic.core` classes.
