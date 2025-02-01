//
// Created by lyk on 25-1-26.
//

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include "pdqsort.h"
#include "MetaMacro.h"

#include "minimx.h"
#include "symusic/score.h"
#include "symusic/ops.h"
#include "symusic/repr.h"
#include "symusic/conversion.h"
#include "fmt/format.h"
#include "unordered_dense.h"

namespace symusic {

namespace details {

/// Default MIDI velocity when not specified
constexpr i8 DEFAULT_VELOCITY = 90;

/**
 * @brief Manages tie relationships between notes
 *
 * Handles tie start/stop logic and maintains active tie connections.
 * Automatically merges tied notes and processes pending ties.
 */

struct PendingTie {
    i8   pitch;
    i8   velocity;
    bool then_start;
    f64  time;
    f64  duration;

    PendingTie(i8 pitch, i8 velocity, f64 time, f64 duration, bool the_start) :
        pitch(pitch), velocity(velocity), then_start(the_start), time(time), duration(duration) {}
};


class TieManager {
    ankerl::unordered_dense::map<std::pair<i8, i32>, size_t> m_tie_start_indices;
    std::vector<Note<Quarter>>&                              m_notes;
    std::vector<PendingTie>                                  m_pending_ties;

    /**
     * @brief Quantize time value for precise comparison
     * @param time Original time value in quarters
     * @return Quantized integer representation
     */
    static i32 quantizeTime(const f64 time) noexcept {
        return static_cast<i32>(std::round(time * 1000));
    }

public:
    /**
     * @brief Construct a new Tie Manager object
     * @param notes Reference to the note collection being built
     */
    explicit TieManager(std::vector<Note<Quarter>>& notes) :
        m_tie_start_indices(notes.capacity() / 2), m_notes(notes) {}

    /**
     * @brief Register a new tie start point
     * @param index Note index in the collection
     */
    void registerTieStart(const size_t index) {
        const auto& note = m_notes[index];
        const auto  key  = std::make_pair(note.pitch, quantizeTime(note.end()));

        if (auto [iter, inserted] = m_tie_start_indices.try_emplace(key, index); !inserted) {
            if (note.time > m_notes[iter->second].time) {
                iter->second = index;   // Keep later note for same position
            }
        }
    }

    /**
     * @brief Process a tie stop event
     * @param pitch MIDI pitch value to match
     * @param time Exact stop time in quarters
     * @param duration Additional duration to extend
     * @return SIZE_MAX if no match found, otherwise extended note index
     */
    template<bool post_processing = false>
    size_t processTieStop(const i8 pitch, const i8 velocity, const f64 time, const f64 duration) {
        const auto iter = m_tie_start_indices.find({pitch, quantizeTime(time)});
        if (iter == m_tie_start_indices.end()) {
            if constexpr (post_processing) {
                m_notes.emplace_back(time, duration, pitch, velocity);
                return m_notes.size() - 1;
            } else {
                m_pending_ties.emplace_back(pitch, velocity, time, duration, false);
                return SIZE_MAX;
            }
        }

        const size_t index = iter->second;
        m_notes[index].duration += static_cast<f32>(duration);
        m_tie_start_indices.erase(iter);
        return index;
    }

    /**
     * @brief Process combined stop-start tie event
     * @param pitch MIDI pitch value to match
     * @param velocity MIDI velocity value
     * @param time Exact stop time in quarters
     * @param duration Additional duration to extend
     */
    template<bool post_processing = false>
    void processTieStopAndStart(
        const i8 pitch, const i8 velocity, const f64 time, const f64 duration
    ) {
        const size_t index = processTieStop<post_processing>(pitch, velocity, time, duration);
        if constexpr (!post_processing) {
            if (index == SIZE_MAX) {
                m_pending_ties.back().then_start = true;
                return;
            }
        }
        registerTieStart(index);
    }

    /**
     * @brief Final processing of unresolved ties
     *
     * Should be called after processing all measure elements. Sorts pending
     * ties by time and attempts to resolve them in chronological order.
     */
    void processPendingTies() {
        if (m_pending_ties.empty()) return;

        gfx::timsort(
            m_pending_ties.begin(),
            m_pending_ties.end(),
            [](const auto& a, const auto& b) { return (a.time) < (b.time); }
        );

        for (const auto& [pitch, velocity, then_start, time, duration] : m_pending_ties) {
            size_t index = processTieStop<true>(pitch, velocity, time, duration);
            if (index == SIZE_MAX) {
                m_notes.emplace_back(time, duration, pitch, velocity);
                index = m_notes.size() - 1;
            }
            if (then_start) registerTieStart(index);
        }
    }
};

// ====================== Utility Functions ======================
namespace {

/**
 * @brief Calculate quarter duration for a time signature
 * @param beats Number of beats per measure
 * @param beat_type Beat unit (4 = quarter note)
 * @return Measure duration in quarter notes
 */
constexpr f64 measureDurationQuarters(const i32 beats, const i32 beat_type) noexcept {
    return static_cast<f64>(beats * 4) / beat_type;
}

/**
 * @brief Convert MusicXML pitch to MIDI number with transposition
 * @param pitch Source pitch information
 * @param transpose Transposition parameters
 * @return Valid MIDI pitch value (0-127)
 * @throws std::runtime_error for invalid pitch values
 */
i8 parseMidiPitch(const minimx::Pitch& pitch, const minimx::Transpose& transpose) {
    const i32 midi_pitch = transpose.empty() ? pitch.midi_pitch() : pitch.midi_pitch(transpose);

    if (midi_pitch < 0 || midi_pitch > 127) {
        throw std::runtime_error(
            fmt::format(
                "Invalid MIDI pitch {} (transposed from {})", midi_pitch, pitch.midi_pitch()
            )
        );
    }
    return static_cast<i8>(midi_pitch);
}
}   // namespace

// ====================== Note Processing ======================
namespace {
/// Previous note type tracking for chord handling
enum class PreviousNoteType : u8 { None, Single, Chord };

/**
 * @brief Adjust timing for chord sequences
 * @param element Current measure element
 * @param current_time Reference to current time position
 * @param chord_duration Accumulated chord duration
 * @param previous_type Tracking of previous note type
 * @param duration Current element duration
 */
void updateChordTiming(
    const minimx::MeasureElement& element,
    f64&                          current_time,
    f64&                          chord_duration,
    PreviousNoteType&             previous_type,
    const f64                     duration
) {
    if (element.isChordTone) {
        if (previous_type != PreviousNoteType::Chord) {
            current_time -= chord_duration;   // Rewind to chord start
        }
        previous_type = PreviousNoteType::Chord;
    } else {
        if (previous_type == PreviousNoteType::Chord) {
            current_time += chord_duration;   // Advance to chord end
        }
        chord_duration = duration;   // Store new base duration
        previous_type  = PreviousNoteType::Single;
    }
}
}   // namespace

/**
 * @brief Process a single note element
 * @param element XML measure element to process
 * @param notes Target note collection
 * @param tie_manager Tie relationship manager
 * @param current_time Reference to current time position
 * @param chord_duration Accumulated chord duration
 * @param previous_note_type Tracking of previous note type
 * @param velocity Current MIDI velocity
 * @param duration Calculated note duration
 * @param transpose Transposition parameters
 */
void processNoteElement(
    const minimx::MeasureElement& element,
    std::vector<Note<Quarter>>&   notes,
    TieManager&                   tie_manager,
    f64&                          current_time,
    f64&                          chord_duration,
    PreviousNoteType&             previous_note_type,
    const i8                      velocity,
    const f64                     duration,
    const minimx::Transpose&      transpose
) {
    if (element.actualNotes < 1) {
        throw std::runtime_error(fmt::format("Invalid actual-notes value {}", element.actualNotes));
    }

    // updateChordTiming(element, current_time, chord_duration, previous_note_type, duration);

    if (element.isRest) {
        current_time += element.isChordTone ? 0 : duration;
        return;
    }

    const i8 pitch = parseMidiPitch(element.pitch, transpose);

    switch (element.tie) {
    case minimx::Tie::Start:
        notes.emplace_back(current_time, duration, pitch, velocity);
        tie_manager.registerTieStart(notes.size() - 1);
        break;

    case minimx::Tie::Stop:
        tie_manager.processTieStop(pitch, velocity, current_time, duration);
        break;

    case minimx::Tie::StopStart:
        tie_manager.processTieStopAndStart(pitch, velocity, current_time, duration);
        break;

    case minimx::Tie::NotTied: notes.emplace_back(current_time, duration, pitch, velocity); break;

    default: throw std::runtime_error("Undefined tie type in note element");
    }

    current_time += element.isChordTone ? 0 : duration;
}

f64 quantTime(const f64 time, const f64 divisions) {
    return std::round(time * divisions) / divisions;
}



// ====================== Main Parsing Logic ======================
/**
 * @brief Parse notes from a MusicXML part
 * @param part Source part data
 * @return Sorted collection of notes with ties resolved
 */
std::vector<Note<Quarter>> parsePartNotes(const minimx::Part& part) {
    std::vector<Note<Quarter>> notes;
    notes.reserve(part.measures.size() * 16);   // Empirical pre-allocation

    f64 current_time      = 0.0;
    f64 divisions         = 0.0;
    i32 current_beats     = 0;
    i32 current_beat_type = 0;

    TieManager tie_manager{notes};
    i8         current_velocity = DEFAULT_VELOCITY;

    for (const auto& measure : part.measures) {
        f64              chord_duration     = 0.0;
        f64              measure_end        = current_time;
        PreviousNoteType previous_note_type = PreviousNoteType::None;


        if (measure.attributes.divisions > 0) { divisions = measure.attributes.divisions; }

        // Update time signature tracking
        if (measure.attributes.time.beats > 0) { current_beats = measure.attributes.time.beats; }
        if (measure.attributes.time.beatType > 0) {
            current_beat_type = measure.attributes.time.beatType;
        }

        const f64 measure_start = current_time;
        const f64 exp_measure_end
            = measure_start + measureDurationQuarters(current_beats, current_beat_type);
        const auto& transpose = measure.attributes.transpose;

        // Update dynamic from measure
        if (measure.sound.dynamics > 0) {
            current_velocity = static_cast<i8>(measure.sound.dynamics * 0.9);
        }

        for (const auto& element : measure.elements) {
            const f64 duration = static_cast<f64>(element.duration) / divisions;
            if (duration < 0.0) {
                throw std::runtime_error(
                    "symusic: Negative duration in measure while parsing MusicXML"
                );
            }

            // Update current time and chord duration
            updateChordTiming(element, current_time, chord_duration, previous_note_type, duration);
            measure_end = std::max(measure_end, current_time);

            switch (element.type) {
            case minimx::MeasureElementType::Note:
                processNoteElement(
                    element,
                    notes,
                    tie_manager,
                    current_time,
                    chord_duration,
                    previous_note_type,
                    current_velocity,
                    duration,
                    transpose
                );
                break;

            case minimx::MeasureElementType::Backup:
                current_time -= duration;
                previous_note_type = PreviousNoteType::None;
                break;

            case minimx::MeasureElementType::Forward:
                current_time += duration;
                previous_note_type = PreviousNoteType::None;
                break;
            }

            current_time = quantTime(current_time, divisions);
            measure_end  = std::max(measure_end, current_time);

            if (current_time < measure_start) {
                throw std::runtime_error(
                    fmt::format(
                        "symusic: Negative time in measure while parsing MusicXML."
                        " Current time: {}, measure start: {}, duration: {}, isBackup: {}",
                        current_time,
                        measure_start,
                        duration,
                        element.type == minimx::MeasureElementType::Backup
                    )
                );
            }
        }

        // Advance to next measure
        current_time = measure_end;
        if (std::abs(measure_end - exp_measure_end) > 0.1) {
            fmt::println(
                "symusic: Measure duration mismatch in MusicXML (expected: {}, actual: {})",
                exp_measure_end,
                measure_end
            );
        }
    }

    tie_manager.processPendingTies();
    gfx::timsort(notes.begin(), notes.end(), [](const auto& a, const auto& b) {
        return (a.time) < (b.time);
    });
    return notes;
}

namespace {

i8 parseTonality(const std::string& mode) {
    // 127 for an empty string (not exist in MusicXML)
    if (mode.empty()) return 127;

    // Check most common modes first
    if (mode == "major") return 0;
    if (mode == "minor") return 1;

    // Check other modes
    if (mode == "ionian" || mode == "lydian" || mode == "mixolydian" || mode == "none") {
        return 0;
    }
    if (mode == "aeolian" || mode == "dorian" || mode == "phyrgian" || mode == "locrian") {
        return 1;
    }

    throw std::runtime_error("symusic: Invalid mode (" + mode + ") in key signature from MusicXML");
}

struct TimeSigManager {
    vec<TimeSignature<Quarter>> time_signatures;
    i32                         prev_beats = 0, prev_beat_type = 0;
    i32                         cur_beats = 0, cur_beat_type = 0;
    bool                        first = true;

    TimeSigManager() = default;

    void reset() {
        first     = true;
        cur_beats = cur_beat_type = 0;
    }

    void end(const f64 cur_time) {
        if (cur_beats > 0 && cur_beat_type > 0
            && (cur_beats != prev_beats || cur_beat_type != prev_beat_type)) {
            time_signatures.emplace_back(cur_time, cur_beats, cur_beat_type);
            prev_beats     = cur_beats;
            prev_beat_type = cur_beat_type;
        }
        reset();
    }

    void process(const minimx::MeasureAttributes& attributes) {
        if (first) {
            cur_beats     = attributes.time.beats;
            cur_beat_type = attributes.time.beatType;
            first         = false;
        } else if (cur_beats != attributes.time.beats
                   || cur_beat_type != attributes.time.beatType) {
            throw std::runtime_error(
                "symusic: Time signature mismatch in different parts when parsing musicxml"
            );
        }
    }
};

struct KeySigManager {
    vec<KeySignature<Quarter>> key_signatures;
    i8                         prev_key = 127, prev_tonality = 127;
    i8                         cur_key = 127, cur_tonality = 127;
    bool                       first = true;

    void reset() {
        first   = true;
        cur_key = cur_tonality = 127;
    }

    void end(const f64 cur_time) {
        if (cur_key != 127 && cur_tonality != 127
            && (cur_key != prev_key || cur_tonality != prev_tonality)) {
            key_signatures.emplace_back(cur_time, cur_key, cur_tonality);
            prev_key      = cur_key;
            prev_tonality = cur_tonality;
        }
        reset();
    }

    void process(const minimx::MeasureAttributes& attributes) {
        const auto tonality = parseTonality(attributes.key.mode);
        if (first) {
            cur_key      = static_cast<i8>(attributes.key.fifths);
            cur_tonality = tonality;
            first        = false;
        } else if (cur_key != attributes.key.fifths || cur_tonality != tonality) {
            throw std::runtime_error(
                "symusic: Key signature mismatch in different parts when parsing musicxml"
            );
        }
    }
};

struct TempoManager {
    vec<Tempo<Quarter>> tempos;
    f64                 prev_tempo = -1.0;
    f64                 cur_tempo  = -1.0;
    bool                first      = true;

    void reset() {
        first     = true;
        cur_tempo = -1.0;
    }

    void end(const f64 cur_time) {
        if (cur_tempo > 0.0 && cur_tempo != prev_tempo) {
            tempos.emplace_back(cur_time, cur_tempo);
            prev_tempo = cur_tempo;
        }
        reset();
    }

    void process(const minimx::Sound& sound) {
        if (first) {
            cur_tempo = sound.tempo;
            first     = false;
        } else if (cur_tempo != sound.tempo) {
            throw std::runtime_error(
                "symusic: Tempo mismatch in different parts when parsing musicxml"
            );
        }
    }
};


/**
 * @brief Build time signature sequence from MusicXML data
 * @param score Source score data
 * @return Validated time signature sequence
 */
void constructGlobalEvents(const minimx::MXScore& score, ScoreNative<Quarter>& result) {
    if (score.parts.empty()) throw std::runtime_error("Empty score");

    TimeSigManager time_sig_manager;
    KeySigManager  key_sig_manager;
    TempoManager   tempo_manager;

    f64    current_time = 0.0;
    size_t max_measures = 0;
    for (const auto& part : score.parts) {
        max_measures = std::max(max_measures, part.measures.size());
    }

    for (size_t i = 0; i < max_measures; ++i) {
        for (const auto& part : score.parts) {
            if (i >= part.measures.size()) continue;

            const auto& measure = part.measures[i];
            const auto& attr    = measure.attributes;

            time_sig_manager.process(attr);
            key_sig_manager.process(attr);
            tempo_manager.process(measure.sound);
        }

        time_sig_manager.end(current_time);
        key_sig_manager.end(current_time);
        tempo_manager.end(current_time);

        current_time += measureDurationQuarters(
            time_sig_manager.prev_beats, time_sig_manager.prev_beat_type
        );
    }

    result.time_signatures = std::move(time_sig_manager.time_signatures);
    result.key_signatures  = std::move(key_sig_manager.key_signatures);
    result.tempos          = std::move(tempo_manager.tempos);
}

}   // namespace

// ====================== Entry Point ======================
/**
 * @brief Convert MusicXML document to ScoreNative representation
 * @param doc Parsed MusicXML document
 * @return Converted score with tracks and time signatures
 */
ScoreNative<Quarter> parse_musicxml_native(const pugi::xml_document& doc) {
    const minimx::MXScore score{doc};
    ScoreNative<Quarter>  result{960};   // Default ticks_per_quarter

    if (score.parts.empty()) return result;

    // Time signature, key signature, tempo
    constructGlobalEvents(score, result);

    for (const auto& part : score.parts) {
        TrackNative<Quarter> track;
        track.name    = part.name;
        track.program = std::clamp(part.midiInstrument.program - 1, 0, 127);

        // Volume control
        if (part.midiInstrument.volume > 0) {
            track.controls.emplace_back(0, 7, static_cast<u8>(part.midiInstrument.volume * 1.27));
        }

        track.notes = parsePartNotes(part);
        result.tracks.push_back(std::move(track));
    }

    return result;
}

}   // namespace details

#define INSTANTIATE_XML_DUMP(__COUNT, T)                                   \
    template<>                                                             \
    template<>                                                             \
    vec<u8> Score<T>::dumps<DataFormat::MusicXML>() const {                \
        throw std::runtime_error("Dumping MusicXML is not supported yet"); \
        return {};                                                         \
    }

REPEAT_ON(INSTANTIATE_XML_DUMP, Tick, Quarter, Second)
#undef INSTANTIATE_XML_DUMP

template<>
template<>
Score<Quarter> Score<Quarter>::parse<DataFormat::MusicXML>(const std::span<const u8> bytes) {
    pugi::xml_document doc;
    doc.load_buffer(bytes.data(), bytes.size());
    auto score_native = details::parse_musicxml_native(doc);
    return to_shared<Quarter>(std::move(score_native));
}

template<>
template<>
Score<Tick> Score<Tick>::parse<DataFormat::MusicXML>(const std::span<const u8> bytes) {
    // return convert<Tick>(parse<DataFormat::MusicXML, Score<Quarter>>(bytes));
    pugi::xml_document doc;
    doc.load_buffer(bytes.data(), bytes.size());
    auto score_native = details::parse_musicxml_native(doc);
    return convert<Tick>(to_shared<Quarter>(std::move(score_native)));
}

template<>
template<>
Score<Second> Score<Second>::parse<DataFormat::MusicXML>(const std::span<const u8> bytes) {
    // return convert<Second>(parse<DataFormat::MusicXML, Score<Quarter>>(bytes));
    pugi::xml_document doc;
    doc.load_buffer(bytes.data(), bytes.size());
    auto score_native = details::parse_musicxml_native(doc);
    return convert<Second>(to_shared<Quarter>(std::move(score_native)));
}

#define INSTANTIATE_GLOBAL_FUNC(__COUNT, T)                                     \
    template<>                                                                  \
    Score<T> parse<DataFormat::MusicXML, Score<T>>(std::span<const u8> bytes) { \
        return Score<T>::parse<DataFormat::MusicXML>(bytes);                    \
    }                                                                           \
    template<>                                                                  \
    vec<u8> dumps<DataFormat::MusicXML, Score<T>>(const Score<T>& data) {       \
        return data.dumps<DataFormat::MusicXML>();                              \
    }

REPEAT_ON(INSTANTIATE_GLOBAL_FUNC, Tick, Quarter, Second)
#undef INSTANTIATE_GLOBAL_FUNC

}   // namespace symusic