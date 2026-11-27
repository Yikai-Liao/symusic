# Native C++ API & Language Standard

symusic 0.7 establishes a C++-first **C++20** core (`symusic::symusic`, default `ValueStorage`) shared across Python, C99, JS/WASM, and Go bindings—with zero `anchorvec`, zero `zpp_bits`, zero `libcurl`/TLS, and zero GPL linkage.

## Layered Architecture

```text
bindings/python (nanobind + anchorvec SharedStorage + private C++20 SYMZ v3 pickle)
bindings/c (C99 ABI)   bindings/js (WASM ValueStorage / N-API)   bindings/go (cgo)   C++ (find_package)
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
| Defaults & naming | `Score<T, S = ValueStorage>`, `Score::ticks_per_quarter`, `Tempo<T>` (`microseconds_per_quarter`), `TextMeta<T>`, `get_beats()` / `get_downbeats()` | `Score<T = Tick, S = ValueStorage>`, `Score::tpq`, `Tempo<T = Tick>` (`mspq`), `TextEvent<T>`, `beats(start=0)` / `downbeats(start=0)` |
| `Note` domains & defaults | `i8` pitch/velocity; the constructor with arguments requires `velocity`, and default construction sets it to `0`; redundant `start()` | `i8` storage, validated musical domain (`0..127`), default `velocity = 64` (`0x40`), drop `start()` (keep `.time`, `.end()`); MIDI wire export additionally encodes `velocity=0` as NoteOff and caps `tpq` to `1..32767` |
| `Tempo` construction | `Tempo::from_qpm(t, 120.0)` exists, but brace initialization `Tempo<Tick>{0, 120}` silently means `120 µs/quarter` (500,000 QPM) | Keep `Tempo::from_qpm(t, 120.0)` and add `Tempo::from_mspq(t, 500000)` (`mspq` = microseconds per quarter), so every construction names its unit; static converters `qpm_to_mspq` / `mspq_to_qpm` |
| `clip`/`trim` & `adjust_time` | `clip(start, end, bool clip_end)`; `trim(start, end, min_overlap, start_mode, end_mode)` with string modes; mutating free `void ops::adjust_time` | `clip(start, end, const ClipOptions& = {})`; `ClipOptions` holds `head` (default `ClipMode::Drop`) and `tail` (default `ClipMode::Keep`) of `enum class ClipMode { Keep, Drop, Cut }`; `trim` and `min_overlap` go; returning `score.adjust_time(orig, target)` taking `span` |
| File I/O & members | Free `read_score(path, format, decode_abc)` and `read_score_file(path)` returning bytes; free `convert` / `resample` / `pianoroll` | `Score::from_file(path, opts)` / `to_file(path)`, `to<To>()`, single-step `resample()`, `Tick`-only `pianoroll()` members |
| `Pianoroll` & `Synthesizer` | `operator()` returns `T*`, non-`const get()`; `Synthesizer` requires absolute `library_path`; `render(score, bool stereo = true)` | `operator()` returns `T&` (+ `const`), `const get()`, `span` view; `render(score, AudioChannels)` (`Mono`, `Stereo`); `Synthesizer(soundfont, const SynthSettings& = {})` (+ `span<const u8>` via `fluid_sfloader_t`) moves optional `library_path` into `SynthSettings` (explicit absolute path → co-located/bundled FluidSynth → system fallback) and adds `gain = 0.2`, `interpolation` aligned to FluidSynth 2.6 (`SynthInterpolation::{None, Linear, FourthOrder, Mid, High, Highest}`, default `FourthOrder`), and `cpu_cores = 1` |
| Emptiness idiom | `Score::empty()`, `Track::empty()`, `Note::empty()` | Uniform `is_*` predicates across C++ and all bindings: `Score::is_empty()`, `Track::is_empty()`, `Note::is_silent()` (only STL sequence containers like `std::vector` / `anchorvec` keep `.empty()`) |

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

`zpp_bits` (6,058-line header) is still removed from the core, for a different reason: it serves only the Python pickle. C++ interchange uses MIDI, and `bindings/python/` owns a private C++20 little-endian **`SYMZ v3`** codec compiled into `symusic._core` (fast-pathing `anchorvec.is_flat()` bulk `memcpy` with zero runtime regression).

## Finalized C++ 0.7 API Synopsis

Complete public C++20 declarations for every finalized and changed type in `namespace symusic` (pure `ValueStorage` default; `SharedStorage` lives in `bindings/python/`).

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

enum class ClipMode : u8 { Keep, Drop, Cut };
struct ClipOptions {
    ClipMode head = ClipMode::Drop; // note extending before start
    ClipMode tail = ClipMode::Keep; // note extending past end
};

enum class PianorollMode : u8 { Onset, Frame, Offset };
struct PianorollOptions {
    vec<PianorollMode> modes           = {PianorollMode::Onset, PianorollMode::Frame};
    std::pair<u8, u8>  pitch_range     = {0, 128};
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

struct SynthSettings {
    int                   sample_rate   = 44100;                           // 8000..96000 Hz
    int                   polyphony     = 256;                             // 1..65535
    f64                   gain          = 0.2;                             // 0.0..10.0 (synth.gain)
    SynthInterpolation    interpolation = SynthInterpolation::FourthOrder; // FluidSynth 2.6+
    bool                  reverb        = false;                           // synth.reverb.active
    bool                  chorus        = false;                           // synth.chorus.active
    f64                   tail_seconds  = 1.0;                             // >= 0, finite
    int                   cpu_cores     = 1;                               // 1..256 (synth.cpu-cores)
    std::filesystem::path library_path  = {}; // empty => co-located/bundled -> system fallback; non-empty => must be absolute
};

} // namespace symusic
```

### Events (`event.h`)

All events inherit `TimeStamp<T>` (`using ttype = T; using unit = typename T::unit; unit time = 0;`), default `T = Tick`, validate musical ranges in constructors (`std::invalid_argument`), compare by value (`operator==`), and drop `serialize()`/`deserialize()`, `operator->`/`operator*`, and redundant `start()`.

```cpp
namespace symusic {

template<TType T = Tick>
struct Note : TimeStamp<T> {
    typename T::unit duration{0};
    i8               pitch{0};
    i8               velocity{64}; // default 64 (MIDI 1.0 0x40); validated 0..127

    Note() = default;
    Note(typename T::unit time, typename T::unit duration, i8 pitch, i8 velocity = 64);

    [[nodiscard]] Note             copy() const;
    [[nodiscard]] typename T::unit end() const;        // checked add_time(time, duration)
    [[nodiscard]] bool             is_silent() const;  // duration <= 0 || velocity <= 0 (replaces empty())
    void                           shift_pitch(i32 offset);
    void                           shift_velocity(i32 offset);
    [[nodiscard]] std::string      to_string() const;
    bool                           operator==(const Note&) const = default;
};

template<TType T = Tick>
struct Pedal : TimeStamp<T> {
    typename T::unit duration{0};

    Pedal() = default;
    Pedal(typename T::unit time, typename T::unit duration);

    [[nodiscard]] Pedal            copy() const;
    [[nodiscard]] typename T::unit end() const;
    [[nodiscard]] std::string      to_string() const;
    bool                           operator==(const Pedal&) const = default;
};

template<TType T = Tick>
struct ControlChange : TimeStamp<T> {
    u8 number{0}, value{0}; // validated 0..127

    ControlChange() = default;
    ControlChange(typename T::unit time, u8 number, u8 value);

    [[nodiscard]] ControlChange copy() const;
    [[nodiscard]] std::string   to_string() const;
    bool                        operator==(const ControlChange&) const = default;
};

template<TType T = Tick>
struct TimeSignature : TimeStamp<T> {
    u8 numerator{4}, denominator{4}; // numerator >= 1; denominator power-of-2 in 1..128

    TimeSignature() = default;
    TimeSignature(typename T::unit time, u8 numerator, u8 denominator);

    [[nodiscard]] TimeSignature copy() const;
    [[nodiscard]] std::string   to_string() const;
    bool                        operator==(const TimeSignature&) const = default;
};

template<TType T = Tick>
struct KeySignature : TimeStamp<T> {
    i8   fifths{0};    // -7..7 sharps/flats (replaces key)
    bool minor{false}; // replaces i8 tonality

    KeySignature() = default;
    KeySignature(typename T::unit time, i8 fifths, bool minor = false);

    [[nodiscard]] u8           tonic() const; // (7 * fifths + 9 * minor) mod 12 (floored, 0..11; replaces degree())
    [[nodiscard]] KeySignature copy() const;
    [[nodiscard]] std::string  to_string() const;
    bool                       operator==(const KeySignature&) const = default;
};

template<TType T = Tick>
struct Tempo : TimeStamp<T> {
    i32 mspq{500000}; // microseconds per quarter (> 0; reverts #134's microseconds_per_quarter)

    Tempo() = default;
    [[nodiscard]] static Tempo         from_qpm(typename T::unit time, f64 qpm);
    [[nodiscard]] static Tempo         from_mspq(typename T::unit time, i32 mspq);
    [[nodiscard]] static i32           qpm_to_mspq(f64 qpm);
    [[nodiscard]] static constexpr f64 mspq_to_qpm(i32 mspq);

    [[nodiscard]] f64         qpm() const;
    void                      set_qpm(f64 qpm); // void (no longer returns Tempo&)
    [[nodiscard]] Tempo       copy() const;
    [[nodiscard]] std::string to_string() const;
    bool                      operator==(const Tempo&) const = default;
};

template<TType T = Tick>
struct PitchBend : TimeStamp<T> {
    i32 value{0}; // -8192..8191

    PitchBend() = default;
    PitchBend(typename T::unit time, i32 value);

    [[nodiscard]] PitchBend   copy() const;
    [[nodiscard]] std::string to_string() const;
    bool                      operator==(const PitchBend&) const = default;
};

template<TType T = Tick>
struct TextEvent : TimeStamp<T> { // renamed from TextMeta
    std::string text{};

    TextEvent() = default;
    TextEvent(typename T::unit time, std::string text);

    [[nodiscard]] TextEvent   copy() const;
    [[nodiscard]] std::string to_string() const;
    bool                      operator==(const TextEvent&) const = default;
};

template<TimeEvent E>
void shift_time(E& event, typename E::unit offset);

} // namespace symusic
```

### `Track` & `Score` (`track.h`, `score.h`)

No `*_inplace` methods, no `trim()`, no `ScoreNative`/`TrackNative`, no `serialize()`/`deserialize()`. Mutators return `void` with strong exception guarantee; transforms are `[[nodiscard]] const`.

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
    [[nodiscard]] unit        start() const;
    [[nodiscard]] unit        end() const;
    [[nodiscard]] bool        is_empty() const;  // replaces empty()
    [[nodiscard]] vec<Pedal<T>> pedals() const;  // derived on demand from CC64 in controls
    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] std::string summary() const;
    bool                      operator==(const Track& other) const;

    // In-place mutators (void, strong guarantee)
    void sort(bool reverse = false);             // stable sort on all event lists
    void shift_time(unit offset);
    void shift_pitch(i32 offset);
    void shift_velocity(i32 offset);

    // Returning transforms (leave receiver unchanged)
    [[nodiscard]] Track          copy() const;
    [[nodiscard]] Track          clip(unit start, unit end, const ClipOptions& opts = {}) const;
    [[nodiscard]] Track          adjust_time(std::span<const unit> original, std::span<const unit> target) const;
    [[nodiscard]] TrackPianoroll pianoroll(const PianorollOptions& opts = {}) const requires std::same_as<T, Tick>;
};

template<TType T = Tick, typename S = ValueStorage>
struct Score {
    using ttype        = T;
    using unit         = typename T::unit;
    using storage_type = S;
    using track_type   = Track<T, S>;

    i32                                         tpq = 960; // >= 1 (replaces ticks_per_quarter; MIDI export 1..32767)
    typename S::template Tracks<track_type>     tracks;
    typename S::template List<TimeSignature<T>> time_signatures;
    typename S::template List<KeySignature<T>>  key_signatures;
    typename S::template List<Tempo<T>>         tempos;
    typename S::template List<TextEvent<T>>     markers;

    Score() = default;
    explicit Score(i64 tpq);
    Score(i64 tpq,
          typename S::template Tracks<track_type>     tracks,
          typename S::template List<TimeSignature<T>> time_signatures,
          typename S::template List<KeySignature<T>>  key_signatures,
          typename S::template List<Tempo<T>>         tempos,
          typename S::template List<TextEvent<T>>     markers);

    // File & wire I/O (optional ABC via symusic::abc)
    [[nodiscard]] static Score from_midi(std::span<const u8> bytes, const MidiReadOptions& opts = {});
    [[nodiscard]] static Score from_file(const std::filesystem::path& path, const MidiReadOptions& opts = {});
    [[nodiscard]] vec<u8>      to_midi() const;
    void                       to_file(const std::filesystem::path& path) const;

    // Queries
    [[nodiscard]] unit        start() const;
    [[nodiscard]] unit        end() const;
    [[nodiscard]] size_t      note_count() const;   // replaces note_num()
    [[nodiscard]] size_t      track_count() const;  // replaces track_num()
    [[nodiscard]] bool        is_empty() const;     // replaces empty()
    [[nodiscard]] vec<unit>   beats(unit start = 0) const;     // replaces get_beats()
    [[nodiscard]] vec<unit>   downbeats(unit start = 0) const; // replaces get_downbeats()
    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] std::string summary() const;
    bool                      operator==(const Score& other) const;

    // In-place mutators (void, strong guarantee)
    void sort(bool reverse = false);                // stable sort on all event lists; keeps track order
    void shift_time(unit offset);
    void shift_pitch(i32 offset);
    void shift_velocity(i32 offset);

    // Returning transforms (leave receiver unchanged)
    [[nodiscard]] Score          copy() const;
    [[nodiscard]] Score          clip(unit start, unit end, const ClipOptions& opts = {}) const;
    [[nodiscard]] Score          adjust_time(std::span<const unit> original, std::span<const unit> target) const;
    template<TType To>
    [[nodiscard]] Score<To, S>   to(typename To::unit min_duration = 0) const;
    [[nodiscard]] Score<Tick, S> resample(i32 tpq, Tick::unit min_duration = 0) const; // single-step quantization
    [[nodiscard]] ScorePianoroll pianoroll(const PianorollOptions& opts = {}) const requires std::same_as<T, Tick>;
};

} // namespace symusic
```

### `EventColumns`, `Pianoroll` & `Synthesizer` (`soa.h`, `pianoroll.h`, `synth.h`)

```cpp
namespace symusic {

// Replaces 8 legacy *Arr structs (NoteArr, PedalArr, ...)
template<TimeEvent E>
class EventColumns {
public:
    explicit EventColumns(const auto& events);
    void for_each(auto&& visit); // visit(const char* field_name, vec<ColumnValue>& column)
};

class TrackPianoroll { // ScorePianoroll adds a track dimension
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
    [[nodiscard]] pianoroll_t*                 release();
};

struct AudioBuffer {
    vec<f32> samples;          // interleaved float32 frames
    size_t   channels    = 0;  // 1 (Mono) or 2 (Stereo)
    int      sample_rate = 0;  // Hz (carried from SynthSettings::sample_rate)

    [[nodiscard]] size_t  frames() const;
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
    explicit Synthesizer(std::span<const u8> soundfont_bytes, const SynthSettings& settings = {});

    template<TType T, typename S>
    [[nodiscard]] AudioBuffer render(
        const Score<T, S>& score,
        AudioChannels      channels = AudioChannels::Stereo
    ) const;
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
    AudioBuffer audio = synth.render(resampled, AudioChannels::Stereo);
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
ScoreNative<Tick> score = to_native(shared);  // free function in 0.6

shared.clip_inplace(0, 1920);
NoteArr<Tick> cols(score.tracks[0].notes);
auto midi_out = score.dumps<DataFormat::MIDI>();

Synthesizer synth("gm.sf3", 44100, 4);
psynth::AudioData audio = synth.render(shared, true);
```
