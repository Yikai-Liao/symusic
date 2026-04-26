#include "symusic/synth.h"

#include "symusic/conversion.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <fluidlite.h>

namespace symusic {

namespace {

constexpr int kMaxMidiChannels = 256;
constexpr size_t kRenderChunkFrames = 1024;
constexpr double kSilenceThreshold = 1e-5;
constexpr double kMaxTailSeconds = 5.0;
constexpr int kSustainPedalControl = 64;
constexpr int kCenterPitchBend = 8192;

template<typename T>
T clamp_value(const T value, const T lower, const T upper) {
    return std::min(std::max(value, lower), upper);
}

[[nodiscard]] u32 seconds_to_frames(const Second::unit time, const u32 sampleRate) {
    if (time <= 0.0) {
        return 0;
    }
    return static_cast<u32>(std::llround(time * static_cast<double>(sampleRate)));
}

void write_u16(std::ostream& stream, const std::uint16_t value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void write_u32(std::ostream& stream, const std::uint32_t value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void write_i16(std::ostream& stream, const std::int16_t value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

class FluidSettingsPtr {
private:
    fluid_settings_t* ptr;

public:
    explicit FluidSettingsPtr(fluid_settings_t* value = nullptr) : ptr(value) {}
    FluidSettingsPtr(const FluidSettingsPtr&) = delete;
    FluidSettingsPtr& operator=(const FluidSettingsPtr&) = delete;
    FluidSettingsPtr(FluidSettingsPtr&& other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }
    FluidSettingsPtr& operator=(FluidSettingsPtr&& other) noexcept {
        if (this != &other) {
            if (ptr != nullptr) {
                delete_fluid_settings(ptr);
            }
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }
    ~FluidSettingsPtr() {
        if (ptr != nullptr) {
            delete_fluid_settings(ptr);
        }
    }

    [[nodiscard]] fluid_settings_t* get() const { return ptr; }
};

class FluidSynthPtr {
private:
    fluid_synth_t* ptr;

public:
    explicit FluidSynthPtr(fluid_synth_t* value = nullptr) : ptr(value) {}
    FluidSynthPtr(const FluidSynthPtr&) = delete;
    FluidSynthPtr& operator=(const FluidSynthPtr&) = delete;
    FluidSynthPtr(FluidSynthPtr&& other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }
    FluidSynthPtr& operator=(FluidSynthPtr&& other) noexcept {
        if (this != &other) {
            if (ptr != nullptr) {
                delete_fluid_synth(ptr);
            }
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }
    ~FluidSynthPtr() {
        if (ptr != nullptr) {
            delete_fluid_synth(ptr);
        }
    }

    [[nodiscard]] fluid_synth_t* get() const { return ptr; }
};

class FluidSfLoaderPtr {
private:
    fluid_sfloader_t* ptr;

public:
    explicit FluidSfLoaderPtr(fluid_sfloader_t* value = nullptr) : ptr(value) {}
    FluidSfLoaderPtr(const FluidSfLoaderPtr&) = delete;
    FluidSfLoaderPtr& operator=(const FluidSfLoaderPtr&) = delete;
    FluidSfLoaderPtr(FluidSfLoaderPtr&& other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }
    FluidSfLoaderPtr& operator=(FluidSfLoaderPtr&& other) noexcept {
        if (this != &other) {
            if (ptr != nullptr) {
                delete_fluid_defsfloader(ptr);
            }
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }
    ~FluidSfLoaderPtr() {
        if (ptr != nullptr) {
            delete_fluid_defsfloader(ptr);
        }
    }

    [[nodiscard]] fluid_sfloader_t* get() const { return ptr; }
};

enum class EventType : u8 {
    bank_select = 0,
    control_change = 1,
    pitch_bend = 2,
    note_off = 3,
    note_on = 4,
};

struct RenderEvent {
    Second::unit time;
    EventType    type;
    u16          channel;
    i32          a;
    i32          b;
};

struct ChannelState {
    u8   program;
    bool isDrum;
};

bool render_event_less(const RenderEvent& lhs, const RenderEvent& rhs) {
    if (lhs.time != rhs.time) {
        return lhs.time < rhs.time;
    }
    if (lhs.type != rhs.type) {
        return lhs.type < rhs.type;
    }
    if (lhs.channel != rhs.channel) {
        return lhs.channel < rhs.channel;
    }
    if (lhs.a != rhs.a) {
        return lhs.a < rhs.a;
    }
    return lhs.b < rhs.b;
}

void check_fluid_call(const int result, const std::string& message) {
    if (result != 0) {
        throw std::runtime_error(message);
    }
}

void check_fluid_setting(const int result, const std::string& message) {
    if (result != 1) {
        throw std::runtime_error(message);
    }
}

void validate_ctor_args(
    const std::string& sfPath,
    const u32          sampleRate,
    const f64          gain,
    const u32          polyphony,
    const u32          chorusNr,
    const f64          chorusLevel,
    const f64          chorusSpeed,
    const f64          chorusDepth,
    const f64          reverbLevel
) {
    if (sfPath.empty()) {
        throw std::invalid_argument("sf_path must not be empty");
    }
    if (sampleRate < 22050 || sampleRate > 96000) {
        throw std::invalid_argument("sample_rate must be in [22050, 96000]");
    }
    if (gain < 0.0 || gain > 10.0) {
        throw std::invalid_argument("gain must be in [0.0, 10.0]");
    }
    if (polyphony < 16 || polyphony > 4096) {
        throw std::invalid_argument("polyphony must be in [16, 4096]");
    }
    if (chorusNr > 99) {
        throw std::invalid_argument("chorus_nr must be in [0, 99]");
    }
    if (chorusLevel < 0.0 || chorusLevel > 10.0) {
        throw std::invalid_argument("chorus_level must be in [0.0, 10.0]");
    }
    if (chorusSpeed < 0.1 || chorusSpeed > 5.0) {
        throw std::invalid_argument("chorus_speed must be in [0.1, 5.0]");
    }
    if (chorusDepth < 0.0 || chorusDepth > 21.0) {
        throw std::invalid_argument("chorus_depth must be in [0.0, 21.0]");
    }
    if (reverbLevel < 0.0 || reverbLevel > 1.0) {
        throw std::invalid_argument("reverb_level must be in [0.0, 1.0]");
    }
}

void append_audio(
    std::vector<float>& left,
    std::vector<float>& right,
    const std::span<const float> leftChunk,
    const std::span<const float> rightChunk
) {
    left.insert(left.end(), leftChunk.begin(), leftChunk.end());
    right.insert(right.end(), rightChunk.begin(), rightChunk.end());
}

void render_frames(
    fluid_synth_t*      synth,
    const size_t        frames,
    std::vector<float>& left,
    std::vector<float>& right
) {
    if (frames == 0) {
        return;
    }

    std::vector<float> leftChunk(frames);
    std::vector<float> rightChunk(frames);
    check_fluid_call(
        fluid_synth_write_float(
            synth,
            static_cast<int>(frames),
            leftChunk.data(),
            0,
            1,
            rightChunk.data(),
            0,
            1
        ),
        "FluidLite failed to render audio"
    );
    append_audio(left, right, leftChunk, rightChunk);
}

void apply_event(
    fluid_synth_t*              synth,
    const unsigned int          soundfontId,
    const std::vector<ChannelState>& channels,
    const RenderEvent&          event
) {
    const auto channel = static_cast<int>(event.channel);
    switch (event.type) {
        case EventType::bank_select:
            check_fluid_call(
                fluid_synth_program_select(
                    synth,
                    channel,
                    soundfontId,
                    static_cast<unsigned int>(event.a),
                    channels[event.channel].program
                ),
                "FluidLite failed to apply bank routing"
            );
            return;
        case EventType::control_change:
            check_fluid_call(
                fluid_synth_cc(synth, channel, event.a, event.b),
                "FluidLite failed to apply a control change"
            );
            return;
        case EventType::pitch_bend:
            check_fluid_call(
                fluid_synth_pitch_bend(synth, channel, event.a),
                "FluidLite failed to apply a pitch bend"
            );
            return;
        case EventType::note_off:
            check_fluid_call(
                fluid_synth_noteoff(synth, channel, event.a),
                "FluidLite failed to turn a note off"
            );
            return;
        case EventType::note_on:
            check_fluid_call(
                fluid_synth_noteon(synth, channel, event.a, event.b),
                "FluidLite failed to turn a note on"
            );
            return;
    }
    throw std::runtime_error("Unknown render event type");
}

std::vector<RenderEvent> build_events(
    const Score<Second>&        score,
    std::vector<ChannelState>& channels
) {
    std::vector<RenderEvent> events;
    channels.clear();
    channels.reserve(score.tracks->size());
    events.reserve(score.note_num() * 2);

    for (size_t trackIndex = 0; trackIndex < score.tracks->size(); ++trackIndex) {
        if (trackIndex >= kMaxMidiChannels) {
            throw std::runtime_error("FluidLite renderer supports at most 256 routed tracks");
        }

        const auto& track = *score.tracks->at(trackIndex);
        const auto  channel = static_cast<u16>(trackIndex);
        channels.push_back(ChannelState{track.program, track.is_drum});

        const auto banks = details::getBanks(track);
        for (const auto& bank : banks) {
            if (bank.time == std::numeric_limits<Second::unit>::max()) {
                continue;
            }
            events.push_back(RenderEvent{
                bank.time,
                EventType::bank_select,
                channel,
                static_cast<i32>(bank.bank),
                0,
            });
        }

        for (const auto& control : *track.controls) {
            if (!track.is_drum && control->number == 0) {
                continue;
            }
            events.push_back(RenderEvent{
                control->time,
                EventType::control_change,
                channel,
                control->number,
                control->value,
            });
        }

        for (const auto& pitchBend : *track.pitch_bends) {
            const auto pitchValue
                = clamp_value(pitchBend->value + kCenterPitchBend, 0, 16383);
            events.push_back(RenderEvent{
                pitchBend->time,
                EventType::pitch_bend,
                channel,
                pitchValue,
                0,
            });
        }

        for (const auto& pedal : *track.pedals) {
            events.push_back(RenderEvent{
                pedal->start(),
                EventType::control_change,
                channel,
                kSustainPedalControl,
                127,
            });
            events.push_back(RenderEvent{
                pedal->end(),
                EventType::control_change,
                channel,
                kSustainPedalControl,
                0,
            });
        }

        for (const auto& note : *track.notes) {
            if (note->empty()) {
                continue;
            }
            events.push_back(RenderEvent{
                note->start(),
                EventType::note_on,
                channel,
                note->pitch,
                note->velocity,
            });
            events.push_back(RenderEvent{
                note->end(),
                EventType::note_off,
                channel,
                note->pitch,
                0,
            });
        }
    }

    std::sort(events.begin(), events.end(), render_event_less);
    return events;
}

AudioBuffer finalize_audio(
    std::vector<float>& left,
    std::vector<float>& right,
    const bool          stereo
) {
    const auto frames = left.size();
    AudioBuffer buffer(stereo ? 2 : 1, frames);
    auto* out = buffer.data();

    if (stereo) {
        for (size_t frame = 0; frame < frames; ++frame) {
            out[frame] = left[frame];
            out[frames + frame] = right[frame];
        }
        return buffer;
    }

    for (size_t frame = 0; frame < frames; ++frame) {
        out[frame] = 0.5f * (left[frame] + right[frame]);
    }
    return buffer;
}

bool chunk_is_silent(const std::span<const float> left, const std::span<const float> right) {
    for (size_t i = 0; i < left.size(); ++i) {
        if (std::abs(left[i]) > kSilenceThreshold || std::abs(right[i]) > kSilenceThreshold) {
            return false;
        }
    }
    return true;
}

}   // namespace

AudioBuffer::AudioBuffer(const size_t channels, const size_t frames) :
    channelCount(channels),
    frameCount(frames),
    dataPtr(std::make_unique<float[]>(channels * frames)) {
    std::fill_n(dataPtr.get(), channels * frames, 0.0f);
}

float* AudioBuffer::data() {
    if (dataPtr == nullptr) {
        throw std::runtime_error("AudioBuffer data has been moved");
    }
    return dataPtr.get();
}

const float* AudioBuffer::data() const {
    if (dataPtr == nullptr) {
        throw std::runtime_error("AudioBuffer data has been moved");
    }
    return dataPtr.get();
}

float* AudioBuffer::release() {
    if (dataPtr == nullptr) {
        throw std::runtime_error("AudioBuffer data has been moved");
    }
    return dataPtr.release();
}

std::tuple<size_t, size_t> AudioBuffer::dims() const {
    return {channelCount, frameCount};
}

void write_wav(
    const std::filesystem::path& path,
    const float*                 data,
    const size_t                 channels,
    const size_t                 frames,
    const i32                    sampleRate,
    const bool                   useInt16
) {
    if (channels == 0 || frames == 0) {
        throw std::invalid_argument("WAV output requires at least one channel and one frame");
    }
    if (sampleRate <= 0) {
        throw std::invalid_argument("sample_rate must be positive");
    }

    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Failed to open WAV output path: " + path.string());
    }

    const std::uint16_t channelCount = static_cast<std::uint16_t>(channels);
    const std::uint16_t bitsPerSample = useInt16 ? 16 : 32;
    const std::uint32_t bytesPerSample = bitsPerSample / 8;
    const std::uint32_t dataSize
        = static_cast<std::uint32_t>(channels * frames * bytesPerSample);
    const std::uint32_t byteRate = static_cast<std::uint32_t>(
        sampleRate * static_cast<i32>(channels) * static_cast<i32>(bytesPerSample)
    );
    const std::uint16_t blockAlign = static_cast<std::uint16_t>(channels * bytesPerSample);
    const std::uint16_t audioFormat = useInt16 ? 1 : 3;

    stream.write("RIFF", 4);
    write_u32(stream, 36u + dataSize);
    stream.write("WAVE", 4);
    stream.write("fmt ", 4);
    write_u32(stream, 16);
    write_u16(stream, audioFormat);
    write_u16(stream, channelCount);
    write_u32(stream, static_cast<std::uint32_t>(sampleRate));
    write_u32(stream, byteRate);
    write_u16(stream, blockAlign);
    write_u16(stream, bitsPerSample);
    stream.write("data", 4);
    write_u32(stream, dataSize);

    for (size_t frame = 0; frame < frames; ++frame) {
        for (size_t channel = 0; channel < channels; ++channel) {
            const auto sample = data[channel * frames + frame];
            if (useInt16) {
                const auto pcm = static_cast<std::int16_t>(std::lrint(
                    clamp_value(sample, -1.0f, 1.0f) * 32767.0f
                ));
                write_i16(stream, pcm);
            } else {
                stream.write(reinterpret_cast<const char*>(&sample), sizeof(sample));
            }
        }
    }
}

class Synthesizer::Impl {
public:
    std::mutex         renderMutex;
    FluidSfLoaderPtr   sfLoader;
    FluidSettingsPtr settings;
    FluidSynthPtr    synth;
    fluid_sfont_t*    soundfont;
    std::string      sfPath;
    SynthInterpolation interpolation;
    bool             reverb;
    f64              reverbRoomsize;
    f64              reverbDamp;
    f64              reverbWidth;
    f64              reverbLevel;
    bool             chorus;
    u32              chorusNr;
    f64              chorusLevel;
    f64              chorusSpeed;
    f64              chorusDepth;
    ChorusWaveform   chorusWaveform;
    int              soundfontId;
    u32              sampleRate;

    Impl(
        const std::string& sfPath,
        const u32          sampleRate,
        const f64          gain,
        const u32          polyphony,
        const SynthInterpolation interpolation,
        const bool         reverb,
        const f64          reverbRoomsize,
        const f64          reverbDamp,
        const f64          reverbWidth,
        const f64          reverbLevel,
        const bool         chorus,
        const u32          chorusNr,
        const f64          chorusLevel,
        const f64          chorusSpeed,
        const f64          chorusDepth,
        const ChorusWaveform chorusWaveform
    ) : sfLoader(new_fluid_defsfloader()),
        settings(new_fluid_settings()),
        synth(nullptr),
        soundfont(nullptr),
        sfPath(sfPath),
        interpolation(interpolation),
        reverb(reverb),
        reverbRoomsize(reverbRoomsize),
        reverbDamp(reverbDamp),
        reverbWidth(reverbWidth),
        reverbLevel(reverbLevel),
        chorus(chorus),
        chorusNr(chorusNr),
        chorusLevel(chorusLevel),
        chorusSpeed(chorusSpeed),
        chorusDepth(chorusDepth),
        chorusWaveform(chorusWaveform),
        soundfontId(0),
        sampleRate(sampleRate) {
        if (sfLoader.get() == nullptr) {
            throw std::runtime_error("Failed to allocate FluidLite soundfont loader");
        }
        if (settings.get() == nullptr) {
            throw std::runtime_error("Failed to allocate FluidLite settings");
        }
        check_fluid_setting(
            fluid_settings_setnum(settings.get(), "synth.sample-rate", sampleRate),
            "Failed to configure FluidLite sample rate"
        );
        check_fluid_setting(
            fluid_settings_setnum(settings.get(), "synth.gain", gain),
            "Failed to configure FluidLite gain"
        );
        check_fluid_setting(
            fluid_settings_setint(settings.get(), "synth.polyphony", static_cast<int>(polyphony)),
            "Failed to configure FluidLite polyphony"
        );
        check_fluid_setting(
            fluid_settings_setint(settings.get(), "synth.midi-channels", kMaxMidiChannels),
            "Failed to configure FluidLite MIDI channels"
        );
        soundfont = sfLoader.get()->load(sfLoader.get(), sfPath.c_str());
        if (soundfont == nullptr) {
            throw std::runtime_error("Failed to load soundfont: " + sfPath);
        }
        recreate_synth();
    }

    ~Impl() {
        detach_soundfont();
        if (soundfont != nullptr) {
            soundfont->free(soundfont);
            soundfont = nullptr;
        }
    }

    void detach_soundfont() {
        if (synth.get() != nullptr && soundfont != nullptr) {
            fluid_synth_remove_sfont(synth.get(), soundfont);
        }
    }

    void recreate_synth() {
        detach_soundfont();
        synth = FluidSynthPtr();
        synth = FluidSynthPtr(new_fluid_synth(settings.get()));
        if (synth.get() == nullptr) {
            throw std::runtime_error("Failed to allocate FluidLite synthesizer");
        }
        fluid_synth_set_sample_rate(synth.get(), static_cast<float>(sampleRate));

        soundfontId = fluid_synth_add_sfont(synth.get(), soundfont);
        if (soundfontId == -1) {
            throw std::runtime_error("Failed to attach loaded soundfont to FluidLite synth");
        }

        check_fluid_call(
            fluid_synth_set_interp_method(
                synth.get(),
                -1,
                static_cast<int>(interpolation)
            ),
            "Failed to configure FluidLite interpolation"
        );
        fluid_synth_set_reverb_on(synth.get(), reverb ? 1 : 0);
        fluid_synth_set_reverb(
            synth.get(),
            reverbRoomsize,
            reverbDamp,
            reverbWidth,
            reverbLevel
        );
        fluid_synth_set_chorus_on(synth.get(), chorus ? 1 : 0);
        fluid_synth_set_chorus(
            synth.get(),
            static_cast<int>(chorusNr),
            chorusLevel,
            chorusSpeed,
            chorusDepth,
            static_cast<int>(chorusWaveform)
        );
    }

    AudioBuffer render_seconds(const Score<Second>& score, const bool stereo) {
        std::lock_guard<std::mutex> guard(renderMutex);

        if (score.tracks->empty()) {
            throw std::runtime_error("No valid tracks in the score");
        }
        recreate_synth();

        std::vector<ChannelState> channels;
        const auto events = build_events(score, channels);
        if (channels.empty()) {
            throw std::runtime_error("No valid tracks in the score");
        }

        for (size_t channel = 0; channel < channels.size(); ++channel) {
            const auto initialBank = channels[channel].isDrum ? 128u : 0u;
            check_fluid_call(
                fluid_synth_program_select(
                    synth.get(),
                    static_cast<int>(channel),
                    static_cast<unsigned int>(soundfontId),
                    initialBank,
                    channels[channel].program
                ),
                "Failed to initialize FluidLite program routing"
            );
            check_fluid_call(
                fluid_synth_pitch_bend(
                    synth.get(),
                    static_cast<int>(channel),
                    kCenterPitchBend
                ),
                "Failed to initialize FluidLite pitch bend"
            );
        }

        std::vector<float> left;
        std::vector<float> right;
        left.reserve(seconds_to_frames(score.end() + kMaxTailSeconds, sampleRate));
        right.reserve(left.capacity());

        u32 currentFrame = 0;
        for (const auto& event : events) {
            const auto targetFrame = seconds_to_frames(event.time, sampleRate);
            if (targetFrame > currentFrame) {
                render_frames(synth.get(), targetFrame - currentFrame, left, right);
                currentFrame = targetFrame;
            }
            apply_event(synth.get(), soundfontId, channels, event);
        }

        const auto maxTailFrames = static_cast<u32>(std::llround(kMaxTailSeconds * sampleRate));
        u32 renderedTailFrames = 0;
        while (renderedTailFrames < maxTailFrames) {
            const auto chunkFrames = std::min<u32>(
                static_cast<u32>(kRenderChunkFrames),
                maxTailFrames - renderedTailFrames
            );
            const auto chunkStart = left.size();
            render_frames(synth.get(), chunkFrames, left, right);
            renderedTailFrames += chunkFrames;
            currentFrame += chunkFrames;

            const std::span<const float> leftChunk(left.data() + chunkStart, chunkFrames);
            const std::span<const float> rightChunk(right.data() + chunkStart, chunkFrames);
            if (fluid_synth_get_active_voice_count(synth.get()) == 0
                && chunk_is_silent(leftChunk, rightChunk)) {
                break;
            }
        }

        while (!left.empty() && !right.empty()) {
            const auto last = left.size() - 1;
            if (std::abs(left[last]) > kSilenceThreshold || std::abs(right[last]) > kSilenceThreshold) {
                break;
            }
            left.pop_back();
            right.pop_back();
        }

        if (left.empty()) {
            left.push_back(0.0f);
            right.push_back(0.0f);
        }

        return finalize_audio(left, right, stereo);
    }
};

Synthesizer::Synthesizer(
    const std::string& sfPath,
    const u32          sampleRate,
    const f64          gain,
    const u32          polyphony,
    const SynthInterpolation interpolation,
    const bool         reverb,
    const f64          reverbRoomsize,
    const f64          reverbDamp,
    const f64          reverbWidth,
    const f64          reverbLevel,
    const bool         chorus,
    const u32          chorusNr,
    const f64          chorusLevel,
    const f64          chorusSpeed,
    const f64          chorusDepth,
    const ChorusWaveform chorusWaveform
) {
    validate_ctor_args(
        sfPath,
        sampleRate,
        gain,
        polyphony,
        chorusNr,
        chorusLevel,
        chorusSpeed,
        chorusDepth,
        reverbLevel
    );
    impl = std::make_unique<Impl>(
        sfPath,
        sampleRate,
        gain,
        polyphony,
        interpolation,
        reverb,
        reverbRoomsize,
        reverbDamp,
        reverbWidth,
        reverbLevel,
        chorus,
        chorusNr,
        chorusLevel,
        chorusSpeed,
        chorusDepth,
        chorusWaveform
    );
}

Synthesizer::Synthesizer(
    const std::filesystem::path& sfPath,
    const u32                    sampleRate,
    const f64                    gain,
    const u32                    polyphony,
    const SynthInterpolation     interpolation,
    const bool                   reverb,
    const f64                    reverbRoomsize,
    const f64                    reverbDamp,
    const f64                    reverbWidth,
    const f64                    reverbLevel,
    const bool                   chorus,
    const u32                    chorusNr,
    const f64                    chorusLevel,
    const f64                    chorusSpeed,
    const f64                    chorusDepth,
    const ChorusWaveform         chorusWaveform
) : Synthesizer(
        sfPath.string(),
        sampleRate,
        gain,
        polyphony,
        interpolation,
        reverb,
        reverbRoomsize,
        reverbDamp,
        reverbWidth,
        reverbLevel,
        chorus,
        chorusNr,
        chorusLevel,
        chorusSpeed,
        chorusDepth,
        chorusWaveform
    ) {}

Synthesizer::~Synthesizer() = default;
Synthesizer::Synthesizer(Synthesizer&&) noexcept = default;
Synthesizer& Synthesizer::operator=(Synthesizer&&) noexcept = default;

namespace details {

vec<BankSelect> getBanks(const Track<Second>& track) {
    vec<BankSelect> banks;
    if (track.is_drum) {
        banks.push_back(BankSelect{0.0, 128});
        banks.push_back(BankSelect{std::numeric_limits<Second::unit>::max(), 128});
        return banks;
    }

    for (const auto& control : *track.controls) {
        if (control->number == 0) {
            banks.push_back(BankSelect{control->time, control->value});
        }
    }
    if (banks.empty()) {
        banks.push_back(BankSelect{0.0, 0});
        banks.push_back(BankSelect{std::numeric_limits<Second::unit>::max(), 0});
        return banks;
    }
    std::sort(banks.begin(), banks.end());
    if (banks.front().time != 0.0) {
        banks.insert(banks.begin(), BankSelect{0.0, 0});
    }
    banks.push_back(BankSelect{std::numeric_limits<Second::unit>::max(), banks.back().bank});
    return banks;
}

}   // namespace details

template<TType T>
AudioBuffer Synthesizer::render(const Score<T>& score, const bool stereo) {
    if constexpr (std::is_same_v<T, Second>) {
        return impl->render_seconds(score, stereo);
    } else {
        return impl->render_seconds(convert<Second>(score), stereo);
    }
}

template AudioBuffer Synthesizer::render(const Score<Tick>&, bool);
template AudioBuffer Synthesizer::render(const Score<Quarter>&, bool);
template AudioBuffer Synthesizer::render(const Score<Second>&, bool);

}   // namespace symusic
