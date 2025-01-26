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
#include "symusic/utils.h"
#include "symusic/conversion.h"
#include "fmt/format.h"

namespace symusic {

namespace details {

inline size_t expected_note_num(const minimx::Part& part) {
    size_t num = 0;
    for (const auto& measure : part.measures) { num += measure.elements.size(); }
    return static_cast<size_t>(static_cast<float>(num) * 1.1);
}

inline double quarter_note_num(const minimx::Time& time) {
    return static_cast<double>(time.beats * 4) / time.beatType;
}

inline double quarter_note_num(const int beats, const int beatType) {
    return static_cast<double>(beats * 4) / beatType;
}

vec<Note<Quarter>> parse_notes(const minimx::Part& part) {
    using unit = Quarter::unit;

    vec<Note<Quarter>> notes{};
    notes.reserve(expected_note_num(part));

    double cur_time = 0;

    std::array<Note<Quarter>*, 128> tied_notes = {nullptr};

    for (const auto& measure : part.measures) {
        const double begin     = cur_time;
        const double division  = measure.attributes.divisions;
        const auto&  transpose = measure.attributes.transpose;
        for (const auto& element : measure.elements) {
            // Calculate the duration of the element
            const double duration = static_cast<double>(element.duration) / division;
            switch (element.type) {
            case minimx::Note: {
                if (element.actualNotes < 1) {
                    throw std::runtime_error(
                        fmt::format(
                            "symusic: Invalid actual-notes value ({}) in note.", element.actualNotes
                        )
                    );
                }
                const int pitch = element.pitch.midi_pitch(transpose);
                if (pitch < 0 || pitch > 127) {
                    throw std::runtime_error(
                        fmt::format("symusic: Invalid pitch value ({}) in note.", pitch)
                    );
                }

                // Check if the note is Tie Stop
                if (element.tie == minimx::Stop) {
                    if (tied_notes[pitch] == nullptr) {
                        throw std::runtime_error(
                            fmt::format(
                                "symusic: Tied note does not exist at pitch {} when parsing "
                                "musicxml.",
                                pitch
                            )
                        );
                    }
                    tied_notes[pitch]->duration
                        = static_cast<f32>(cur_time - tied_notes[pitch]->time + duration);
                    tied_notes[pitch] = nullptr;
                } else if (tied_notes[pitch] == nullptr && !element.isRest) {
                    // if tied_notes[pitch] is not nullptr,
                    // then this note is not in the middle of a long note
                    // And the rest note should also be ignored
                    if (element.actualNotes == 1) [[likely]] {
                        notes.emplace_back(cur_time, duration, pitch, 100);
                    } else {
                        // Deal with triplets or other time modifications
                        f64 duration_per_note = duration / element.actualNotes;
                        f64 tmp_time          = cur_time;
                        for (int i = 0; i < element.actualNotes;
                             ++i, tmp_time += duration_per_note) {
                            notes.emplace_back(tmp_time, duration_per_note, pitch, 100);
                        }
                    }
                }

                if (!element.isChordTone) { cur_time += duration; }

                // Tie Start
                if (element.tie == minimx::Start) {
                    if (tied_notes[pitch] != nullptr) {
                        throw std::runtime_error(
                            fmt::format(
                                "symusic: Tied note already exists at pitch {} when parsing "
                                "musicxml.",
                                pitch
                            )
                        );
                    }
                    tied_notes[pitch] = &notes.back();
                }
                break;
            }
            case minimx::Backup: {
                cur_time -= duration;
                break;
            }
            case minimx::Forward: {
                cur_time += duration;
                break;
            }
            }
        }
        cur_time = begin + quarter_note_num(measure.attributes.time);
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

    int pre_beats     = 0;
    int pre_beat_type = 0;
    double cur_time = 0;
    vec<TimeSignature<Quarter>> time_signatures{};

    for (auto i = 0; i < measure_num; ++i) {
        int  cur_beats     = 0;
        int  cur_beat_type = 0;
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
            throw std::runtime_error("symusic: Time signature not found in one part of the MusicXML Score.");
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

ScoreNative<Quarter> parse_musicxml_native(const pugi::xml_document& doc) {
    // Parsing the MusicXML file
    const minimx::MXScore mx_score{doc};
    // Create an empty score
    ScoreNative<Quarter> score{};

    if (mx_score.parts.empty()) { return score; }

    score.time_signatures = std::move(build_time_signature(mx_score));

    score.tracks.reserve(mx_score.parts.size());
    for (const auto& part : mx_score.parts) {
        TrackNative<Quarter> track{};
        track.name = part.name;
        // Reserve space for notes
        track.notes = std::move(parse_notes(part));
    }
    return score;
}

}   // namespace details

#define INSTANTIATE_XML_DUMP(__COUNT, T)                                   \
    template<>                                                             \
    vec<u8> dumps<DataFormat::MusicXML>(const Score<T>& data) {            \
        throw std::runtime_error("Dumping MusicXML is not supported yet"); \
        return {};                                                         \
    }

REPEAT_ON(INSTANTIATE_XML_DUMP, Tick, Quarter, Second)
#undef INSTANTIATE_XML_DUMP


}   // namespace symusic