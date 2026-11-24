#include "symusic/synth.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <limits>
#include <mutex>
#include <tuple>

#include <fmt/format.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif


namespace symusic {
namespace {

std::string path_as_utf8(const std::filesystem::path& path) {
    const auto utf8 = path.u8string();
    return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}

[[noreturn]] void throw_soundfont_load_error(const std::filesystem::path& path) {
    // FluidSynth reports one status for both file access and format errors.
#ifdef _WIN32
    std::FILE* file       = nullptr;
    const int  open_error = _wfopen_s(&file, path.c_str(), L"rb");
#else
    auto*     file       = std::fopen(path.c_str(), "rb");
    const int open_error = file ? 0 : errno;
#endif
    if (!file) {
        throw std::filesystem::filesystem_error(
            "Cannot open SoundFont", path, std::error_code(open_error, std::generic_category())
        );
    }
    errno = 0;
    std::fgetc(file);
    const bool read_failed = std::ferror(file);
    const int  read_error  = errno;
    std::fclose(file);
    if (read_failed) {
        const auto code = read_error ? std::error_code(read_error, std::generic_category()) :
                                       std::make_error_code(std::errc::io_error);
        throw std::filesystem::filesystem_error("Cannot read SoundFont", path, code);
    }
    throw std::runtime_error(fmt::format("Cannot load SoundFont at {}", path_as_utf8(path)));
}

class SharedLibrary {
#ifdef _WIN32
    HMODULE handle;
#else
    void* handle;
#endif
    std::string name;

public:
    explicit SharedLibrary(const std::filesystem::path& path) :
        handle(nullptr), name(path_as_utf8(path)) {
        if (!path.is_absolute()) {
            throw std::invalid_argument("FluidSynth library path must be absolute");
        }
#ifdef _WIN32
        handle = LoadLibraryExW(
            path.c_str(),
            nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS
        );
        if (!handle) {
            const auto error = GetLastError();
            throw std::runtime_error(
                fmt::format("Cannot load FluidSynth at {} (Windows error {})", name, error)
            );
        }
#else
        handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            const char* error = dlerror();
            throw std::runtime_error(
                fmt::format(
                    "Cannot load FluidSynth at {}: {}", name, error ? error : "unknown error"
                )
            );
        }
#endif
    }

    SharedLibrary(const SharedLibrary&)            = delete;
    SharedLibrary& operator=(const SharedLibrary&) = delete;

    ~SharedLibrary() {
#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
    }

    template<typename Signature>
    Signature* symbol(const char* entry) const {
#ifdef _WIN32
        const auto address = GetProcAddress(handle, entry);
#else
        const auto address = dlsym(handle, entry);
#endif
        if (!address) {
            throw std::runtime_error(fmt::format("FluidSynth at {} is missing {}", name, entry));
        }
        // POSIX dlsym (void*) and Win32 GetProcAddress (FARPROC) require reinterpret_cast to
        // recover the typed C ABI function pointer.
        return reinterpret_cast<Signature*>(address);
    }
};

// Opaque types and signatures from the FluidSynth 2.x C ABI.
struct FluidSettings;
struct FluidSynth;

constexpr int kMaxChannels     = 256;
constexpr int kMidiKeys        = 128;
constexpr int kDrumBank        = 128;
constexpr int kPitchBendCenter = 8192;
constexpr int kAllNotesOff     = 123;
constexpr int kOmniOff         = 124;

enum class EventKind { Control, PitchBend, NoteOff, NoteOn };

struct Event {
    f64       time;
    size_t    frame;
    int       channel;
    EventKind kind;
    int       value;
    int       parameter = 0;
    size_t    note_id   = 0;
};

struct Channel {
    int                           program;
    bool                          drum;
    int                           bank_msb      = 0;
    int                           bank_lsb      = 0;
    bool                          has_msb       = false;
    bool                          has_lsb       = false;
    int                           selected_bank = -1;
    std::array<size_t, kMidiKeys> sounding{};

    [[nodiscard]] int bank() const {
        if (has_lsb) { return bank_msb * 128 + bank_lsb; }
        return has_msb ? bank_msb : (drum ? kDrumBank : 0);
    }
};

size_t frame_count(f64 frames) {
    // Stereo size must fit both the vector and pointer arithmetic.
    if (!(frames >= 0)) {
        throw std::invalid_argument(
            fmt::format("Audio render times must be finite and nonnegative, got {}", frames)
        );
    }
    if (frames >= static_cast<f64>(vec<f32>().max_size() / 2)) {
        throw std::overflow_error("Audio render length exceeds the sample buffer limit");
    }
    return static_cast<size_t>(frames);
}

void validate_event_time(f64 time) {
    if (!std::isfinite(time) || time < 0) {
        throw std::invalid_argument(
            fmt::format("Audio render times must be finite and nonnegative, got {}", time)
        );
    }
}

size_t event_frame(f64 time, int sample_rate, int block_size) {
    validate_event_time(time);
    return frame_count(std::ceil(time * sample_rate / block_size) * block_size);
}

void validate_midi_data(int value) {
    if (value < 0 || value >= kMidiKeys) {
        throw std::invalid_argument(
            fmt::format("MIDI data must be in the range 0 to 127, got {}", value)
        );
    }
}

void schedule_track(
    const Track<Second>& track, int channel, int sample_rate, int block_size, vec<Event>& events
) {
    const auto add = [&](f64 time, EventKind kind, int value, int parameter, size_t note_id = 0) {
        events.push_back(
            {time,
             event_frame(time, sample_rate, block_size),
             channel,
             kind,
             value,
             parameter,
             note_id}
        );
    };
    for (const auto& control : track.controls) {
        validate_midi_data(control.number);
        validate_midi_data(control.value);
        add(control.time, EventKind::Control, control.number, control.value);
    }
    for (const auto& bend : track.pitch_bends) {
        if (bend.value < -kPitchBendCenter || bend.value >= kPitchBendCenter) {
            throw std::invalid_argument(
                fmt::format("Pitch bend must be in the range -8192 to 8191, got {}", bend.value)
            );
        }
        add(bend.time, EventKind::PitchBend, bend.value + kPitchBendCenter, 0);
    }
    for (const auto& note : track.notes) {
        validate_midi_data(note.pitch);
        validate_midi_data(note.velocity);
        if (!std::isfinite(note.duration) || note.duration < 0) {
            throw std::invalid_argument(
                fmt::format("Note duration must be finite and nonnegative, got {}", note.duration)
            );
        }
        if (note.duration == 0 || note.velocity == 0) {
            validate_event_time(note.time);
            continue;
        }
        const size_t note_id = events.size() + 1;
        add(note.time, EventKind::NoteOn, note.pitch, note.velocity, note_id);
        const size_t first_off_frame = events.back().frame + static_cast<size_t>(block_size);
        add(
            static_cast<f64>(note.time) + note.duration, EventKind::NoteOff, note.pitch, 0, note_id
        );
        events.back().frame = std::max(events.back().frame, first_off_frame);
    }
}

void check(int status, const char* operation) {
    if (status != 0) {
        throw std::runtime_error(fmt::format("FluidSynth failed to {}", operation));
    }
}

}   // namespace

struct Synthesizer::Backend {
    explicit Backend(const std::filesystem::path& path) : library(path) {
        load(new_fluid_settings, "new_fluid_settings");
        load(delete_fluid_settings, "delete_fluid_settings");
        load(fluid_settings_setnum, "fluid_settings_setnum");
        load(fluid_settings_setint, "fluid_settings_setint");
        load(new_fluid_synth, "new_fluid_synth");
        load(delete_fluid_synth, "delete_fluid_synth");
        load(fluid_synth_sfload, "fluid_synth_sfload");
        load(fluid_synth_set_channel_type, "fluid_synth_set_channel_type");
        load(fluid_synth_program_select, "fluid_synth_program_select");
        load(fluid_synth_noteon, "fluid_synth_noteon");
        load(fluid_synth_noteoff, "fluid_synth_noteoff");
        load(fluid_synth_cc, "fluid_synth_cc");
        load(fluid_synth_pitch_bend, "fluid_synth_pitch_bend");
        load(fluid_synth_all_notes_off, "fluid_synth_all_notes_off");
        load(fluid_synth_get_internal_bufsize, "fluid_synth_get_internal_bufsize");
        load(fluid_synth_set_interp_method, "fluid_synth_set_interp_method");
        load(fluid_synth_write_float, "fluid_synth_write_float");
    }

    template<typename F>
    void load(F*& function, const char* name) const {
        function = library.symbol<F>(name);
    }

    struct FluidLibrary : SharedLibrary {
        explicit FluidLibrary(const std::filesystem::path& path) : SharedLibrary(path) {
            int major = 0, minor = 0, patch = 0;
            symbol<void(int*, int*, int*)>("fluid_version")(&major, &minor, &patch);
            if (major != 2 || std::pair{minor, patch} < std::pair{6, 2}) {
                throw std::runtime_error(
                    fmt::format(
                        "FluidSynth at {} reports {}.{}.{}. Expected 2.6.2 or a later 2.x release.",
                        path_as_utf8(path),
                        major,
                        minor,
                        patch
                    )
                );
            }
        }
    };

    FluidLibrary library;

    FluidSettings* (*new_fluid_settings)();
    void (*delete_fluid_settings)(FluidSettings*);
    int (*fluid_settings_setnum)(FluidSettings*, const char*, double);
    int (*fluid_settings_setint)(FluidSettings*, const char*, int);
    FluidSynth* (*new_fluid_synth)(FluidSettings*);
    void (*delete_fluid_synth)(FluidSynth*);
    int (*fluid_synth_sfload)(FluidSynth*, const char*, int);
    int (*fluid_synth_set_channel_type)(FluidSynth*, int, int);
    int (*fluid_synth_program_select)(FluidSynth*, int, int, int, int);
    int (*fluid_synth_noteon)(FluidSynth*, int, int, int);
    int (*fluid_synth_noteoff)(FluidSynth*, int, int);
    int (*fluid_synth_cc)(FluidSynth*, int, int, int);
    int (*fluid_synth_pitch_bend)(FluidSynth*, int, int);
    int (*fluid_synth_all_notes_off)(FluidSynth*, int);
    int (*fluid_synth_get_internal_bufsize)(FluidSynth*);
    int (*fluid_synth_set_interp_method)(FluidSynth*, int, int);
    int (*fluid_synth_write_float)(FluidSynth*, int, void*, int, int, void*, int, int);

    void select_program(FluidSynth* synth, int index, int font_id, Channel& channel) const {
        const int bank = channel.bank();
        if (channel.selected_bank == bank) { return; }
        if (fluid_synth_program_select(synth, index, font_id, bank, channel.program) != 0) {
            throw std::runtime_error(
                fmt::format(
                    "SoundFont has no preset for bank {}, program {} on channel {}",
                    bank,
                    channel.program,
                    index
                )
            );
        }
        channel.selected_bank = bank;
    }

    void dispatch(FluidSynth* synth, int font_id, Channel& channel, const Event& event) const {
        const auto [time, frame, index, kind, value, parameter, note_id] = event;
        switch (kind) {
        case EventKind::Control:
            if (value == 0) {
                channel.bank_msb = parameter;
                channel.has_msb  = true;
            } else if (value == 32) {
                channel.bank_lsb = parameter;
                channel.has_lsb  = true;
            }
            // Omni and Mono/Poly mode messages also release notes. Keep that
            // effect local and leave FluidSynth's channel groups untouched.
            if (value >= kOmniOff) {
                check(fluid_synth_cc(synth, index, kAllNotesOff, 0), "release notes");
            } else {
                check(fluid_synth_cc(synth, index, value, parameter), "send a control change");
            }
            break;
        case EventKind::PitchBend:
            check(fluid_synth_pitch_bend(synth, index, value), "send a pitch bend");
            break;
        case EventKind::NoteOff:
            if (channel.sounding[value] == note_id) {
                // FluidSynth can also end a voice before its scheduled note-off.
                fluid_synth_noteoff(synth, index, value);
                channel.sounding[value] = 0;
            }
            break;
        case EventKind::NoteOn:
            select_program(synth, index, font_id, channel);
            check(fluid_synth_noteon(synth, index, value, parameter), "start a note");
            // A retrigger ends the previous voice for this key.
            channel.sounding[value] = note_id;
            break;
        }
    }

    AudioBuffer render(
        const Score<Second>&         score,
        const std::filesystem::path& soundfont,
        const SynthSettings&         config,
        bool                         stereo
    ) const {
        vec<Channel> channels;
        for (const auto& track : score.tracks) {
            if (!track.notes.empty()) {
                validate_midi_data(track.program);
                channels.push_back({track.program, track.is_drum});
            }
        }
        if (channels.size() > kMaxChannels) {
            throw std::invalid_argument("FluidSynth supports up to 256 note-bearing tracks");
        }
        const int channel_count = static_cast<int>(channels.size());
        const std::unique_ptr<FluidSettings, decltype(delete_fluid_settings)> settings(
            new_fluid_settings(), delete_fluid_settings
        );
        if (!settings) { throw std::runtime_error("Cannot allocate FluidSynth settings"); }
        const auto set = [&](const char* setting, int value) {
            check(fluid_settings_setint(settings.get(), setting, value), setting);
        };
        check(
            fluid_settings_setnum(settings.get(), "synth.sample-rate", config.sample_rate),
            "set sample rate"
        );
        set("synth.polyphony", config.polyphony);
        set("synth.midi-channels", std::max(16, (channel_count + 15) / 16 * 16));
        set("synth.cpu-cores", 1);
        set("synth.min-note-length", 0);
        set("synth.reverb.active", config.reverb);
        set("synth.chorus.active", config.chorus);

        // Initialization uses shared tables. SoundFont teardown can free a cached
        // sample before a concurrent load finishes acquiring its reference.
        static std::mutex lifecycle_mutex;
        const auto        destroy_synth = [this](FluidSynth* state) {
            const std::lock_guard lock(lifecycle_mutex);
            delete_fluid_synth(state);
        };
        std::unique_ptr<FluidSynth, decltype(destroy_synth)> synth(nullptr, destroy_synth);
        int                                                  font_id;
        {
            const std::lock_guard lock(lifecycle_mutex);
            synth.reset(new_fluid_synth(settings.get()));
            if (!synth) { throw std::runtime_error("Cannot initialize FluidSynth"); }
            font_id = fluid_synth_sfload(synth.get(), path_as_utf8(soundfont).c_str(), 0);
        }
        if (font_id < 0) { throw_soundfont_load_error(soundfont); }
        auto* state = synth.get();
        check(fluid_synth_set_interp_method(state, -1, 4), "set fourth-order interpolation");
        const int block_size = fluid_synth_get_internal_bufsize(state);
        if (block_size <= 0) {
            throw std::runtime_error("FluidSynth reported an invalid rendering block size");
        }

        vec<Event> events;
        int        index = 0;
        for (const auto& track : score.tracks) {
            if (!track.notes.empty()) {
                check(
                    fluid_synth_set_channel_type(state, index, channels[index].drum ? 1 : 0),
                    "set channel type"
                );
                schedule_track(track, index, config.sample_rate, block_size, events);
                ++index;
            }
        }
        std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
            return std::tie(a.frame, a.time, a.kind) < std::tie(b.frame, b.time, b.kind);
        });

        if (events.empty()) return AudioBuffer{{}, stereo ? 2U : 1U};
        const size_t last_frame = events.back().frame;
        const size_t tail       = frame_count(std::round(config.tail_seconds * config.sample_rate));
        const size_t frames     = last_frame + tail;
        if (frames > vec<f32>().max_size() / 2) {
            throw std::overflow_error("Audio render length exceeds the sample buffer limit");
        }
        AudioBuffer audio{vec<f32>(frames * 2), 2};
        size_t      written = 0;
        // Keep writes block-aligned until all events have been dispatched.
        const int  max_write   = std::numeric_limits<int>::max() / block_size * block_size;
        const auto write_until = [&](size_t frame) {
            while (written < frame) {
                const int count
                    = static_cast<int>(std::min(frame - written, static_cast<size_t>(max_write)));
                f32* out = audio.samples.data() + written * 2;
                check(fluid_synth_write_float(state, count, out, 0, 2, out, 1, 2), "render audio");
                written += static_cast<size_t>(count);
            }
        };
        for (const auto& event : events) {
            write_until(event.frame);
            dispatch(state, font_id, channels[event.channel], event);
        }
        for (index = 0; index < channel_count; ++index) {
            check(fluid_synth_cc(state, index, 64, 0), "release sustain");
            check(fluid_synth_cc(state, index, 66, 0), "release sostenuto");
            check(fluid_synth_all_notes_off(state, index), "release notes");
        }
        write_until(frames);

        if (!stereo) {
            for (size_t frame = 0; frame < frames; ++frame) {
                audio.samples[frame]
                    = (audio.samples[frame * 2] + audio.samples[frame * 2 + 1]) * 0.5f;
            }
            audio.samples.resize(frames);
            audio.channels = 1;
        }
        return audio;
    }
};

Synthesizer::Synthesizer(
    const std::filesystem::path& library_path,
    const std::filesystem::path& soundfont,
    SynthSettings                settings
) : settings(settings) {
    if (settings.sample_rate < 8000 || settings.sample_rate > 96000) {
        throw std::invalid_argument(
            fmt::format(
                "sample_rate must be in the range 8000 to 96000 Hz, got {}", settings.sample_rate
            )
        );
    }
    if (settings.polyphony < 1 || settings.polyphony > 65535) {
        throw std::invalid_argument(
            fmt::format("polyphony must be in the range 1 to 65535, got {}", settings.polyphony)
        );
    }
    if (!std::isfinite(settings.tail_seconds) || settings.tail_seconds < 0) {
        throw std::invalid_argument(
            fmt::format(
                "tail_seconds must be finite and nonnegative, got {}", settings.tail_seconds
            )
        );
    }
    this->soundfont = std::filesystem::absolute(soundfont);
    if (!std::filesystem::exists(this->soundfont)) {
        throw std::filesystem::filesystem_error(
            "SoundFont file does not exist",
            this->soundfont,
            std::make_error_code(std::errc::no_such_file_or_directory)
        );
    }
    backend = std::make_shared<Backend>(library_path);
}

AudioBuffer Synthesizer::render_seconds(const Score<Second>& score, bool stereo) const {
    return backend->render(score, soundfont, settings, stereo);
}

}   // namespace symusic
