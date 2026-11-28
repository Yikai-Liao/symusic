# Native C++ API & Language Standard

symusic 0.7 establishes a C++-first **C++20** core (`symusic::symusic`, default `ValueStorage`). The Python binding uses it in 0.7; the Node/Bun, WASM and Go bindings that follow share it ([JS & CLI direction](./js-cli.md)). Public headers under `include/symusic/` expose declarations only and depend solely on the C++20 standard library: `minimidi` and `{fmt}` are `PRIVATE` implementation dependencies, storage-generic `*_impl.h` templates live in repo-private `src/`, and the core links no `anchorvec`, no `zpp_bits`, no `libcurl`/TLS and nothing under the GPL. Behaviour that every binding must reproduce, such as clip windows, rounding and MIDI export, is specified once in [semantics.md](./semantics.md); this page gives the C++ spelling.

## Layered Architecture

```text
bindings/python (nanobind + anchorvec SharedStorage + private C++20 SYMZ v3 pickle)
bindings/js (Node & Bun N-API, WASM; ValueStorage)   bindings/c & bindings/go (cgo)   C++ (find_package)
        │                                                           │                         │
        └─────────────────────────────┬─────────────────────────────┴─────────────────────────┘
                                      ▼
               symusic::symusic  (Pure C++20 Core · ValueStorage · Stdlib-Only Public Headers)
                     Score<T> · Track · Event · EventColumns<E> · Pianoroll
          ┌─────────────────────┬─────────────────────────┬─────────────────────────┐
          ▼                     ▼                         ▼                         ▼
  minimidi + fmt        symusic::abc (MIT)       symusic::synth (LGPL)     assets/soundfonts.json
  (PRIVATE to src/)     extras/abcmidi GPLv2     dlopen / injected table   per-binding stdlib fetch
```

## Native C++ API Alignment

Reconciling `origin/07/pr6-api` headers with finalized Python 0.7 ([RFC #136](./rfc-136.md)) while keeping idioms genuinely native C++20:

| Area | Current (`07/pr6-api`) | Finalized Native C++ 0.7 |
|---|---|---|
| Defaults & naming | `Score<T, S = ValueStorage>`, public mutable `Score::ticks_per_quarter`, `Tempo<T>` (`microseconds_per_quarter`), `TextMeta<T>`, `get_beats()` / `get_downbeats()` | Single-parameter `Score<T = Tick>` and `Track<T = Tick>` (backed by `detail::BasicScore<T, S>` / `detail::BasicTrack<T, S>` so the storage policy never enters public C++ signatures), `Score` as a `class` whose resolution is the read-only `tpq()` (set by the constructor; only `resample()` produces a different one), `Tempo<T = Tick>` (`mspq`), `TextEvent<T>`, `beats(start=0)` / `downbeats(start=0)` |
| Event constructors & `Note` defaults | `i8` pitch/velocity; constructors take the storage types, so `Note(0, 480, 300, 64)` wraps; the constructor with arguments requires `velocity`, and default construction sets it to `0`; redundant `start()` | Checked wide inputs across constructors and scalar time methods (`shift_time`, `clip`, `beats`, `downbeats`, `to`, `resample`, `convert_time`): `Tick` times and durations arrive as `i64` (with floating-point overloads `= delete`d, throwing `std::overflow_error` outside `i32`), `Quarter`/`Second` times and durations arrive as `f64` (`std::invalid_argument` on NaN/Inf, `std::overflow_error` outside finite `f32`), and musical integers arrive as `i64` (`pitch` and `velocity` `0..127`, `duration >= 0`). `i8` storage, default `velocity = 64` (`0x40`), drop `start()` (keep `.time`, `.end()`). Fields stay public; assigning one is unchecked, and every encoder validates what it writes. MIDI export skips silent notes (`velocity == 0` or `duration == 0`) and rejects a `tpq` outside `1..32767` ([MIDI export](./semantics.md#midi-export)) |
| `Tempo` construction | `Tempo::from_qpm(t, 120.0)` exists, but brace initialization `Tempo<Tick>{0, 120}` silently means `120 µs/quarter` (500,000 QPM) | `Tempo` has no two-number constructor, so `Tempo<Tick>{0, 120}` no longer compiles. Every construction names its unit: `Tempo::from_qpm(t, 120.0)` or `Tempo::from_mspq(t, 500000)` (`mspq` = microseconds per quarter); static converters `qpm_to_mspq` / `mspq_to_qpm` |
| `clip`/`trim` & `adjust_time` | `clip(start, end, bool clip_end)`; `trim(start, end, min_overlap, start_mode, end_mode)` with string modes; mutating free `void ops::adjust_time` | `clip(start, end, const ClipOptions& = {})`; `ClipOptions` holds `head` (default `ClipMode::Drop`) and `tail` (default `ClipMode::Keep`) of `enum class ClipMode { Keep, Drop, Cut }`; `trim`, `min_overlap` and mutating `ops::*` helpers go; returning `score.adjust_time(orig, target)` taking `span`. Both follow [clip](./semantics.md#clip) and [adjust_time](./semantics.md#warping-time) |
| File I/O, free helpers & members | Free `read_file`/`write_file`, `read_score(path, format, decode_abc)` and `read_score_file(path)` returning bytes; free `convert` / `resample` / `pianoroll`; public `repr.h` pulling `<fmt/...>` | Minimal public header surface: `Score::from_midi(bytes, opts)` / `to_midi()` and `Score::from_file(path, opts)` / `to_file(path)` (MIDI only; raw `read_file`/`write_file`, `read_score`, `serialize`/`deserialize`, and `<fmt/...>` leave public headers). ABC is free functions in the optional `symusic::abc` adapter ([boundaries](./cpp-boundaries.md)). Members `to<To>()` (explicitly instantiated for all `3 × 3` unit pairs), single-step `resample()`, `Tick`-only `pianoroll()`; free `sort_tracks`, `sort_events`, `is_sorted`, `convert_time`, `convert_times`, and `events_from_columns` serve `std::vector`/`std::span` |
| `Pianoroll` & `Synthesizer` | `operator()` returns `T*`, non-`const get()`, `release()` hands out a raw `uint8_t*` for `delete[]`; no score-level declaration; `Synthesizer` requires absolute `library_path`; `render(score, bool stereo = true)` | Backed by `vec<pianoroll_t>`: `operator()` returns `T&` (+ `const`), `const get()`, `span` view, O(1) `take() &&`, `ScorePianoroll` declared in full, `std::overflow_error` on volume overflow. `render(score, const RenderOptions& = {})` with `channels` (`Mono`, `Stereo`), `sample_frames`, `tail_seconds`, and cooperative `std::stop_token` cancellation; `Synthesizer(soundfont, const SynthSettings& = {})` (+ `span<const u8>` via `fluid_sfloader_t`) moves optional `library_path` into `SynthSettings` and adds `gain = 0.2`, FluidSynth 2.6 `interpolation`, and `cpu_cores = 1` |
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

Complete public C++20 declarations for every finalized and changed type in `namespace symusic`. `Tick`, `Quarter` and `Second` are the only time units (`T` is not a user extension point, and other arguments are unsupported even where they compile). Storage-generic `detail::BasicTrack<T, S>` and `detail::BasicScore<T, S>` stay internal (`detail::ValueStorage` for C++, C ABI, Go and JS; `py::SharedStorage` in `bindings/python/`), so public `symusic::Track<T = Tick>` and `symusic::Score<T = Tick>` take a single template parameter `T` and expose `vec<E>` / `vec<Track<T>>` directly without leaking a storage parameter into caller code. The behaviour behind each declaration is in [semantics.md](./semantics.md).

### Time Units, Concepts, Enums & Options

```cpp
namespace symusic {

struct Tick    : TimeUnit<i32> {};
struct Quarter : TimeUnit<f32> {};
struct Second  : TimeUnit<f32> {};

template<typename T> concept TType       = std::is_base_of_v<TimeUnit<typename T::unit>, T>;
template<typename T> concept TimeEvent   = std::is_base_of_v<TimeStamp<typename T::ttype>, T>;
template<typename T> concept HasDuration = requires(T t) { { t.duration } -> std::convertible_to<typename T::unit>; }; // fixes HashDuration

class ParseError     : public std::runtime_error { using std::runtime_error::runtime_error; };
class CancelledError : public std::runtime_error { using std::runtime_error::runtime_error; };

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
    std::stop_token       stop_token    = {};  // checked at each render block; throws CancelledError when requested
};

} // namespace symusic
```

### Events (`event.h`)

All events inherit `TimeStamp<T>` (`using ttype = T; using unit = typename T::unit; unit time = 0;`), default `T = Tick`, and are structs with public fields and checked constructors. Constructor parameters arrive wide (`time_input_t<T>`: `i64` for `Tick`, `f64` for `Quarter` and `Second`; `i64` for every musical integer) so nothing wraps or narrows silently at the call site: `Tick` constructors `= delete` floating-point time/duration arguments at compile time and throw `std::overflow_error` outside `i32`; `Quarter`/`Second` constructors throw `std::invalid_argument` on NaN or infinity and `std::overflow_error` outside finite `f32`; musical integers outside range or durations `< 0` throw `std::invalid_argument`. Assigning a field directly is unchecked: keeping it valid is the caller's precondition for correct results, never for memory safety, and encoders check every value they write ([conventions](../development/conventions.md#design-rules)). Events compare by value (`operator==`), stream with `operator<<` (`<iosfwd>` in header, no `<fmt/...>` leak), copy by value (no `copy()` member), and drop `serialize()`/`deserialize()`, `operator->`/`operator*`, and redundant `start()`.

```cpp
namespace symusic {

template<TType T>
using time_input_t = std::conditional_t<std::same_as<T, Tick>, i64, f64>;

template<TType T = Tick>
struct Note : TimeStamp<T> {
    typename T::unit duration{0};
    i8               pitch{0};
    i8               velocity{64}; // default 64 (MIDI 1.0 0x40)

    Note() = default;
    Note(time_input_t<T> time, time_input_t<T> duration, i64 pitch, i64 velocity = 64); // pitch, velocity 0..127
    template<typename U, typename V>
        requires (std::same_as<T, Tick> && (std::floating_point<U> || std::floating_point<V>))
    Note(U, V, i64, i64 = 64) = delete;

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
    Pedal(time_input_t<T> time, time_input_t<T> duration);
    template<typename U, typename V>
        requires (std::same_as<T, Tick> && (std::floating_point<U> || std::floating_point<V>))
    Pedal(U, V) = delete;

    [[nodiscard]] typename T::unit end() const;
    [[nodiscard]] std::string      to_string() const;
    bool                           operator==(const Pedal&) const = default;
};

template<TType T = Tick>
struct ControlChange : TimeStamp<T> {
    u8 number{0}, value{0};

    ControlChange() = default;
    ControlChange(time_input_t<T> time, i64 number, i64 value); // both 0..127
    template<std::floating_point U> requires std::same_as<T, Tick>
    ControlChange(U, i64, i64) = delete;

    [[nodiscard]] std::string to_string() const;
    bool                      operator==(const ControlChange&) const = default;
};

template<TType T = Tick>
struct TimeSignature : TimeStamp<T> {
    u8 numerator{4}, denominator{4};

    TimeSignature() = default;
    TimeSignature(time_input_t<T> time, i64 numerator, i64 denominator); // numerator 1..255; denominator a power of 2 in 1..128
    template<std::floating_point U> requires std::same_as<T, Tick>
    TimeSignature(U, i64, i64) = delete;

    [[nodiscard]] std::string to_string() const;
    bool                      operator==(const TimeSignature&) const = default;
};

template<TType T = Tick>
struct KeySignature : TimeStamp<T> {
    i8   fifths{0};    // -7..7 sharps/flats (replaces key)
    bool minor{false}; // replaces i8 tonality

    KeySignature() = default;
    KeySignature(time_input_t<T> time, i64 fifths, bool minor = false);
    template<std::floating_point U> requires std::same_as<T, Tick>
    KeySignature(U, i64, bool = false) = delete;

    [[nodiscard]] u8          tonic() const; // (7 * fifths + 9 * minor) mod 12 (floored, 0..11; replaces degree())
    [[nodiscard]] std::string to_string() const;
    bool                      operator==(const KeySignature&) const = default;
};

template<TType T = Tick>
struct Tempo : TimeStamp<T> {
    i32 mspq{500000}; // microseconds per quarter, > 0; MIDI export requires <= 16777215 (reverts #134's microseconds_per_quarter)

    Tempo() = default; // no two-number constructor: Tempo<Tick>{0, 120} does not compile
    [[nodiscard]] static Tempo         from_qpm(time_input_t<T> time, f64 qpm);
    [[nodiscard]] static Tempo         from_mspq(time_input_t<T> time, i64 mspq);
    template<std::floating_point U> requires std::same_as<T, Tick>
    static Tempo                       from_qpm(U, f64) = delete;
    template<std::floating_point U> requires std::same_as<T, Tick>
    static Tempo                       from_mspq(U, i64) = delete;
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
    PitchBend(time_input_t<T> time, i64 value); // -8192..8191
    template<std::floating_point U> requires std::same_as<T, Tick>
    PitchBend(U, i64) = delete;

    [[nodiscard]] std::string to_string() const;
    bool                      operator==(const PitchBend&) const = default;
};

template<TType T = Tick>
struct TextEvent : TimeStamp<T> { // renamed from TextMeta
    std::string text{}; // UTF-8

    TextEvent() = default;
    TextEvent(time_input_t<T> time, std::string text);
    template<std::floating_point U> requires std::same_as<T, Tick>
    TextEvent(U, std::string) = delete;

    [[nodiscard]] std::string to_string() const;
    bool                      operator==(const TextEvent&) const = default;
};

template<TimeEvent E>
void shift_time(E& event, time_input_t<typename E::ttype> offset);
template<TimeEvent E, std::floating_point U>
    requires std::same_as<typename E::ttype, Tick>
void shift_time(E&, U) = delete;

} // namespace symusic
```

### `Track` & `Score` (`track.h`, `score.h`)

No `*_inplace` methods, no `trim()`, no `ScoreNative`/`TrackNative`, no `serialize()`/`deserialize()`, no `copy()`. In headers, `Track<T = Tick>` and `Score<T = Tick>` alias `detail::BasicTrack<T, detail::ValueStorage>` and `detail::BasicScore<T, detail::ValueStorage>` (shown expanded below as callers use them); template definitions live in repo-private `src/*_impl.h` and are explicitly instantiated per unit. Mutators return `void` with the strong exception guarantee; transforms are `[[nodiscard]] const` and return a fresh value that shares nothing with the receiver, even when nothing changes (`to<Tick>()` on a `Tick` score, `resample()` to the same `tpq`).

```cpp
namespace symusic {

// Public single-parameter alias: using Track = detail::BasicTrack<T, detail::ValueStorage>;
template<TType T = Tick>
struct Track {
    using ttype = T;
    using unit  = typename T::unit;

    std::string            name;
    u8                     program = 0; // 0..127
    bool                   is_drum = false;
    vec<Note<T>>           notes;
    vec<ControlChange<T>>  controls;
    vec<PitchBend<T>>      pitch_bends;
    vec<TextEvent<T>>      lyrics;      // stored pedals removed

    Track() = default;
    Track(std::string name, i64 program = 0, bool is_drum = false);
    Track(std::string           name,
          i64                   program,
          bool                  is_drum,
          vec<Note<T>>          notes,
          vec<ControlChange<T>> controls,
          vec<PitchBend<T>>     pitch_bends,
          vec<TextEvent<T>>     lyrics);

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
    void shift_time(time_input_t<T> offset);
    template<std::floating_point U> requires std::same_as<T, Tick>
    void shift_time(U) = delete;
    void shift_pitch(i64 offset);
    void shift_velocity(i64 offset);

    // Returning transforms (leave receiver unchanged)
    [[nodiscard]] Track          clip(time_input_t<T> start, time_input_t<T> end, const ClipOptions& opts = {}) const;
    template<typename U, typename V>
        requires (std::same_as<T, Tick> && (std::floating_point<U> || std::floating_point<V>))
    Track                        clip(U, V, const ClipOptions& = {}) const = delete;
    [[nodiscard]] Track          adjust_time(std::span<const unit> original, std::span<const unit> target) const;
    [[nodiscard]] TrackPianoroll pianoroll(const PianorollOptions& opts = {}) const requires std::same_as<T, Tick>;
};

// Public single-parameter alias: using Score = detail::BasicScore<T, detail::ValueStorage>;
template<TType T = Tick>
class Score {
public:
    using ttype      = T;
    using unit       = typename T::unit;
    using track_type = Track<T>;

    vec<Track<T>>         tracks;
    vec<TimeSignature<T>> time_signatures;
    vec<KeySignature<T>>  key_signatures;
    vec<Tempo<T>>         tempos;
    vec<TextEvent<T>>     markers;

    Score() = default; // tpq() == 960
    explicit Score(i64 tpq); // >= 1
    template<std::floating_point U>
    explicit Score(U) = delete;
    Score(i64                   tpq,
          vec<Track<T>>         tracks,
          vec<TimeSignature<T>> time_signatures,
          vec<KeySignature<T>>  key_signatures,
          vec<Tempo<T>>         tempos,
          vec<TextEvent<T>>     markers);

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
    [[nodiscard]] vec<unit>   beats(time_input_t<T> start = 0) const;     // replaces get_beats()
    [[nodiscard]] vec<unit>   downbeats(time_input_t<T> start = 0) const; // replaces get_downbeats()
    template<std::floating_point U> requires std::same_as<T, Tick>
    vec<unit>                 beats(U) const = delete;
    template<std::floating_point U> requires std::same_as<T, Tick>
    vec<unit>                 downbeats(U) const = delete;
    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] std::string summary() const;
    bool                      operator==(const Score& other) const;

    // In-place mutators (void, strong guarantee)
    void sort(SortOrder order = SortOrder::Ascending); // stable, on every event list; keeps track order
    void shift_time(time_input_t<T> offset);
    template<std::floating_point U> requires std::same_as<T, Tick>
    void shift_time(U) = delete;
    void shift_pitch(i64 offset);    // every track, drums included
    void shift_velocity(i64 offset);

    // Returning transforms (leave receiver unchanged)
    [[nodiscard]] Score          clip(time_input_t<T> start, time_input_t<T> end, const ClipOptions& opts = {}) const;
    template<typename U, typename V>
        requires (std::same_as<T, Tick> && (std::floating_point<U> || std::floating_point<V>))
    Score                        clip(U, V, const ClipOptions& = {}) const = delete;
    [[nodiscard]] Score          adjust_time(std::span<const unit> original, std::span<const unit> target) const;
    template<TType To>
    [[nodiscard]] Score<To>      to(time_input_t<To> min_duration = 0) const; // floor in the target unit
    template<TType To, std::floating_point U> requires std::same_as<To, Tick>
    Score<To>                    to(U) const = delete;
    [[nodiscard]] Score<Tick>    resample(i64 tpq, i64 min_duration = 0) const; // one rounding step
    template<typename U, typename V = i64>
        requires (std::floating_point<U> || std::floating_point<V>)
    Score<Tick>                  resample(U, V = 0) const = delete;
    [[nodiscard]] ScorePianoroll pianoroll(const PianorollOptions& opts = {}) const requires std::same_as<T, Tick>;

private:
    i32 tpq_ = 960;
};

} // namespace symusic
```

### Free Operations, `EventColumns`, `Pianoroll` & `Synthesizer` (`ops.h`, `conversion.h`, `soa.h`, `pianoroll.h`, `synth.h`)

Because `ValueStorage` uses plain `std::vector`, track-list sorting, event-span sorting, and point/batch time-map queries are provided as free functions in `namespace symusic`; legacy mutating free functions (`ops::clip`, `ops::trim`, `ops::clip_with_sentinel`, `ops::adjust_time`) and raw file/serialization helpers (`read_file`, `write_file`, `read_score_file`, `read_score`, `serialize`, `deserialize`) are removed from the public surface.

`EventColumns<E>` covers every numeric event (`TextEvent` is excluded, matching `to_numpy()`) and pairs with `events_from_columns<E>(...)`, the single core validation authority backing Python `from_numpy`, JS `fromColumns` and C ABI bulk column imports.

A pianoroll stores one dense `vec<pianoroll_t>` (`u8`) array in row-major order, so `std::move(roll).take()` moves the vector out in O(1) without copying, and volume overflow throws `std::overflow_error`. Pointers, references and spans from `data()`, `span()` and `operator()` stay valid until the roll is destroyed, moved from or taken; `clear()` zeroes the values and keeps them valid. After move or `take()`, every dimension is `0` and `span()` is empty.

`render()`, `AudioBuffer`, `to_wav()` and `write_wav()` follow [rendering](./semantics.md#rendering) and [audio buffers and WAV](./semantics.md#audio-buffers-and-wav); their `ValueError` is `std::invalid_argument`, `OverflowError` is `std::overflow_error`, cancellation throws `CancelledError`, and file errors are `std::filesystem::filesystem_error`.

```cpp
namespace symusic {

// Vector/span operations for ValueStorage callers (default keys from semantics.md#sort-keys)
template<TType T>
void sort_tracks(vec<Track<T>>& tracks, SortOrder order = SortOrder::Ascending);
template<TimeEvent E>
void sort_events(vec<E>& events, SortOrder order = SortOrder::Ascending);
template<TimeEvent E>
[[nodiscard]] bool is_sorted(std::span<const E> events, SortOrder order = SortOrder::Ascending);

// Point and batch time conversion through a score's tempo map without copying the score
template<TType To, TType From>
[[nodiscard]] typename To::unit convert_time(time_input_t<From> time, const Score<From>& context);
template<TType To, TType From, std::floating_point U>
    requires std::same_as<From, Tick>
typename To::unit convert_time(U, const Score<From>&) = delete;
template<TType To, TType From>
[[nodiscard]] vec<typename To::unit> convert_times(std::span<const typename From::unit> times, const Score<From>& context);

// Numeric events only (excludes TextEvent). ColumnValue<KeySignature<T>, 2> is u8 so std::vector<bool> never appears.
template<typename E>
concept ColumnEvent = TimeEvent<E> && !std::same_as<E, TextEvent<typename E::ttype>>;

// Replaces 8 legacy *Arr structs (NoteArr, PedalArr, ...) and backs all bindings' column I/O.
template<ColumnEvent E>
class EventColumns {
public:
    explicit EventColumns(std::span<const E> events);
    [[nodiscard]] size_t size() const noexcept;
    template<size_t I> [[nodiscard]] auto&       column() noexcept;
    template<size_t I> [[nodiscard]] const auto& column() const noexcept;
    void for_each(auto&& visit);             // visit(std::string_view field_name, vec<ColumnValue>& column)
    void for_each(auto&& visit) const;       // visit(std::string_view field_name, const vec<ColumnValue>& column)
    [[nodiscard]] vec<E> to_events() const;  // validates every row before building any event
};

template<ColumnEvent E, typename... Spans>
[[nodiscard]] vec<E> events_from_columns(Spans... columns); // equal-length 1-D spans, validated per semantics.md#numeric-input

class TrackPianoroll { // (mode, pitch, time)
public:
    TrackPianoroll(size_t mode_dim, size_t pitch_dim, size_t time_dim);
    [[nodiscard]] pianoroll_t*                 data() noexcept;
    [[nodiscard]] const pianoroll_t*           data() const noexcept;
    [[nodiscard]] std::span<pianoroll_t>       span() noexcept;
    [[nodiscard]] std::span<const pianoroll_t> span() const noexcept;
    [[nodiscard]] pianoroll_t&                 operator()(size_t mode, size_t pitch, size_t time);
    [[nodiscard]] const pianoroll_t&           operator()(size_t mode, size_t pitch, size_t time) const;
    [[nodiscard]] pianoroll_t                  get(size_t mode, size_t pitch, size_t time) const;
    void                                       set(size_t mode, size_t pitch, size_t start, size_t duration, pianoroll_t value);
    void                                       clear() noexcept;
    [[nodiscard]] std::tuple<size_t, size_t, size_t> dims() const noexcept;
    [[nodiscard]] vec<pianoroll_t>             take() && noexcept; // O(1) move out; replaces release()
};

class ScorePianoroll { // (mode, track, pitch, time)
public:
    ScorePianoroll(size_t mode_dim, size_t track_dim, size_t pitch_dim, size_t time_dim);
    [[nodiscard]] pianoroll_t*                 data() noexcept;
    [[nodiscard]] const pianoroll_t*           data() const noexcept;
    [[nodiscard]] std::span<pianoroll_t>       span() noexcept;
    [[nodiscard]] std::span<const pianoroll_t> span() const noexcept;
    [[nodiscard]] pianoroll_t&                 operator()(size_t mode, size_t track, size_t pitch, size_t time);
    [[nodiscard]] const pianoroll_t&           operator()(size_t mode, size_t track, size_t pitch, size_t time) const;
    [[nodiscard]] pianoroll_t                  get(size_t mode, size_t track, size_t pitch, size_t time) const;
    void                                       set(size_t mode, size_t track, size_t pitch, size_t start, size_t duration, pianoroll_t value);
    void                                       clear() noexcept;
    [[nodiscard]] std::tuple<size_t, size_t, size_t, size_t> dims() const noexcept;
    [[nodiscard]] vec<pianoroll_t>             take() && noexcept;
};

struct AudioBuffer {
    vec<f32> samples;          // interleaved frames (L, R, L, R, ... for stereo)
    size_t   channels    = 0;  // 1 (Mono) or 2 (Stereo)
    int      sample_rate = 0;  // Hz (carried from SynthSettings::sample_rate)

    [[nodiscard]] size_t  sample_frames() const; // samples.size() / channels; 0 when channels == 0
    [[nodiscard]] vec<u8> to_wav(AudioEncoding encoding = AudioEncoding::Pcm16) const; // zero-dep MIT WAV bytes
};

// Zero-dependency MIT WAV encoders (accepting an AudioBuffer or a sliced sample span)
[[nodiscard]] vec<u8> to_wav(
    std::span<const f32> samples,
    int                  sample_rate,
    AudioChannels        channels,
    AudioEncoding        encoding = AudioEncoding::Pcm16
);
void write_wav(
    const std::filesystem::path& path,
    const AudioBuffer&           audio,
    AudioEncoding                encoding = AudioEncoding::Pcm16
);
void write_wav(
    const std::filesystem::path& path,
    std::span<const f32>         samples,
    int                          sample_rate,
    AudioChannels                channels,
    AudioEncoding                encoding = AudioEncoding::Pcm16
);

class Synthesizer {
public:
    // FluidSynth 2.6.2+ loaded at runtime (explicit library_path -> co-located/bundled -> system fallback)
    explicit Synthesizer(const std::filesystem::path& soundfont, const SynthSettings& settings = {});
    explicit Synthesizer(std::span<const u8> soundfont_bytes, const SynthSettings& settings = {}); // copies the bytes

    // Each call starts from fresh synthesizer state; concurrent calls are safe while no thread mutates `score`.
    template<TType T>
    [[nodiscard]] AudioBuffer render(const Score<T>& score, const RenderOptions& options = {}) const;
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
