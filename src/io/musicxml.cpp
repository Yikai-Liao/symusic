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

size_t expected_note_num(const minimx::Part& part) {
    size_t num = 0;
    for (const auto& measure : part.measures) { num += measure.elements.size(); }
    return static_cast<size_t>(static_cast<float>(num) * 1.1);
}

double quarter_note_num(const minimx::Time& time) {
    return static_cast<double>(time.beats * 4) / time.beatType;
}

vec<Note<Quarter>> parse_notes(const minimx::Part& part) {
    using unit = Quarter::unit;

    vec<Note<Quarter>> notes{};
    notes.reserve(expected_note_num(part));

    double cur_time = 0;
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
                    throw std::runtime_error(fmt::format(
                        "symusic: Invalid actual-notes value ({}) in note.",
                        element.actualNotes
                    ));
                }
                const int pitch = element.pitch.midi_pitch(transpose);
                if (pitch < 0 || pitch > 127) {
                    throw std::runtime_error(fmt::format(
                        "symusic: Invalid pitch value ({}) in note.",
                        pitch
                    ));
                }

                if (element.actualNotes == 1) [[likely]] {
                    notes.emplace_back(cur_time, duration, pitch, 100);
                } else {
                    // Deal with triplets or other time modifications
                    f64 duration_per_note = duration / element.actualNotes;
                    f64 tmp_time = cur_time;
                    for (int i = 0; i < element.actualNotes; ++i, tmp_time += duration_per_note) {
                        notes.emplace_back(tmp_time, duration_per_note, pitch, 100);
                    }
                }
                if (!element.isChordTone) {
                    cur_time += duration;
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

ScoreNative<Quarter> parse_musicxml_native(const pugi::xml_document& doc) {
    // Parsing the MusicXML file
    const minimx::MXScore mx_score{doc};
    // Create a empty score
    ScoreNative<Quarter> score{};

    score.tracks.reserve(mx_score.parts.size());
    for (const auto& part : mx_score.parts) {
        TrackNative<Quarter> track{};
        track.name = part.name;
        // Reserve space for notes
        track.notes.reserve(expected_note_num(part));
    }
    return {};
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