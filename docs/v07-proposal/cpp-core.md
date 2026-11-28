# Native C++ API & Language Standard

symusic 0.7 establishes a C++-first **C++20** core (`symusic::symusic`, default `ValueStorage`). The Python binding uses it in 0.7; the C ABI, JS/WASM and Go bindings that follow share it ([JS & CLI direction](./js-cli.md)). The core links no `anchorvec`, no `zpp_bits`, no `libcurl`/TLS and nothing under the GPL. Behaviour that every binding must reproduce, such as clip windows, rounding and MIDI export, is specified once in [semantics.md](./semantics.md); this page gives the C++ spelling.

## Layered Architecture

```text
bindings/python (nanobind + anchorvec SharedStorage + private C++20 SYMZ v3 pickle)
bindings/c (C99 ABI)   bindings/js (Node, WASM; ValueStorage)    bindings/go (cgo)   C++ (find_package)
        │                              │                             │                   │
        └──────────────────────────────┴──────────────┬──────────────┴───────────────────┘
                                                      ▼
                       symusic::symusic  (Pure C++20 Core · ValueStorage)
              Score<T> · Track · detail/event_fields.h · EventColumns<E> · Pianoroll
          ┌─────────────────────┬─────────────────────────┬─────────────────────────┐
          ▼                     ▼                         ▼                         ▼
  minimidi (PRIVATE)    symusic::abc (MIT)       symusic::synth (LGPL)     assets/soundfonts.json
  Tier-1 SMF wire       extras/abcmidi GPLv2     dlopen / span<const u8>   per-binding stdlib fetch
```

## Native C++ API Alignment

Reconciling `origin/07/pr6-api` headers with finalized Python 0.7 ([RFC #136](./rfc-136.md)) while keeping idioms genuinely native C++20:

| Area | Current (`07/pr6-api`) | Finalized Native C++ 0.7 |
|---|---|---|
| Defaults & naming | `Score<T, S = ValueStorage>`, public mutable `Score::ticks_per_quarter`, `Tempo<T>` (`microseconds_per_quarter`), `TextMeta<T>`, `get_beats()` / `get_downbeats()` | `Score<T = Tick, S = ValueStorage>` as a `class` whose resolution is the read-only `tpq()` (set by the constructor; only `resample()` produces a different one), `Tempo<T = Tick>` (`mspq`), `TextEvent<T>`, `beats(start=0)` / `downbeats(start=0)` |
| Event constructors & `Note` defaults | `i8` pitch/velocity; constructors take the storage types, so `Note(0, 480, 300, 64)` wraps; the constructor with arguments requires `velocity`, and default construction sets it to `0`; redundant `start()` | Checked constructors: musical integers arrive as `i64` and are validated before narrowing (`pitch` and `velocity` `0..127`, `duration >= 0`), so out-of-range values throw `std::invalid_argument` instead of wrapping. `i8` storage, default `velocity = 64` (`0x40`), drop `start()` (keep `.time`, `.end()`). Fields stay public; assigning one is unchecked, and every encoder validates what it writes. MIDI export skips velocity-0 notes, which SMF cannot encode, and rejects a `tpq` outside `1..32767` ([MIDI export](./semantics.md#midi-export)) |
| `Tempo` construction | `Tempo::from_qpm(t, 120.0)` exists, but brace initialization `Tempo<Tick>{0, 120}` silently means `120 µs/quarter` (500,000 QPM) | `Tempo` has no two-number constructor, so `Tempo<Tick>{0, 120}` no longer compiles. Every construction names its unit: `Tempo::from_qpm(t, 120.0)` or `Tempo::from_mspq(t, 500000)` (`mspq` = microseconds per quarter); static converters `qpm_to_mspq` / `mspq_to_qpm` |
| `clip`/`trim` & `adjust_time` | `clip(start, end, bool clip_end)`; `trim(start, end, min_overlap, start_mode, end_mode)` with string modes; mutating free `void ops::adjust_time` | `clip(start, end, const ClipOptions& = {})`; `ClipOptions` holds `head` (default `ClipMode::Drop`) and `tail` (default `ClipMode::Keep`) of `enum class ClipMode { Keep, Drop, Cut }`; `trim` and `min_overlap` go; returning `score.adjust_time(orig, target)` taking `span`. Both follow [clip](./semantics.md#clip) and [adjust_time](./semantics.md#warping-time) |
| File I/O & members | Free `read_score(path, format, decode_abc)` and `read_score_file(path)` returning bytes; free `convert` / `resample` / `pianoroll` | MIDI only in the core: `Score::from_midi(bytes, opts)` / `to_midi()` and `Score::from_file(path, opts)` / `to_file(path)`. ABC is a set of free functions in the optional `symusic::abc` adapter ([boundaries](./cpp-boundaries.md)), so a build without it fails to compile rather than to link; the Python `Score.from_file` dispatches on the extension or `format`. Members `to<To>()`, single-step `resample()`, `Tick`-only `pianoroll()` |
| `Pianoroll` & `Synthesizer` | `operator()` returns `T*`, non-`const get()`, `release()` hands out a raw `uint8_t*` for `delete[]`; no score-level declaration; `Synthesizer` requires absolute `library_path`; `render(score, bool stereo = true)` | `operator()` returns `T&` (+ `const`), `const get()`, `span` view; `take() &&` moves the array out as `vec<pianoroll_t>`; `ScorePianoroll` declared in full. `render(score, const RenderOptions& = {})` with `channels` (`Mono`, `Stereo`), an exact `sample_frames` length and `tail_seconds`; `Synthesizer(soundfont, const SynthSettings& = {})` (+ `span<const u8>` via `fluid_sfloader_t`) moves optional `library_path` into `SynthSettings` (explicit absolute path → co-located/bundled FluidSynth → system fallback) and adds `gain = 0.2`, `interpolation` aligned to FluidSynth 2.6 (`SynthInterpolation::{None, Linear, FourthOrder, Mid, High, Highest}`, default `FourthOrder`), and `cpu_cores = 1` |
| Emptiness idiom | `Score::empty()`, `Track::empty()`, `Note::empty()` | Uniform `is_*` predicates in C++ and Python: `Score::is_empty()`, `Track::is_empty()`, `Note::is_silent()` ([emptiness](./semantics.md#bounds-and-emptiness); only STL sequence containers like `std::vector` / `anchorvec` keep `.empty()`). Later bindings follow their host's predicate spelling |
| Sorting | `sort(bool reverse = false)` | `sort(SortOrder order = SortOrder::Ascending)`, because a setting is never a `bool` parameter ([conventions](../development/conventions.md#language-rules)); Python keeps `reverse=` as `list.sort` does. Stable, with the keys in [sort keys](./semantics.md#sort-keys) |
| Copies | A `copy()` member on every event, track and score | No `copy()` member: value types copy with their copy constructor (`auto y = x;`), which with `ValueStorage` shares nothing. Python keeps `copy()` |

> [!WARNING]
> **7 verified defects caught during the `07/pr6-api` header audit**
>
> - **`std::numeric_limits<f32>::min()` instead of `lowest()`** in `Track::end` / `ops::end` (`Score::end()` already uses `lowest()`) — breaks negative floating-point timestamps (add regression test for all-negative + empty track returning `0`).
> - **`HashDuration` typo** (`HasDuration`) in public concepts.
> - **Global `fmt::formatter<std::shared_ptr<T>>`** specialization in `repr.h` — ODR hazard for any host project using `fmt`.
> - **`checked_event_field` compares via `f64`.** Every current destination field's bounds are exact in `f64`, so no input is misjudged today; a future `i64` destination would lose exact bounds checking above `2^53`. Compare integers with `std::in_range` instead.
> - **`read_score()` cannot accept `MidiReadOptions`**: its MIDI branch always uses the defaults, so a file reader cannot opt out of strict parsing.
> - **Unused `#include "pdqsort.h"`** leaked by `ops.h`.
> - **`minimidi` `Message::channel()` on `0xFF` Meta events** masks the low nibble (`0xF`), misassigning lyrics to channel 15 on the no-notes fallback in `ScoreBuilder::finish_group`.

## Decision: Stay on C++20

> [!TIP]
> **Finalized Decision**
>
> `symusic`, `minimidi` and `anchorvec` stay on **C++20** (`cxx_std_20`). Lowering to C++17 would help only hosts that cannot compile C++20: Android NDK r23–r25, whose libc++ lacks `std::ranges::sort`, hosts pinned to `/std:c++17` such as ROS 2 and older JUCE projects, and old Xcode toolchains. The planned C ABI, JS/WASM and Go `cgo` bindings consume a compiled library and do not depend on its language standard. The cost would be rewriting about 178 constrained template parameters, 23 `requires` clauses and 125 `std::span` uses at #134.
>
> Lowering later stays possible without breaking callers as long as public signatures do not change. The one exception is vocabulary types in signatures: `std::span` must either stay or be replaced before 0.7 freezes the API, so that choice is settled now: public signatures keep `std::span`.

`zpp_bits` (6,058-line header) is still removed from the core, for a different reason: it serves only the Python pickle. C++ interchange uses MIDI, and `bindings/python/` owns a private C++20 little-endian **`SYMZ v3`** codec compiled into `symusic._core`. When an event type's in-memory bytes equal its canonical little-endian wire record and its list is flat (`anchorvec` `is_flat()`), the codec copies the list in one `memcpy`; other lists are encoded field by field. The target is pickling no slower than 0.6, measured before the codec merges.

## Finalized C++ 0.7 API Synopsis

Complete public C++20 declarations for every finalized and changed type in `namespace symusic` (pure `ValueStorage` default; `SharedStorage` lives in `bindings/python/`). `Tick`, `Quarter` and `Second` are the only time units, and `ValueStorage` plus the binding's private `SharedStorage` the only storages: the template parameters are not extension points, and other arguments are unsupported even where they compile. The behaviour behind each declaration is in [semantics.md](./semantics.md).

### Time Units, Concepts, Enums & Options

```cpp
namespace symusic {

struct Tick    : TimeUnit<i32> {};
struct Quarter : TimeUnit<f32> {};
struct Second  : TimeUnit<f32> {};

template<typename T> concept TType       = std::is_base_of_v<TimeUnit<typename T::unit>, T>;
template<typename T> concept TimeEvent   = std::is_base_of_v<TimeStamp<typename T::ttype>, T>;
template<typename T> concept HasDuration = requires(T t) { { t.duration } -> std::convertible_to<typename T::unit>; }; // fixes HashDuration

struct ValueStorage; // default storage policy (std::vector<E>, tracks by value)

class ParseError : public std::runtime_error { using std::runtime_error::runtime_error; };

enum class DataFormat : u8 { MIDI, ABC };
struct MidiReadOptions {
    bool strict = true; // true: reject MIDI channel data bytes > 127; false: clamp to 127
};

enum class SortOrder : u8 { Ascending, Descending };

enum class ClipMode : u8 { Keep, Drop, Cut };
struct ClipOptions {
    ClipMode head = ClipMode::Drop; // note extending before start
    ClipMode tail = ClipMode::Keep; // note extending past end
};

enum class PianorollMode : u8 { Onset, Frame, Offset };
struct PianorollOptions {
    vec<PianorollMode> modes           = {PianorollMode::Frame, PianorollMode::Onset}; // axis 0 follows this order
    std::pair<u8, u8>  pitch_range     = {0, 128}; // half-open [low, high), 0 <= low < high <= 128
    bool               encode_velocity = false;
};

enum class AudioChannels : u8 { Mono = 1, Stereo = 2 };
enum class AudioEncoding : u8 { Pcm16, Float32 }; // WAV sample encoding (zero-dep MIT)

// 1-to-1 with FluidSynth 2.6+ fluid_interp (FLUID_INTERP_7THORDER deprecated upstream)
enum class SynthInterpolation : int {
    None        = 0, // FLUID_INTERP_NONE
    Linear      = 1, // FLUID_INTERP_LINEAR
    FourthOrder = 4, // FLUID_INTERP_4THORDER (default, 4-point cubic)
    Mid         = 5, // FLUID_INTERP_MID (11th-order Hann-windowed sinc)
    High        = 6, // FLUID_INTERP_HIGH (17th-order Hann-windowed sinc)
    Highest     = 7, // FLUID_INTERP_HIGHEST (25th-order Hann-windowed sinc)
};

// Fixed for the lifetime of a Synthesizer.
struct SynthSettings {
    int                   sample_rate   = 44100;                           // 8000..96000 Hz
    int                   polyphony     = 256;                             // 1..65535
    f64                   gain          = 0.2;                             // 0.0..10.0 (synth.gain)
    SynthInterpolation    interpolation = SynthInterpolation::FourthOrder; // FluidSynth 2.6+
    bool                  reverb        = false;                           // synth.reverb.active
    bool                  chorus        = false;                           // synth.chorus.active
    int                   cpu_cores     = 1;   // 1..256 (synth.cpu-cores); speed only: a FluidSynth built without threads renders on one core
    std::filesystem::path library_path  = {}; // empty => co-located/bundled -> system fallback; non-empty => must be absolute
};

// Chosen per render() call.
struct RenderOptions {
    AudioChannels         channels      = AudioChannels::Stereo;
    std::optional<size_t> sample_frames = {};  // exact output length; empty => natural length
    f64                   tail_seconds  = 1.0; // >= 0, finite; part of the natural length, ignored with sample_frames
};

} // namespace symusic
```

### Events (`event.h`)

All events inherit `TimeStamp<T>` (`using ttype = T; using unit = typename T::unit; unit time = 0;`), default `T = Tick`, and are structs with public fields and checked constructors. Time and duration arrive as `unit`; every other numeric parameter arrives as `i64` or `f64` and is validated before it is narrowed to the field's type, so `Note(0, 480, 300)` throws `std::invalid_argument` instead of storing `44`. Durations are `>= 0`. Assigning a field directly is unchecked: keeping it valid is the caller's precondition for correct results, never for memory safety, and encoders check every value they write ([conventions](../development/conventions.md#design-rules)). Events compare by value (`operator==`), copy by value (no `copy()` member), and drop `serialize()`/`deserialize()`, `operator->`/`operator*`, and redundant `start()`.

```cpp
namespace symusic {

template<TType T = Tick>
struct Note : TimeStamp<T> {
    typename T::unit duration{0};
    i8               pitch{0};
    i8               velocity{64}; // default 64 (MIDI 1.0 0x40)

    Note() = default;
    Note(typename T::unit time, typename T::unit duration, i64 pitch, i64 velocity = 64); // pitch, velocity 0..127

    [[nodiscard]] typename T::unit end() const;        // checked add_time(time, duration)
    [[nodiscard]] bool             is_silent() const;  // duration == 0 || velocity == 0 (replaces empty())
    void                           shift_pitch(i64 offset);    // result must stay 0..127
    void                           shift_velocity(i64 offset); // result must stay 0..127
    [[nodiscard]] std::string      to_string() const;
    bool                           operator==(const Note&) const = default;
};

template<TType T = Tick>
struct Pedal : TimeStamp<T> { // returned by Track::pedals(); never stored
    typename T::unit duration{0};

    Pedal() = default;
    Pedal(typename T::unit time, typename T::unit duration);

    [[nodiscard]] typename T::unit end() const;
    [[nodiscard]] std::string      to_string() const;
    bool                           operator==(const Pedal&) const = default;
};

template<TType T = Tick>
struct ControlChange : TimeStamp<T> {
    u8 number{0}, value{0};

    ControlChange() = default;
    ControlChange(typename T::unit time, i64 number, i64 value); // both 0..127

    [[nodiscard]] std::string to_string() const;
    bool                      operator==(const ControlChange&) const = default;
};

template<TType T = Tick>
struct TimeSignature : TimeStamp<T> {
    u8 numerator{4}, denominator{4};

    TimeSignature() = default;
    TimeSignature(typename T::unit time, i64 numerator, i64 denominator); // numerator 1..255; denominator a power of 2 in 1..128

    [[nodiscard]] std::string to_string() const;
    bool                      operator==(const TimeSignature&) const = default;
};

template<TType T = Tick>
struct KeySignature : TimeStamp<T> {
    i8   fifths{0};    // -7..7 sharps/flats (replaces key)
    bool minor{false}; // replaces i8 tonality

    KeySignature() = default;
    KeySignature(typename T::unit time, i64 fifths, bool minor = false);

    [[nodiscard]] u8          tonic() const; // (7 * fifths + 9 * minor) mod 12 (floored, 0..11; replaces degree())
    [[nodiscard]] std::string to_string() const;
    bool                      operator==(const KeySignature&) const = default;
};

template<TType T = Tick>
struct Tempo : TimeStamp<T> {
    i32 mspq{500000}; // microseconds per quarter, > 0; MIDI export requires <= 16777215 (reverts #134's microseconds_per_quarter)

    Tempo() = default; // no two-number constructor: Tempo<Tick>{0, 120} does not compile
    [[nodiscard]] static Tempo         from_qpm(typename T::unit time, f64 qpm);
    [[nodiscard]] static Tempo         from_mspq(typename T::unit time, i64 mspq);
    [[nodiscard]] static i32           qpm_to_mspq(f64 qpm); // round half away from zero, then range-checked
    [[nodiscard]] static constexpr f64 mspq_to_qpm(i32 mspq);

    [[nodiscard]] f64         qpm() const;
    void                      set_qpm(f64 qpm); // void (no longer returns Tempo&)
    [[nodiscard]] std::string to_string() const;
    bool                      operator==(const Tempo&) const = default;
};

template<TType T = Tick>
struct PitchBend : TimeStamp<T> {
    i32 value{0};

    PitchBend() = default;
    PitchBend(typename T::unit time, i64 value); // -8192..8191

    [[nodiscard]] std::string to_string() const;
    bool                      operator==(const PitchBend&) const = default;
};

template<TType T = Tick>
struct TextEvent : TimeStamp<T> { // renamed from TextMeta
    std::string text{}; // UTF-8

    TextEvent() = default;
    TextEvent(typename T::unit time, std::string text);

    [[nodiscard]] std::string to_string() const;
    bool                      operator==(const TextEvent&) const = default;
};

template<TimeEvent E>
void shift_time(E& event, typename E::unit offset);

} // namespace symusic
```

### `Track` & `Score` (`track.h`, `score.h`)

No `*_inplace` methods, no `trim()`, no `ScoreNative`/`TrackNative`, no `serialize()`/`deserialize()`, no `copy()`. Mutators return `void` with the strong exception guarantee; transforms are `[[nodiscard]] const` and return a fresh value that shares nothing with the receiver, even when nothing changes (`to<Tick>()` on a `Tick` score, `resample()` to the same `tpq`).

```cpp
namespace symusic {

template<TType T = Tick, typename S = ValueStorage>
struct Track {
    using ttype        = T;
    using unit         = typename T::unit;
    using storage_type = S;

    std::string                                 name;
    u8                                          program = 0; // 0..127
    bool                                        is_drum = false;
    typename S::template List<Note<T>>          notes;
    typename S::template List<ControlChange<T>> controls;
    typename S::template List<PitchBend<T>>     pitch_bends;
    typename S::template List<TextEvent<T>>     lyrics;      // stored pedals removed

    Track() = default;
    Track(std::string name, i64 program = 0, bool is_drum = false);
    Track(std::string name, i64 program, bool is_drum,
          typename S::template List<Note<T>>          notes,
          typename S::template List<ControlChange<T>> controls,
          typename S::template List<PitchBend<T>>     pitch_bends,
          typename S::template List<TextEvent<T>>     lyrics);

    void set_program(i64 value); // validates 0..127

    // Queries
    [[nodiscard]] unit          start() const;
    [[nodiscard]] unit          end() const;
    [[nodiscard]] bool          is_empty() const;  // no events in any list (replaces empty())
    [[nodiscard]] vec<Pedal<T>> pedals() const;    // derived on demand from CC64 in controls
    [[nodiscard]] std::string   to_string() const;
    [[nodiscard]] std::string   summary() const;
    bool                        operator==(const Track& other) const;

    // In-place mutators (void, strong guarantee)
    void sort(SortOrder order = SortOrder::Ascending); // stable, on every event list
    void shift_time(unit offset);
    void shift_pitch(i64 offset);
    void shift_velocity(i64 offset);

    // Returning transforms (leave receiver unchanged)
    [[nodiscard]] Track          clip(unit start, unit end, const ClipOptions& opts = {}) const;
    [[nodiscard]] Track          adjust_time(std::span<const unit> original, std::span<const unit> target) const;
    [[nodiscard]] TrackPianoroll pianoroll(const PianorollOptions& opts = {}) const requires std::same_as<T, Tick>;
};

template<TType T = Tick, typename S = ValueStorage>
class Score {
public:
    using ttype        = T;
    using unit         = typename T::unit;
    using storage_type = S;
    using track_type   = Track<T, S>;

    typename S::template Tracks<track_type>     tracks;
    typename S::template List<TimeSignature<T>> time_signatures;
    typename S::template List<KeySignature<T>>  key_signatures;
    typename S::template List<Tempo<T>>         tempos;
    typename S::template List<TextEvent<T>>     markers;

    Score() = default; // tpq() == 960
    explicit Score(i64 tpq); // >= 1
    Score(i64 tpq,
          typename S::template Tracks<track_type>     tracks,
          typename S::template List<TimeSignature<T>> time_signatures,
          typename S::template List<KeySignature<T>>  key_signatures,
          typename S::template List<Tempo<T>>         tempos,
          typename S::template List<TextEvent<T>>     markers);

    // MIDI only; ABC is symusic::abc. from_file/to_file report file errors as std::filesystem::filesystem_error.
    [[nodiscard]] static Score from_midi(std::span<const u8> bytes, const MidiReadOptions& opts = {});
    [[nodiscard]] static Score from_file(const std::filesystem::path& path, const MidiReadOptions& opts = {});
    [[nodiscard]] vec<u8>      to_midi() const; // requires tpq() in 1..32767
    void                       to_file(const std::filesystem::path& path) const;

    // Queries
    [[nodiscard]] i32         tpq() const;          // ticks per quarter (replaces ticks_per_quarter)
    [[nodiscard]] unit        start() const;
    [[nodiscard]] unit        end() const;
    [[nodiscard]] size_t      note_count() const;   // replaces note_num()
    [[nodiscard]] size_t      track_count() const;  // replaces track_num()
    [[nodiscard]] bool        is_empty() const;     // no events anywhere; empty tracks do not count (replaces empty())
    [[nodiscard]] vec<unit>   beats(unit start = 0) const;     // replaces get_beats()
    [[nodiscard]] vec<unit>   downbeats(unit start = 0) const; // replaces get_downbeats()
    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] std::string summary() const;
    bool                      operator==(const Score& other) const;

    // In-place mutators (void, strong guarantee)
    void sort(SortOrder order = SortOrder::Ascending); // stable, on every event list; keeps track order
    void shift_time(unit offset);
    void shift_pitch(i64 offset);    // every track, drums included
    void shift_velocity(i64 offset);

    // Returning transforms (leave receiver unchanged)
    [[nodiscard]] Score          clip(unit start, unit end, const ClipOptions& opts = {}) const;
    [[nodiscard]] Score          adjust_time(std::span<const unit> original, std::span<const unit> target) const;
    template<TType To>
    [[nodiscard]] Score<To, S>   to(typename To::unit min_duration = 0) const;    // floor in the target unit
    [[nodiscard]] Score<Tick, S> resample(i64 tpq, Tick::unit min_duration = 0) const; // one rounding step
    [[nodiscard]] ScorePianoroll pianoroll(const PianorollOptions& opts = {}) const requires std::same_as<T, Tick>;

private:
    i32 tpq_ = 960;
};

} // namespace symusic
```

### `EventColumns`, `Pianoroll` & `Synthesizer` (`soa.h`, `pianoroll.h`, `synth.h`)

A pianoroll owns one dense `pianoroll_t` (`u8`) array in row-major order. Pointers, references and spans from `data()`, `span()` and `operator()` stay valid until the roll is destroyed, moved from or taken; `clear()` zeroes the values and keeps them valid. After `std::move(roll).take()`, every dimension is `0` and `span()` is empty.

`render()`, `AudioBuffer`, `to_wav()` and `write_wav()` follow [rendering](./semantics.md#rendering) and [audio buffers and WAV](./semantics.md#audio-buffers-and-wav); their `ValueError` is `std::invalid_argument`, `OverflowError` is `std::overflow_error`, and file errors are `std::filesystem::filesystem_error`.

```cpp
namespace symusic {

// Replaces 8 legacy *Arr structs (NoteArr, PedalArr, ...)
template<TimeEvent E>
class EventColumns {
public:
    explicit EventColumns(const auto& events);
    void for_each(auto&& visit); // visit(const char* field_name, vec<ColumnValue>& column)
};

class TrackPianoroll { // (mode, pitch, time)
public:
    TrackPianoroll(size_t mode_dim, size_t pitch_dim, size_t time_dim);
    [[nodiscard]] pianoroll_t*                 data();
    [[nodiscard]] const pianoroll_t*           data() const;
    [[nodiscard]] std::span<pianoroll_t>       span();
    [[nodiscard]] std::span<const pianoroll_t> span() const;
    [[nodiscard]] pianoroll_t&                 operator()(size_t mode, size_t pitch, size_t time);
    [[nodiscard]] const pianoroll_t&           operator()(size_t mode, size_t pitch, size_t time) const;
    [[nodiscard]] pianoroll_t                  get(size_t mode, size_t pitch, size_t time) const;
    void                                       set(size_t mode, size_t pitch, size_t start, size_t duration, pianoroll_t value);
    void                                       clear();
    [[nodiscard]] std::tuple<size_t, size_t, size_t> dims() const;
    [[nodiscard]] vec<pianoroll_t>             take() &&; // replaces release()
};

class ScorePianoroll { // (mode, track, pitch, time)
public:
    ScorePianoroll(size_t mode_dim, size_t track_dim, size_t pitch_dim, size_t time_dim);
    [[nodiscard]] pianoroll_t*                 data();
    [[nodiscard]] const pianoroll_t*           data() const;
    [[nodiscard]] std::span<pianoroll_t>       span();
    [[nodiscard]] std::span<const pianoroll_t> span() const;
    [[nodiscard]] pianoroll_t&                 operator()(size_t mode, size_t track, size_t pitch, size_t time);
    [[nodiscard]] const pianoroll_t&           operator()(size_t mode, size_t track, size_t pitch, size_t time) const;
    [[nodiscard]] pianoroll_t                  get(size_t mode, size_t track, size_t pitch, size_t time) const;
    void                                       set(size_t mode, size_t track, size_t pitch, size_t start, size_t duration, pianoroll_t value);
    void                                       clear();
    [[nodiscard]] std::tuple<size_t, size_t, size_t, size_t> dims() const;
    [[nodiscard]] vec<pianoroll_t>             take() &&;
};

struct AudioBuffer {
    vec<f32> samples;          // interleaved frames (L, R, L, R, ... for stereo)
    size_t   channels    = 0;  // 1 (Mono) or 2 (Stereo)
    int      sample_rate = 0;  // Hz (carried from SynthSettings::sample_rate)

    [[nodiscard]] size_t  sample_frames() const; // samples.size() / channels; 0 when channels == 0
    [[nodiscard]] vec<u8> to_wav(AudioEncoding encoding = AudioEncoding::Pcm16) const; // zero-dep MIT WAV bytes
};

// Zero-dependency MIT WAV file writer (shared across C++, C ABI, Python, Go, JS/WASM)
void write_wav(
    const std::filesystem::path& path,
    const AudioBuffer&           audio,
    AudioEncoding                encoding = AudioEncoding::Pcm16
);

class Synthesizer {
public:
    // FluidSynth 2.6.2+ loaded at runtime (explicit library_path -> co-located/bundled -> system fallback)
    explicit Synthesizer(const std::filesystem::path& soundfont, const SynthSettings& settings = {});
    explicit Synthesizer(std::span<const u8> soundfont_bytes, const SynthSettings& settings = {}); // copies the bytes

    // Each call starts from fresh synthesizer state; concurrent calls are safe while no thread mutates `score`.
    template<TType T, typename S>
    [[nodiscard]] AudioBuffer render(const Score<T, S>& score, const RenderOptions& options = {}) const;
};

} // namespace symusic
```

## C++ Before / After

**0.7 Native C++20 Proposal (planned API)**

```cpp
#include <span>
#include <symusic.h>
using namespace symusic;

int main() {
    auto score = Score<>::from_file("piece.mid", {.strict = false});

    auto tempo = Tempo<>::from_qpm(0, 120.0);  // names its unit
    auto window = score.clip(0, 1920, {.tail = ClipMode::Cut});  // cut notes at end

    const Tick::unit original[] = {0, 960, 1920};
    const Tick::unit target[] = {0, 1200, 1920};
    auto warped = window.adjust_time(std::span(original), std::span(target));
    auto resampled = warped.resample(480);  // one rounding step

    Synthesizer synth("gm.sf3", {.sample_rate = 44100, .interpolation = SynthInterpolation::Highest});
    AudioBuffer audio = synth.render(resampled, {.sample_frames = 44100 * 30});  // exactly 30 s, stereo
    write_wav("out.wav", audio);  // zero-dependency WAV (Pcm16 default; or audio.to_wav())
}
```

**0.7 Current (PRs #129–#134)**

```cpp
#include "symusic.h"
using namespace symusic;

// Score<Tick> defaults to ValueStorage (pure std::vector, zero shared_ptr)
auto bytes = read_score_file("piece.mid");
auto score = Score<Tick>::from_midi(
    bytes, {.invalid_data_bytes = InvalidDataBytes::Clamp}
);

auto window = score.clip(0, 1920);
EventColumns<Note<Tick>> cols(window.tracks[0].notes);
auto pedals = window.tracks[0].pedals();  // derived from CC64
auto midi_out = window.to_midi();

// Runtime-loaded FluidSynth (dlopen / LoadLibraryExW)
Synthesizer synth(lib_path, "gm.sf3", SynthSettings{.sample_rate = 44100});
AudioBuffer audio = synth.render(window, /*stereo=*/true);
```

**0.6 (Legacy)**

```cpp
#include "symusic.h"
using namespace symusic;

auto bytes = read_file("piece.mid");
auto shared = Score<Tick>::parse<DataFormat::MIDI>(bytes);
shared.clip_inplace(0, 1920);
ScoreNative<Tick> score = to_native(shared);  // free function in 0.6

NoteArr<Tick> cols(score.tracks[0].notes);
auto midi_out = score.dumps<DataFormat::MIDI>();

Synthesizer synth("gm.sf3", 44100, 4);
psynth::AudioData audio = synth.render(shared, true);
```
