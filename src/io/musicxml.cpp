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
#include "fmt/ranges.h"
#include "unordered_dense.h"

namespace symusic {

namespace details {

struct TieHook {
    ankerl::unordered_dense::map<std::pair<i8, i32>, size_t> tie_starts{};
    // std::unordered_map<std::pair<i8, i32>, size_t> tie_starts;
    vec<Note<Quarter>>& notes;

    explicit TieHook(vec<Note<Quarter>>& notes) : tie_starts(notes.capacity() / 2), notes(notes) {}

    template<typename T>
    static i32 quantize_time(const T time) {
        return static_cast<i32>(std::round(time * 1000));
    }

    [[nodiscard]] std::string to_string() const {
        vec<std::pair<i8, i32>> keys;
        for (const auto& [key, _] : tie_starts) { keys.push_back(key); }
        return fmt::format("TieHook: {}", keys);
    }

    void add_tie_start(const size_t index, const i8 pitch, const f64 time) {
        // check not exist
        const auto& note = notes.at(index);
        if (const auto [_, success]
            = tie_starts.try_emplace({note.pitch, quantize_time(note.end())}, index);
            !success) {
            throw std::runtime_error(
                fmt::format(
                    "symusic: Tied note already exists at pitch {} and time {:.4f} when parsing "
                    "musicxml.",
                    note.pitch,
                    note.time
                )
            );
        }
    }

    size_t add_tie_stop(
        const i8 pitch, const f64 time, const f64 duration, const minimx::MeasureElement& element
    ) {
        // try to find the tied note
        const auto iter = tie_starts.find({pitch, quantize_time(time)});
        if (iter == tie_starts.end()) {
            throw std::runtime_error(
                fmt::format(
                    "symusic: Tied note does not exist at pitch ( {}/{}/{} -- {} ) at time {:.4f} "
                    "when parsing musicxml. {}",
                    element.pitch.octave,
                    element.pitch.alter,
                    element.pitch.step,
                    pitch,
                    time,
                    this->to_string()
                )
            );
        }

        const auto index = iter->second;
        notes[index].duration += static_cast<f32>(duration);
        tie_starts.erase(iter);
        return index;
    }

    void add_tie_stop_start(
        const i8 pitch, const f64 time, const f64 duration, const minimx::MeasureElement& element
    ) {
        const auto index = add_tie_stop(pitch, time, duration, element);
        add_tie_start(index, pitch, notes[index].time);
    }

    [[nodiscard]] bool empty() const { return tie_starts.empty(); }
};

inline size_t expected_note_num(const minimx::Part& part) {
    size_t num = 0;
    for (const auto& measure : part.measures) { num += measure.elements.size(); }
    return static_cast<size_t>(static_cast<f32>(num) * 1.1);
}

inline f64 quarter_note_num(const minimx::Time& time) {
    return static_cast<f64>(time.beats * 4) / time.beatType;
}

inline f64 quarter_note_num(const i32 beats, const i32 beatType) {
    return static_cast<f64>(beats * 4) / beatType;
}

inline i8 parse_pitch(const minimx::Pitch& pitch, const minimx::Transpose& transpose) {
    const i32 pitch_value = transpose.empty() ? pitch.midi_pitch() : pitch.midi_pitch(transpose);
    if (pitch_value < 0 | pitch_value > 127) {
        throw std::runtime_error(
            fmt::format(
                "symusic: Parsing invalid pitch {} in musicxml. It should be {} with "
                "out transpose (diatonic={}, chromatic={}, octave_change={}, "
                "double={}).",
                pitch_value,
                pitch.midi_pitch(),
                transpose.diatonic,
                transpose.chromatic,
                transpose.octave_change,
                transpose.double_
            )
        );
    }
    return static_cast<i8>(pitch_value);
}

inline void add_note(
    const minimx::MeasureElement& element,
    vec<Note<Quarter>>&           notes,
    const f64                     cur_time,
    const i8                      pitch,
    const i8                      velocity,
    const f64                     duration
) {
    // check rest note
    if (element.isRest) { return; }
    // check time modification
    if (element.actualNotes == 1) [[likely]] {
        notes.emplace_back(cur_time, duration, pitch, velocity);
    } else {
        // Deal with triplets or other time modifications
        f64 duration_per_note = duration / element.actualNotes;
        f64 tmp_time          = cur_time;
        for (i32 i = 0; i < element.actualNotes; ++i, tmp_time += duration_per_note) {
            notes.emplace_back(tmp_time, duration_per_note, pitch, velocity);
        }
    }
}

void check_tied_note_existence(const std::array<Note<Quarter>*, 128>& tied_notes, const i8 pitch) {
    if (tied_notes[pitch] == nullptr) {
        throw std::runtime_error(
            fmt::format(
                "symusic: Tied note does not exist at pitch {} when parsing "
                "musicxml.",
                pitch
            )
        );
    }
}

std::string tie2str(const minimx::Tie tie) {
    switch (tie) {
    case minimx::Tie::NotTied: return "NotTied";
    case minimx::Tie::Start: return "Start";
    case minimx::Tie::Stop: return "Stop";
    case minimx::Tie::StopStart: return "StopStart";
    case minimx::Tie::NotDefined: return "NotDefined";
    }
    return "Unknown";
}

inline void proc_chord_duration(
    const minimx::MeasureElement& element,
    f64&                          cur_time,
    f64&                          chord_duration,
    bool&                         is_prev_chord,
    const f64                     duration
) {
    // Check if Previous note is a chord note and the current note is not a chord note
    if (element.isChordTone) {
        if (chord_duration <= 0) {
            throw std::runtime_error(
                fmt::format("symusic: Invalid chord duration ({}) in musicxml.", chord_duration)
            );
        }
        if (!is_prev_chord) {
            // move the time to the beginning of previous note (without chord label)
            cur_time -= chord_duration;
        }
        is_prev_chord = true;
    } else {
        // Current note is not a chord note
        if (is_prev_chord) {
            // Previous note is a chord note
            // Move the time to the end of the chord note
            cur_time += chord_duration;
            if (chord_duration <= 0) {
                throw std::runtime_error(
                    fmt::format("symusic: Invalid chord duration ({}) in musicxml.", chord_duration)
                );
            }
        }
        // Store duration if it is not a chord note
        // This is preserved for the next note
        chord_duration = duration;
        is_prev_chord  = false;
    }
}

inline void parse_note(
    const minimx::MeasureElement& element,
    vec<Note<Quarter>>&           notes,
    TieHook&                      tie_hook,
    f64&                          cur_time,
    f64&                          chord_duration,
    bool&                         is_prev_chord,
    const i8                      velocity,
    const f64                     duration,
    const minimx::Transpose&      transpose
) {
    if (element.actualNotes < 1) {
        throw std::runtime_error(
            fmt::format("symusic: Invalid actual-notes value ({}) in note.", element.actualNotes)
        );
    }

    proc_chord_duration(element, cur_time, chord_duration, is_prev_chord, duration);

    // Check if it is a rest note
    if (element.isRest) {
        cur_time += element.isChordTone ? 0 : duration;
        return;
    }

    const i8 pitch = parse_pitch(element.pitch, transpose);

    switch (element.tie) {
    case minimx::Tie::NotTied: {
        add_note(element, notes, cur_time, pitch, velocity, duration);
        break;
    }
    case minimx::Tie::Start: {
        add_note(element, notes, cur_time, pitch, velocity, duration);
        tie_hook.add_tie_start(notes.size() - 1, pitch, cur_time);
        break;
    }
    case minimx::Tie::Stop: {
        tie_hook.add_tie_stop(pitch, cur_time, duration, element);
        break;
    }
    case minimx::Tie::StopStart: {
        tie_hook.add_tie_stop_start(pitch, cur_time, duration, element);
        break;
    }
    case minimx::Tie::NotDefined: {
        throw std::runtime_error("symusic: Tie::NotDefined is not expected in a note.");
    }
    }
    // Move the time to the end of the note if it is not a chord note
    cur_time += element.isChordTone ? 0 : duration;
}

template<typename T>
void update_positive(T& old_value, const T& new_value) {
    old_value = new_value > 0 ? new_value : old_value;
}



vec<Note<Quarter>> parse_notes(const minimx::Part& part) {
    vec<Note<Quarter>> notes{};
    notes.reserve(expected_note_num(part));

    f64 cur_time  = 0;
    f64 divisions = 0;
    u8  beats     = 0;
    u8  beat_type = 0;

    TieHook tie_hook{notes};
    auto    velocity = static_cast<i8>(127. * 0.9);

    for (const auto& measure : part.measures) {
        f64  chord_duration = 0;
        bool is_prev_chord  = false;

        update_positive(divisions, static_cast<f64>(measure.attributes.divisions));
        if (divisions == 0) { throw std::runtime_error("symusic: Division is zero in musicxml."); }

        const f64   begin     = cur_time;
        const auto& transpose = measure.attributes.transpose;

        if (measure.sound.dynamics > 0) {
            velocity = static_cast<i8>(1.27 * measure.sound.dynamics);
        }

        for (const auto& element : measure.elements) {
            // Calculate the duration of the element
            const f64 duration = static_cast<f64>(element.duration) / divisions;
            switch (element.type) {
            case minimx::MeasureElementType::Note: {
                parse_note(
                    element,
                    notes,
                    tie_hook,
                    cur_time,
                    chord_duration,
                    is_prev_chord,
                    velocity,
                    duration,
                    transpose
                );
                break;
            }
            case minimx::MeasureElementType::Backup: {
                cur_time -= duration;
                break;
            }
            case minimx::MeasureElementType::Forward: {
                cur_time += duration;
                break;
            }
            }
        }
        update_positive(beats, measure.attributes.time.beats);
        update_positive(beat_type, measure.attributes.time.beatType);
        cur_time = begin + quarter_note_num(beats, beat_type);
    }

    // Check if all tied notes are closed
    if (!tie_hook.empty()) {
        vec<Note<Quarter>> unclosed_notes{};
        for (const auto& [key, index] : tie_hook.tie_starts) {
            unclosed_notes.emplace_back(notes[index]);
        }
        throw std::runtime_error(
            fmt::format(
                "symusic: Tied notes are not closed in musicxml. {} unclosed notes: {}",
                unclosed_notes.size(),
                unclosed_notes
            )
        );
    }
    return notes;
}

vec<TimeSignature<Quarter>> build_time_signature(const minimx::MXScore& mx_score) {
    if (mx_score.parts.empty()) {
        throw std::runtime_error("symusic: Cannot check the time signature of an empty score.");
    }
    size_t measure_num = 0;
    for (const auto& part : mx_score.parts) {
        measure_num = std::max(measure_num, part.measures.size());
    }

    i32                         pre_beats     = 0;
    i32                         pre_beat_type = 0;
    f64                         cur_time      = 0;
    vec<TimeSignature<Quarter>> time_signatures{};

    for (auto i = 0; i < measure_num; ++i) {
        i32  cur_beats     = 0;
        i32  cur_beat_type = 0;
        bool first         = true;
        for (const auto& part : mx_score.parts) {
            if (part.measures.size() <= i) { continue; }
            const auto& measure = part.measures[i];
            if (first) {
                cur_beats     = measure.attributes.time.beats;
                cur_beat_type = measure.attributes.time.beatType;
                first         = false;
            } else {
                if (cur_beats != measure.attributes.time.beats
                    || cur_beat_type != measure.attributes.time.beatType) {
                    throw std::runtime_error(
                        "symusic: Time signature mismatch in the MusicXML Score."
                    );
                }
            }
        }
        if (first) {
            throw std::runtime_error(
                "symusic: Time signature not found in one part of the MusicXML Score."
            );
        } else if (cur_beats == 0 || cur_beat_type == 0) {
            continue;
        }
        if (cur_beats != pre_beats || cur_beat_type != pre_beat_type) {
            time_signatures.emplace_back(cur_time, cur_beats, cur_beat_type);
            pre_beats     = cur_beats;
            pre_beat_type = cur_beat_type;
        }
        cur_time += quarter_note_num(cur_beats, cur_beat_type);
    }
    return time_signatures;
}

void add_volume_control(TrackNative<Quarter>& track, const minimx::Part& part) {
    if (part.midiInstrument.volume > 0) {
        track.controls.emplace_back(0, 7, static_cast<u8>(1.27 * part.midiInstrument.volume));
    }
}

ScoreNative<Quarter> parse_musicxml_native(const pugi::xml_document& doc) {
    // Parsing the MusicXML file
    const minimx::MXScore mx_score{doc};
    // Create an empty score
    ScoreNative<Quarter> score{960};

    if (mx_score.parts.empty()) { return score; }

    score.time_signatures = std::move(build_time_signature(mx_score));

    score.tracks.reserve(mx_score.parts.size());
    for (const auto& part : mx_score.parts) {
        TrackNative<Quarter> track{};
        track.name = part.name;
        // TODO: Add support for instrument changes
        // Program in MusicXML is 1-based
        track.program = std::max(0, part.midiInstrument.program - 1);
        add_volume_control(track, part);
        // Reserve space for notes
        track.notes = std::move(parse_notes(part));
        pdqsort_detail::insertion_sort(
            track.notes.begin(),
            track.notes.end(),
            [](const auto& a, const auto& b) { return (a.time) < (b.time); }
        );
        score.tracks.emplace_back(std::move(track));
    }
    return score;
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