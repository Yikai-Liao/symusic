#include "miniabc/miniabc.h"

#include "symusic/conversion.h"
#include "symusic/event.h"
#include "symusic/score.h"
#include "MetaMacro.h"

#include <algorithm>
#include <numeric>
#include <span>
#include <string_view>
#include <sstream>

namespace symusic {

namespace {

using miniabc::Document;
using miniabc::Fraction;

constexpr Fraction normalize_fraction(const Fraction& frac) {
    if (frac.den == 0) { return Fraction{0, 1}; }
    auto num = frac.num;
    auto den = frac.den;
    if (den < 0) {
        num = -num;
        den = -den;
    }
    const auto gcd = std::gcd(std::llabs(num), std::llabs(den));
    return Fraction{num / gcd, den / gcd};
}

Fraction ticks_to_fraction(const i64 ticks, const i32 tpq) {
    const i64 ticks_per_whole = static_cast<i64>(tpq) * 4;
    const i64 g               = std::gcd(std::llabs(ticks), ticks_per_whole);
    return Fraction{ticks / g, ticks_per_whole / g};
}

i32 fraction_to_ticks(const Fraction& fraction, const i32 tpq) {
    const i64 ticks_per_whole = static_cast<i64>(tpq) * 4;
    const i64 scaled          = (fraction.num * ticks_per_whole) / fraction.den;
    return static_cast<i32>(scaled);
}

Fraction infer_unit_note_length(const Score<Tick>& score) {
    i64 gcd_ticks = 0;
    if (!score.tracks->empty()) {
        for (const auto& track : *score.tracks) {
            if (track->notes->empty()) { continue; }
            for (const auto& note : *track->notes) {
                const auto duration = static_cast<i64>(note.duration);
                gcd_ticks = gcd_ticks == 0 ? duration : std::gcd(gcd_ticks, duration);
            }
        }
    }
    if (gcd_ticks <= 0) { return Fraction{1, 8}; }
    const i64 ticks_per_whole = static_cast<i64>(score.ticks_per_quarter) * 4;
    return normalize_fraction(Fraction{gcd_ticks, ticks_per_whole});
}

std::vector<std::string> tokenize_lyrics(const std::vector<std::string>& lines) {
    std::vector<std::string> tokens;
    for (const auto& line : lines) {
        std::string current;
        for (char ch : line) {
            if (std::isspace(static_cast<unsigned char>(ch))) {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
            } else {
                current.push_back(ch);
            }
        }
        if (!current.empty()) { tokens.push_back(current); }
    }
    return tokens;
}

template<typename T>
void attach_lyrics(const miniabc::Voice& voice, TrackNative<T>& track) {
    if (voice.lyrics.empty() || track.notes.empty()) { return; }
    const auto tokens = tokenize_lyrics(voice.lyrics);
    std::size_t note_index = 0;
    for (const auto& token : tokens) {
        if (token.empty() || token == "|") { continue; }
        if (token == "_") { continue; }
        if (note_index >= track.notes.size()) { break; }
        track.lyrics.emplace_back(track.notes[note_index].time, token);
        ++note_index;
    }
}

ScoreNative<Tick> document_to_native(const Document& doc) {
    ScoreNative<Tick> native(doc.header.ticks_per_quarter);
    if (doc.header.meter.numerator > 0 && doc.header.meter.denominator > 0) {
        native.time_signatures.emplace_back(0, doc.header.meter.numerator, doc.header.meter.denominator);
    }
    native.tempos.emplace_back(
        0, Tempo<Tick>::qpm2mspq(doc.header.tempo.qpm)
    );
    native.key_signatures.emplace_back(
        0, static_cast<i8>(doc.header.key.accidentals), doc.header.key.is_minor ? 1 : 0
    );
    native.markers.clear();
    for (const auto& directive : doc.directives) {
        if (directive.empty()) { continue; }
        native.markers.emplace_back(0, directive);
    }

    if (doc.voices.empty()) {
        native.tracks.emplace_back(TrackNative<Tick>{"Voice 1", 0, false});
        return native;
    }

    for (const auto& voice : doc.voices) {
        TrackNative<Tick> track{voice.name.empty() ? "Voice" : voice.name, 0, false};
        i32               cursor = 0;
        for (const auto& element : voice.elements) {
            if (const auto* note = std::get_if<miniabc::NoteEvent>(&element)) {
                const auto duration = fraction_to_ticks(note->length, doc.header.ticks_per_quarter);
                track.notes.emplace_back(
                    cursor,
                    duration,
                    static_cast<i8>(std::clamp(note->midi_pitch, 0, 127)),
                    static_cast<i8>(std::clamp(note->velocity, 0, 127))
                );
                cursor += duration;
            } else if (const auto* chord = std::get_if<miniabc::ChordEvent>(&element)) {
                for (const auto& chord_note : chord->notes) {
                    const auto duration = fraction_to_ticks(chord_note.length, doc.header.ticks_per_quarter);
                    track.notes.emplace_back(
                        cursor,
                        duration,
                        static_cast<i8>(std::clamp(chord_note.midi_pitch, 0, 127)),
                        static_cast<i8>(std::clamp(chord_note.velocity, 0, 127))
                    );
                }
                if (!chord->notes.empty()) {
                    cursor += fraction_to_ticks(chord->notes.front().length, doc.header.ticks_per_quarter);
                }
            } else if (const auto* rest = std::get_if<miniabc::RestEvent>(&element)) {
                cursor += fraction_to_ticks(rest->length, doc.header.ticks_per_quarter);
            }
        }
        attach_lyrics(voice, track);
        native.tracks.push_back(std::move(track));
    }
    return native;
}

miniabc::Meter score_meter(const Score<Tick>& score) {
    if (!score.time_signatures->empty()) {
        const auto& ts = score.time_signatures->at(0);
        return miniabc::Meter{ts.numerator, ts.denominator};
    }
    return miniabc::Meter{4, 4};
}

miniabc::TempoMark score_tempo(const Score<Tick>& score) {
    miniabc::TempoMark tempo{};
    if (!score.tempos->empty()) {
        const auto& tempo_event = score.tempos->at(0);
        tempo.qpm = tempo_event.qpm();
    }
    return tempo;
}

miniabc::KeySignature score_key(const Score<Tick>& score) {
    miniabc::KeySignature key{};
    if (!score.key_signatures->empty()) {
        const auto& key_event = score.key_signatures->at(0);
        key.accidentals       = key_event.key;
        key.is_minor          = key_event.tonality != 0;
    }
    return key;
}

miniabc::Voice track_to_voice(const Track<Tick>& track, const i32 tpq) {
    miniabc::Voice voice;
    voice.name = track.name;
    if (!track.notes->empty()) {
        auto notes = track.notes->collect();
        std::sort(notes.begin(), notes.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.time == rhs.time) { return lhs.duration < rhs.duration; }
            return lhs.time < rhs.time;
        });

        i32 cursor = 0;
        std::size_t idx = 0;
        while (idx < notes.size()) {
            const auto start = notes[idx].time;
            if (start > cursor) {
                const auto gap_ticks = start - cursor;
                voice.elements.emplace_back(miniabc::RestEvent{ticks_to_fraction(gap_ticks, tpq)});
                cursor = start;
            }
            std::size_t j = idx + 1;
            while (j < notes.size() && notes[j].time == start) { ++j; }
            const std::size_t group_size = j - idx;
            bool can_chord = group_size > 1;
            const auto base_duration = notes[idx].duration;
            if (can_chord) {
                for (std::size_t k = idx; k < j; ++k) {
                    if (notes[k].duration != base_duration) {
                        can_chord = false;
                        break;
                    }
                }
            }
            if (can_chord) {
                miniabc::ChordEvent chord;
                chord.notes.reserve(group_size);
                for (std::size_t k = idx; k < j; ++k) {
                    chord.notes.push_back(miniabc::NoteEvent{
                        ticks_to_fraction(notes[k].duration, tpq),
                        notes[k].pitch,
                        notes[k].velocity,
                        false
                    });
                }
                voice.elements.emplace_back(std::move(chord));
                cursor = start + base_duration;
                idx = j;
                continue;
            }

            const auto duration = ticks_to_fraction(notes[idx].duration, tpq);
            voice.elements.emplace_back(miniabc::NoteEvent{
                duration,
                notes[idx].pitch,
                notes[idx].velocity,
                false
            });
            cursor = std::max(cursor, notes[idx].time + notes[idx].duration);
            ++idx;
        }
    }
    if (track.lyrics && !track.lyrics->empty()) {
        auto lyrics = track.lyrics->collect();
        std::sort(lyrics.begin(), lyrics.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.time < rhs.time;
        });
        std::ostringstream line;
        for (std::size_t i = 0; i < lyrics.size(); ++i) {
            if (i) line << ' ';
            line << lyrics[i].text;
        }
        voice.lyrics.push_back(line.str());
    }
    return voice;
}

Document score_to_document(const Score<Tick>& score) {
    Document doc;
    doc.header.reference_number  = 1;
    doc.header.title             = score.tracks->empty() ? "symusic" : score.tracks->at(0)->name;
    doc.header.meter             = score_meter(score);
    doc.header.unit_note_length  = infer_unit_note_length(score);
    doc.header.tempo             = score_tempo(score);
    doc.header.key               = score_key(score);
    doc.header.ticks_per_quarter = score.ticks_per_quarter;
    if (!score.markers->empty()) {
        for (const auto& marker : *score.markers) {
            if (!marker.text.empty() && marker.text.rfind("%%", 0) == 0) {
                doc.directives.push_back(marker.text);
            }
        }
    }

    if (!score.tracks->empty()) {
        for (const auto& shared_track : *score.tracks) {
        doc.voices.push_back(track_to_voice(*shared_track, score.ticks_per_quarter));
        }
    } else {
        doc.voices.push_back(miniabc::Voice{});
    }

    return doc;
}

}  // namespace

template<>
template<>
Score<Tick> Score<Tick>::parse<DataFormat::ABC>(std::span<const u8> bytes) {
    const auto view = std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    miniabc::ParseOptions options;
    options.ticks_per_quarter = 960;
    auto document             = miniabc::parse_document(view, options);
    return to_shared(document_to_native(document));
}

template<>
template<>
Score<Quarter> Score<Quarter>::parse<DataFormat::ABC>(std::span<const u8> bytes) {
    return convert<Quarter>(Score<Tick>::parse<DataFormat::ABC>(bytes));
}

template<>
template<>
Score<Second> Score<Second>::parse<DataFormat::ABC>(std::span<const u8> bytes) {
    return convert<Second>(Score<Tick>::parse<DataFormat::ABC>(bytes));
}

template<>
template<>
vec<u8> Score<Tick>::dumps<DataFormat::ABC>() const {
    const auto document = score_to_document(*this);
    const auto text     = miniabc::render_document(document);
    return vec<u8>(text.begin(), text.end());
}

template<>
template<>
vec<u8> Score<Quarter>::dumps<DataFormat::ABC>() const {
    return convert<Tick>(*this).dumps<DataFormat::ABC>();
}

template<>
template<>
vec<u8> Score<Second>::dumps<DataFormat::ABC>() const {
    return convert<Tick>(*this).dumps<DataFormat::ABC>();
}

#define INSTANTIATE_ABC(__COUNT, T)                                                       \
    template<>                                                                            \
    Score<T> parse<DataFormat::ABC, Score<T>>(std::span<const u8> bytes) {                \
        return Score<T>::parse<DataFormat::ABC>(bytes);                                   \
    }                                                                                     \
    template<>                                                                            \
    vec<u8> dumps<DataFormat::ABC, Score<T>>(const Score<T>& data) {                      \
        return data.dumps<DataFormat::ABC>();                                             \
    }

REPEAT_ON(INSTANTIATE_ABC, Tick, Quarter, Second)
#undef INSTANTIATE_ABC

}  // namespace symusic
