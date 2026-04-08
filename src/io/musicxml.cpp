//
// MusicXML support built on top of mx. The implementation stays entirely in the C++ core layer:
// no nanobind or Python coupling appears in this file.
//

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "ezxml/XFactory.h"
#include "mx/api/DirectionData.h"
#include "mx/api/DurationData.h"
#include "mx/api/KeyData.h"
#include "mx/api/MeasureData.h"
#include "mx/api/NoteData.h"
#include "mx/api/PartData.h"
#include "mx/api/PitchData.h"
#include "mx/api/ScoreData.h"
#include "mx/api/StaffData.h"
#include "mx/api/TempoData.h"
#include "mx/core/Document.h"
#include "mx/core/XsString.h"
#include "mx/core/elements/Lyric.h"
#include "mx/core/elements/LyricTextChoice.h"
#include "mx/core/elements/Note.h"
#include "mx/core/elements/Syllabic.h"
#include "mx/core/elements/SyllabicTextGroup.h"
#include "mx/core/elements/Text.h"
#include "mx/impl/Cursor.h"
#include "mx/impl/MeasureCursor.h"
#include "mx/impl/NoteFunctions.h"
#include "mx/impl/NoteReader.h"
#include "mx/impl/ScoreReader.h"
#include "mx/impl/ScoreWriter.h"

#include "symusic/conversion.h"

namespace symusic {

namespace {

constexpr int kDefaultMusicXmlTpq = 480;
constexpr int kDefaultVelocity = 64;
constexpr int kDrumMusicXmlChannel = 10;
constexpr int kMaxDotCount = 3;

using mx::api::DurationData;
using mx::api::DurationName;
using mx::api::MeasureData;
using mx::api::NoteData;
using mx::api::PartData;
using mx::api::PitchData;
using mx::api::ScoreData;
using mx::api::Step;
using mx::core::DocumentChoice;

template<typename T>
using shared_track = shared<Track<T>>;

struct ParsedMusicXml {
    ScoreData                    score_data;
    mx::core::ScorePartwisePtr   score_partwise;
};

struct AbsoluteTempo {
    int tick_time;
    i32 mspq;
};

struct AbsoluteKeySignature {
    int tick_time;
    i8  key;
    i8  tonality;
};

struct MeasureLayout {
    int                  start_tick = 0;
    int                  length = 0;
    mx::api::MeasureData* measure = nullptr;
};

struct TrackMeasureNote {
    int            voice = 0;
    int            chord_group = -1;
    int            abs_time = 0;
    int            local_time = 0;
    int            duration = 0;
    int            pitch = 0;
    int            velocity = 0;
    bool           tie_start = false;
    bool           tie_stop = false;
    bool           needs_type_patch = false;
    DurationName   duration_name = DurationName::quarter;
    int            duration_dots = 0;
};

struct OutputPatchInfo {
    int          measure_index = 0;
    int          local_time = 0;
    int          duration = 0;
    int          pitch = 0;
    int          voice = 0;
    int          velocity = 0;
    bool         needs_type_patch = false;
};

struct OutputPatchKey {
    int measure_index = 0;
    int local_time = 0;
    int duration = 0;
    int pitch = 0;
    int voice = 0;

    auto operator<=>(const OutputPatchKey&) const = default;
};

struct TraversedNote {
    NoteData        note;
    mx::core::Note* core_note = nullptr;
    int             measure_index = 0;
    int             staff_index = 0;
    int             absolute_time = 0;
};

struct TieKey {
    int staff_index = 0;
    int voice = 0;
    int pitch = 0;

    auto operator<=>(const TieKey&) const = default;
};

struct ActiveTie {
    int       start = 0;
    int       duration = 0;
    i8        pitch = 0;
    i8        velocity = 0;
};

struct ChordCluster {
    int                start = 0;
    int                end = 0;
    std::vector<size_t> note_indices;
    int                voice = 0;
};

struct SegmentInfo {
    int          measure_index = 0;
    int          voice = 0;
    int          chord_group = -1;
    int          abs_start = 0;
    int          local_start = 0;
    int          duration = 0;
    int          pitch = 0;
    int          velocity = 0;
    bool         tie_start = false;
    bool         tie_stop = false;
    bool         needs_type_patch = false;
    DurationName duration_name = DurationName::quarter;
    int          duration_dots = 0;
};

[[nodiscard]] std::string_view to_string_view(const std::span<const u8> bytes) {
    return {
        reinterpret_cast<const char*>(bytes.data()),
        bytes.size()
    };
}

[[nodiscard]] int sanitize_musicxml_tpq(const int tpq) {
    return tpq > 0 ? tpq : kDefaultMusicXmlTpq;
}

[[nodiscard]] ParsedMusicXml parse_musicxml_document(const std::span<const u8> bytes) {
    auto xdoc = ezxml::XFactory::makeXDoc();
    std::istringstream stream(std::string(to_string_view(bytes)));
    xdoc->loadStream(stream);

    auto document = mx::core::makeDocument();
    std::ostringstream messages;
    const bool ok = document->fromXDoc(messages, *xdoc);
    if (!ok) {
        throw std::runtime_error(
            fmt::format("Failed to parse MusicXML document: {}", messages.str())
        );
    }

    if (document->getChoice() == DocumentChoice::timewise) { document->convertContents(); }
    auto score_partwise = document->getScorePartwise();
    if (!score_partwise) {
        throw std::runtime_error("Failed to construct a partwise MusicXML score");
    }

    mx::impl::ScoreReader reader(*score_partwise);
    auto                  score_data = reader.getScoreData();
    score_data.ticksPerQuarter = sanitize_musicxml_tpq(score_data.ticksPerQuarter);
    return {std::move(score_data), std::move(score_partwise)};
}

[[nodiscard]] int midi_pitch_from_musicxml_pitch(const PitchData& pitch_data) {
    constexpr std::array<int, 7> step_to_semitone = {0, 2, 4, 5, 7, 9, 11};
    const auto step_index = static_cast<int>(pitch_data.step);
    if (step_index < 0 || step_index >= static_cast<int>(step_to_semitone.size())) {
        throw std::runtime_error("Unsupported MusicXML pitch step");
    }
    return (pitch_data.octave + 1) * 12 + step_to_semitone[step_index] + pitch_data.alter;
}

[[nodiscard]] PitchData pitch_data_from_midi_pitch(const int midi_pitch) {
    if (midi_pitch < 0 || midi_pitch > 127) {
        throw std::out_of_range(fmt::format("MIDI pitch out of range: {}", midi_pitch));
    }

    static constexpr std::array<std::pair<Step, int>, 12> spelling = {
        std::pair{Step::c, 0},
        std::pair{Step::c, 1},
        std::pair{Step::d, 0},
        std::pair{Step::d, 1},
        std::pair{Step::e, 0},
        std::pair{Step::f, 0},
        std::pair{Step::f, 1},
        std::pair{Step::g, 0},
        std::pair{Step::g, 1},
        std::pair{Step::a, 0},
        std::pair{Step::a, 1},
        std::pair{Step::b, 0},
    };

    const int chroma = midi_pitch % 12;
    const int octave = midi_pitch / 12 - 1;
    PitchData  pitch_data{};
    pitch_data.step = spelling[static_cast<size_t>(chroma)].first;
    pitch_data.alter = spelling[static_cast<size_t>(chroma)].second;
    pitch_data.octave = octave;
    pitch_data.cents = 0.0;
    pitch_data.hideAccidental();
    if (pitch_data.alter != 0) { pitch_data.showAccidental(); }
    return pitch_data;
}

[[nodiscard]] i8 clamp_velocity(const int velocity) {
    return static_cast<i8>(std::clamp(velocity, 0, 127));
}

[[nodiscard]] i32 mspq_from_tempo_data(const mx::api::TempoData& tempo_data) {
    const auto to_qpm = [](const DurationName duration_name, const int dots, const int bpm) -> double {
        const auto duration_value = [&](const DurationName name) -> double {
            switch (name) {
                case DurationName::maxima:
                    return 32.0;
                case DurationName::longa:
                    return 16.0;
                case DurationName::breve:
                    return 8.0;
                case DurationName::whole:
                    return 4.0;
                case DurationName::half:
                    return 2.0;
                case DurationName::quarter:
                    return 1.0;
                case DurationName::eighth:
                    return 0.5;
                case DurationName::dur16th:
                    return 0.25;
                case DurationName::dur32nd:
                    return 0.125;
                case DurationName::dur64th:
                    return 0.0625;
                case DurationName::dur128th:
                    return 0.03125;
                case DurationName::dur256th:
                    return 0.015625;
                case DurationName::dur512th:
                    return 0.0078125;
                case DurationName::dur1024th:
                    return 0.00390625;
                case DurationName::unspecified:
                    return 1.0;
            }
            return 1.0;
        };

        double quarter_ratio = duration_value(duration_name);
        double add = quarter_ratio / 2.0;
        for (int i = 0; i < dots; ++i) {
            quarter_ratio += add;
            add /= 2.0;
        }
        return static_cast<double>(bpm) * quarter_ratio;
    };

    switch (tempo_data.tempoType) {
        case mx::api::TempoType::beatsPerMinute:
            if (tempo_data.beatsPerMinute.beatsPerMinute > 0) {
                return Tempo<Tick>::qpm2mspq(
                    to_qpm(
                        tempo_data.beatsPerMinute.durationName,
                        tempo_data.beatsPerMinute.dots < 0 ? 0 : tempo_data.beatsPerMinute.dots,
                        tempo_data.beatsPerMinute.beatsPerMinute
                    )
                );
            }
            break;
        case mx::api::TempoType::tempoText:
            if (tempo_data.tempoText.playbackBeatsPerMinute.beatsPerMinute > 0) {
                return Tempo<Tick>::qpm2mspq(
                    to_qpm(
                        tempo_data.tempoText.playbackBeatsPerMinute.durationName,
                        tempo_data.tempoText.playbackBeatsPerMinute.dots < 0
                            ? 0
                            : tempo_data.tempoText.playbackBeatsPerMinute.dots,
                        tempo_data.tempoText.playbackBeatsPerMinute.beatsPerMinute
                    )
                );
            }
            break;
        case mx::api::TempoType::metricModulation:
        case mx::api::TempoType::unspecified:
            break;
    }
    throw std::runtime_error("Unsupported MusicXML tempo representation");
}

[[nodiscard]] std::optional<i8> tonality_from_mode(const mx::api::KeyMode mode) {
    switch (mode) {
        case mx::api::KeyMode::major:
            return static_cast<i8>(0);
        case mx::api::KeyMode::minor:
            return static_cast<i8>(1);
        case mx::api::KeyMode::unspecified:
        case mx::api::KeyMode::unsupported:
            return std::nullopt;
    }
    return std::nullopt;
}

[[nodiscard]] int nominal_measure_length(
    const mx::api::TimeSignatureData& time_signature, const int tpq
) {
    const int beats = time_signature.beats > 0 ? time_signature.beats : 4;
    const int beat_type = time_signature.beatType > 0 ? time_signature.beatType : 4;
    const long long numerator = static_cast<long long>(tpq) * 4LL * static_cast<long long>(beats);
    return static_cast<int>(std::max(1LL, numerator / beat_type));
}

[[nodiscard]] mx::api::TimeSignatureData make_time_signature_data(
    const int beats, const int beat_type, const bool is_implicit
) {
    auto time_signature = mx::api::TimeSignatureData{};
    time_signature.symbol = mx::api::TimeSignatureSymbol::unspecified;
    time_signature.beats = beats;
    time_signature.beatType = beat_type;
    time_signature.isImplicit = is_implicit;
    time_signature.display = mx::api::Bool::unspecified;
    return time_signature;
}

template<typename Callback>
int traverse_measure_notes(
    const mx::core::PartwiseMeasure& measure,
    mx::impl::MeasureCursor&         cursor,
    const int                        measure_index,
    const int                        absolute_measure_start,
    Callback&&                       callback
) {
    const auto& choices = measure.getMusicDataGroup()->getMusicDataChoiceSet();
    int         max_tick = 0;

    for (auto iter = choices.cbegin(); iter != choices.cend(); ++iter) {
        const auto& choice = **iter;
        switch (choice.getChoice()) {
            case mx::core::MusicDataChoice::Choice::properties: {
                const auto& properties = *choice.getProperties();
                if (properties.getHasDivisions()) {
                    const auto divisions = properties.getDivisions()->getValue().getValue();
                    cursor.ticksPerQuarter = static_cast<int>(std::ceil(divisions - 0.5));
                }
                break;
            }
            case mx::core::MusicDataChoice::Choice::backup: {
                if (!cursor.isBackupInProgress) { ++cursor.voiceIndex; }
                cursor.isBackupInProgress = true;
                const int amount = cursor.convertDurationToGlobalTickScale(*choice.getBackup()->getDuration());
                cursor.tickTimePosition -= amount;
                if (cursor.tickTimePosition < 0) { cursor.tickTimePosition = 0; }
                break;
            }
            case mx::core::MusicDataChoice::Choice::forward: {
                cursor.isBackupInProgress = false;
                const int amount = cursor.convertDurationToGlobalTickScale(*choice.getForward()->getDuration());
                cursor.tickTimePosition += amount;
                max_tick = std::max(max_tick, cursor.tickTimePosition);
                break;
            }
            case mx::core::MusicDataChoice::Choice::note: {
                cursor.isBackupInProgress = false;
                const auto& note = *choice.getNote();

                bool is_next_note_part_of_chord = false;
                auto peek = iter + 1;
                while (peek != choices.cend()
                       && (*peek)->getChoice() != mx::core::MusicDataChoice::Choice::note) {
                    ++peek;
                }
                if (peek != choices.cend()) {
                    mx::impl::NoteReader next_reader(*(*peek)->getNote());
                    is_next_note_part_of_chord = next_reader.getIsChord();
                }

                mx::impl::NoteReader  note_reader(note);
                mx::impl::NoteFunctions note_functions(note, cursor);
                auto                  note_data = note_functions.parseNote();
                note_data.isChord = note_data.isChord || is_next_note_part_of_chord;

                int staff_index = note_reader.getStaffNumber() - 1;
                if (staff_index < 0) { staff_index = 0; }
                cursor.staffIndex = staff_index;

                const int absolute_time = absolute_measure_start + note_data.tickTimePosition;
                callback(note_data, choice.getNote(), measure_index, staff_index, absolute_time);

                if (!note_data.isChord || !is_next_note_part_of_chord) {
                    cursor.tickTimePosition += note_data.durationData.durationTimeTicks;
                }
                max_tick = std::max(
                    max_tick,
                    note_data.tickTimePosition + note_data.durationData.durationTimeTicks
                );
                break;
            }
            case mx::core::MusicDataChoice::Choice::direction:
            case mx::core::MusicDataChoice::Choice::harmony:
            case mx::core::MusicDataChoice::Choice::figuredBass:
            case mx::core::MusicDataChoice::Choice::print:
            case mx::core::MusicDataChoice::Choice::sound:
            case mx::core::MusicDataChoice::Choice::barline:
            case mx::core::MusicDataChoice::Choice::grouping:
            case mx::core::MusicDataChoice::Choice::link:
            case mx::core::MusicDataChoice::Choice::bookmark:
                break;
        }
    }

    return max_tick;
}

[[nodiscard]] int velocity_from_core_note(const mx::core::Note& note) {
    const auto attributes = note.getAttributes();
    if (attributes && attributes->hasDynamics) {
        const double dynamics = static_cast<double>(attributes->dynamics.getValue());
        return std::clamp(static_cast<int>(std::lround(dynamics)), 0, 127);
    }
    return kDefaultVelocity;
}

template<typename LyricsOut>
void collect_note_lyrics(
    const mx::core::Note& note, const int absolute_time, LyricsOut& lyrics_out
) {
    for (const auto& lyric : note.getLyricSet()) {
        const auto text_choice = lyric->getLyricTextChoice();
        if (!text_choice) { continue; }
        if (text_choice->getChoice() != mx::core::LyricTextChoice::Choice::syllabicTextGroup) {
            continue;
        }
        const auto group = text_choice->getSyllabicTextGroup();
        if (!group) { continue; }
        const auto text = group->getText();
        if (!text) { continue; }
        lyrics_out.emplace_back(absolute_time, text->getValue().getValue());
    }
}

TrackNative<Tick> import_musicxml_track_tick(
    const PartData&                part_data,
    const mx::core::PartwisePart&  core_part,
    const int                      global_tpq,
    std::vector<AbsoluteTempo>&    tempos_out,
    std::vector<AbsoluteKeySignature>& keys_out,
    std::vector<TimeSignature<Tick>>& time_signatures_out
) {
    TrackNative<Tick> tick_track{
        !part_data.displayName.empty() ? part_data.displayName : part_data.name,
        static_cast<u8>(
            std::clamp(part_data.instrumentData.midiData.program - 1, 0, 127)
        ),
        part_data.instrumentData.midiData.channel == kDrumMusicXmlChannel
    };

    const int num_staves = std::max(1, part_data.getNumStaves());
    mx::impl::MeasureCursor cursor{num_staves, global_tpq};
    cursor.partIndex = 0;

    std::map<TieKey, ActiveTie> active_ties;
    int                         absolute_measure_start = 0;

    const auto& measures = core_part.getPartwiseMeasureSet();
    for (size_t measure_index = 0; measure_index < measures.size(); ++measure_index) {
        cursor.reset();
        cursor.measureIndex = static_cast<int>(measure_index);

        const auto& measure_data = part_data.measures.at(measure_index);
        if (!measure_data.timeSignature.isImplicit) {
            time_signatures_out.emplace_back(
                absolute_measure_start,
                static_cast<u8>(measure_data.timeSignature.beats),
                static_cast<u8>(measure_data.timeSignature.beatType)
            );
        }

        for (const auto& key_data : measure_data.keys) {
            const auto tonality = tonality_from_mode(key_data.mode);
            if (!tonality.has_value()) { continue; }
            keys_out.push_back(
                {absolute_measure_start + key_data.tickTimePosition, static_cast<i8>(key_data.fifths), *tonality}
            );
        }

        for (const auto& staff : measure_data.staves) {
            for (const auto& key_data : staff.keys) {
                const auto tonality = tonality_from_mode(key_data.mode);
                if (!tonality.has_value()) { continue; }
                keys_out.push_back(
                    {absolute_measure_start + key_data.tickTimePosition, static_cast<i8>(key_data.fifths), *tonality}
                );
            }
            for (const auto& direction : staff.directions) {
                for (const auto& tempo_data : direction.tempos) {
                    try {
                        tempos_out.push_back(
                            {absolute_measure_start + direction.tickTimePosition, mspq_from_tempo_data(tempo_data)}
                        );
                    } catch (const std::runtime_error&) {
                        // Unsupported tempo markings are ignored in v1.
                    }
                }
            }
        }

        const auto& core_measure = *measures.at(measure_index);
        const int   consumed_ticks = traverse_measure_notes(
            core_measure,
            cursor,
            static_cast<int>(measure_index),
            absolute_measure_start,
            [&](const NoteData& note_data,
                const mx::core::NotePtr& core_note,
                const int,
                const int staff_index,
                const int absolute_time) {
                if (note_data.isRest || note_data.durationData.durationTimeTicks <= 0) { return; }

                const auto midi_pitch = midi_pitch_from_musicxml_pitch(note_data.pitchData);
                const auto velocity = velocity_from_core_note(*core_note);
                const TieKey tie_key{staff_index, note_data.userRequestedVoiceNumber, midi_pitch};

                collect_note_lyrics(*core_note, absolute_time, tick_track.lyrics);

                if (note_data.isTieStop) {
                    auto active = active_ties.find(tie_key);
                    if (active != active_ties.end() && active->second.start + active->second.duration == absolute_time) {
                        active->second.duration += note_data.durationData.durationTimeTicks;
                        if (!note_data.isTieStart) {
                            tick_track.notes.emplace_back(
                                active->second.start,
                                active->second.duration,
                                static_cast<i8>(active->second.pitch),
                                static_cast<i8>(active->second.velocity)
                            );
                            active_ties.erase(active);
                        }
                        return;
                    }
                }

                if (note_data.isTieStart) {
                    active_ties[tie_key] = {
                        absolute_time,
                        note_data.durationData.durationTimeTicks,
                        static_cast<i8>(midi_pitch),
                        static_cast<i8>(velocity)
                    };
                    return;
                }

                tick_track.notes.emplace_back(
                    absolute_time,
                    note_data.durationData.durationTimeTicks,
                    static_cast<i8>(midi_pitch),
                    static_cast<i8>(velocity)
                );
            }
        );

        const int nominal_ticks = nominal_measure_length(measure_data.timeSignature, global_tpq);
        const int measure_ticks = measure_data.implicit == mx::api::Bool::yes
            ? std::max(consumed_ticks, 0)
            : std::max(consumed_ticks, nominal_ticks);
        absolute_measure_start += std::max(measure_ticks, 0);
    }

    for (const auto& [_, active_tie] : active_ties) {
        tick_track.notes.emplace_back(
            active_tie.start,
            active_tie.duration,
            active_tie.pitch,
            active_tie.velocity
        );
    }

    auto sort_text = [](const auto& lhs, const auto& rhs) {
        return std::tie(lhs.time, lhs.text) < std::tie(rhs.time, rhs.text);
    };
    auto sort_note = [](const auto& lhs, const auto& rhs) {
        return std::tie(lhs.time, lhs.pitch, lhs.duration, lhs.velocity)
            < std::tie(rhs.time, rhs.pitch, rhs.duration, rhs.velocity);
    };
    std::sort(tick_track.notes.begin(), tick_track.notes.end(), sort_note);
    std::sort(tick_track.lyrics.begin(), tick_track.lyrics.end(), sort_text);

    return tick_track;
}

template<TType T>
TrackNative<T> import_musicxml_track(
    const PartData&                    part_data,
    const mx::core::PartwisePart&      core_part,
    const int                          global_tpq,
    std::vector<AbsoluteTempo>&        tempos_out,
    std::vector<AbsoluteKeySignature>& keys_out,
    std::vector<TimeSignature<Tick>>&  time_signatures_out
) {
    auto tick_track = import_musicxml_track_tick(
        part_data,
        core_part,
        global_tpq,
        tempos_out,
        keys_out,
        time_signatures_out
    );

    if constexpr (std::is_same_v<T, Tick>) {
        return tick_track;
    } else {
        auto single_track_score = ScoreNative<Tick>{global_tpq};
        single_track_score.tracks.emplace_back(std::move(tick_track));
        auto converted_score = convert<T>(to_shared(std::move(single_track_score)));
        return to_native(converted_score.tracks->at(0)->copy());
    }
}

Score<Tick> import_musicxml_tick_score(const std::span<const u8> bytes) {
    const auto parsed = parse_musicxml_document(bytes);
    const int  tpq = sanitize_musicxml_tpq(parsed.score_data.ticksPerQuarter);

    ScoreNative<Tick> native_score(tpq);
    std::vector<AbsoluteTempo>        tempos;
    std::vector<AbsoluteKeySignature> keys;
    std::vector<TimeSignature<Tick>>  time_signatures;

    const auto& parts = parsed.score_data.parts;
    const auto& core_parts = parsed.score_partwise->getPartwisePartSet();
    if (parts.size() != core_parts.size()) {
        throw std::runtime_error("MusicXML part reconciliation failed");
    }

    native_score.tracks.reserve(parts.size());
    for (size_t i = 0; i < parts.size(); ++i) {
        native_score.tracks.emplace_back(import_musicxml_track<Tick>(
            parts.at(i),
            *core_parts.at(i),
            tpq,
            tempos,
            keys,
            time_signatures
        ));
    }

    std::sort(time_signatures.begin(), time_signatures.end(), [](const auto& lhs, const auto& rhs) {
        return std::tie(lhs.time, lhs.numerator, lhs.denominator)
            < std::tie(rhs.time, rhs.numerator, rhs.denominator);
    });
    time_signatures.erase(
        std::unique(
            time_signatures.begin(),
            time_signatures.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.time == rhs.time && lhs.numerator == rhs.numerator
                    && lhs.denominator == rhs.denominator;
            }
        ),
        time_signatures.end()
    );
    native_score.time_signatures = std::move(time_signatures);

    std::sort(keys.begin(), keys.end(), [](const auto& lhs, const auto& rhs) {
        return std::tie(lhs.tick_time, lhs.key, lhs.tonality)
            < std::tie(rhs.tick_time, rhs.key, rhs.tonality);
    });
    keys.erase(
        std::unique(
            keys.begin(),
            keys.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.tick_time == rhs.tick_time && lhs.key == rhs.key
                    && lhs.tonality == rhs.tonality;
            }
        ),
        keys.end()
    );
    native_score.key_signatures.reserve(keys.size());
    for (const auto& key : keys) {
        native_score.key_signatures.emplace_back(key.tick_time, key.key, key.tonality);
    }

    std::sort(tempos.begin(), tempos.end(), [](const auto& lhs, const auto& rhs) {
        return std::tie(lhs.tick_time, lhs.mspq) < std::tie(rhs.tick_time, rhs.mspq);
    });
    tempos.erase(
        std::unique(
            tempos.begin(),
            tempos.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.tick_time == rhs.tick_time && lhs.mspq == rhs.mspq;
            }
        ),
        tempos.end()
    );
    native_score.tempos.reserve(tempos.size());
    for (const auto& tempo : tempos) {
        native_score.tempos.emplace_back(tempo.tick_time, tempo.mspq);
    }

    return to_shared(std::move(native_score));
}

[[nodiscard]] std::optional<std::pair<DurationName, int>> find_duration_name_and_dots(
    const int duration_ticks, const int tpq
) {
    using candidate = std::pair<DurationName, long double>;
    static constexpr std::array<candidate, 14> base_values = {
        candidate{DurationName::maxima, 32.0L},
        candidate{DurationName::longa, 16.0L},
        candidate{DurationName::breve, 8.0L},
        candidate{DurationName::whole, 4.0L},
        candidate{DurationName::half, 2.0L},
        candidate{DurationName::quarter, 1.0L},
        candidate{DurationName::eighth, 0.5L},
        candidate{DurationName::dur16th, 0.25L},
        candidate{DurationName::dur32nd, 0.125L},
        candidate{DurationName::dur64th, 0.0625L},
        candidate{DurationName::dur128th, 0.03125L},
        candidate{DurationName::dur256th, 0.015625L},
        candidate{DurationName::dur512th, 0.0078125L},
        candidate{DurationName::dur1024th, 0.00390625L},
    };

    const auto equals = [](const long double lhs, const long double rhs) {
        return std::abs(lhs - rhs) < 0.0001L;
    };

    for (const auto& [name, quarter_value] : base_values) {
        const long double base_ticks = quarter_value * static_cast<long double>(tpq);
        long double       total = base_ticks;
        long double       add = base_ticks / 2.0L;
        if (equals(total, static_cast<long double>(duration_ticks))) { return {{name, 0}}; }
        for (int dots = 1; dots <= kMaxDotCount; ++dots) {
            total += add;
            add /= 2.0L;
            if (equals(total, static_cast<long double>(duration_ticks))) { return {{name, dots}}; }
        }
    }

    return std::nullopt;
}

[[nodiscard]] DurationName measure_placeholder_duration_name() {
    return DurationName::quarter;
}

[[nodiscard]] std::vector<MeasureLayout> build_measure_layouts(const Score<Tick>& score) {
    std::vector<TimeSignature<Tick>> time_signatures = score.time_signatures->collect();
    std::sort(time_signatures.begin(), time_signatures.end(), [](const auto& lhs, const auto& rhs) {
        return std::tie(lhs.time, lhs.numerator, lhs.denominator)
            < std::tie(rhs.time, rhs.numerator, rhs.denominator);
    });

    const auto max_event_time = [&]() {
        int result = score.end();
        for (const auto& tempo : *score.tempos) { result = std::max(result, tempo.time); }
        for (const auto& key : *score.key_signatures) { result = std::max(result, key.time); }
        for (const auto& ts : time_signatures) { result = std::max(result, ts.time); }
        for (const auto& marker : *score.markers) { result = std::max(result, marker.time); }
        return result;
    }();

    auto current_time_signature = TimeSignature<Tick>{0, 4, 4};
    size_t time_signature_index = 0;

    std::vector<MeasureLayout> measures;
    int measure_start = 0;
    while (measure_start <= max_event_time || measures.empty()) {
        if (time_signature_index < time_signatures.size()
            && time_signatures[time_signature_index].time == measure_start) {
            current_time_signature = time_signatures[time_signature_index];
            ++time_signature_index;
        }

        const int nominal_length = nominal_measure_length(
            make_time_signature_data(
                current_time_signature.numerator,
                current_time_signature.denominator,
                false
            ),
            score.ticks_per_quarter
        );
        int measure_length = nominal_length;
        if (time_signature_index < time_signatures.size()) {
            const int next_change_time = time_signatures[time_signature_index].time;
            if (next_change_time > measure_start && next_change_time < measure_start + nominal_length) {
                measure_length = next_change_time - measure_start;
            }
        }

        auto measure = MeasureLayout{};
        measure.start_tick = measure_start;
        measure.length = std::max(1, measure_length);
        measures.push_back(measure);
        measure_start += measure.length;
    }

    return measures;
}

[[nodiscard]] size_t find_measure_index(
    const std::vector<MeasureLayout>& measures, const int tick_time
) {
    auto iter = std::upper_bound(
        measures.begin(),
        measures.end(),
        tick_time,
        [](const int value, const MeasureLayout& measure) { return value < measure.start_tick; }
    );
    if (iter == measures.begin()) { return 0; }
    --iter;
    return static_cast<size_t>(std::distance(measures.begin(), iter));
}

[[nodiscard]] std::vector<ChordCluster> assign_cluster_voices(const Track<Tick>& track) {
    std::vector<ChordCluster> clusters;
    clusters.reserve(track.notes->size());

    std::map<std::pair<int, int>, size_t> cluster_lookup;
    for (size_t i = 0; i < track.notes->size(); ++i) {
        const auto& note = track.notes->at(i);
        const auto  key = std::pair{note.time, note.time + note.duration};
        if (auto iter = cluster_lookup.find(key); iter != cluster_lookup.end()) {
            clusters[iter->second].note_indices.push_back(i);
        } else {
            auto cluster = ChordCluster{};
            cluster.start = note.time;
            cluster.end = note.time + note.duration;
            cluster.note_indices.push_back(i);
            cluster_lookup.emplace(key, clusters.size());
            clusters.push_back(std::move(cluster));
        }
    }

    std::sort(clusters.begin(), clusters.end(), [](const auto& lhs, const auto& rhs) {
        return std::tie(lhs.start, lhs.end) < std::tie(rhs.start, rhs.end);
    });

    std::vector<int> voice_end_times;
    for (auto& cluster : clusters) {
        auto assigned = false;
        for (size_t voice = 0; voice < voice_end_times.size(); ++voice) {
            if (voice_end_times[voice] <= cluster.start) {
                cluster.voice = static_cast<int>(voice);
                voice_end_times[voice] = cluster.end;
                assigned = true;
                break;
            }
        }
        if (!assigned) {
            cluster.voice = static_cast<int>(voice_end_times.size());
            voice_end_times.push_back(cluster.end);
        }
    }

    return clusters;
}

[[nodiscard]] std::vector<SegmentInfo> split_track_into_segments(
    const Track<Tick>&               track,
    const std::vector<MeasureLayout>& measures,
    const int                       tpq
) {
    auto clusters = assign_cluster_voices(track);
    std::unordered_map<size_t, int> note_to_voice;
    note_to_voice.reserve(track.notes->size());
    for (const auto& cluster : clusters) {
        for (const auto index : cluster.note_indices) {
            note_to_voice.emplace(index, cluster.voice);
        }
    }

    std::vector<SegmentInfo> segments;
    for (size_t note_index = 0; note_index < track.notes->size(); ++note_index) {
        const auto& note = track.notes->at(note_index);
        if (note.duration <= 0 || note.velocity <= 0) { continue; }

        int start = note.time;
        const int end = note.time + note.duration;
        bool first_segment = true;
        while (start < end) {
            const auto measure_index = find_measure_index(measures, start);
            const auto& measure = measures.at(measure_index);
            const int segment_end = std::min(end, measure.start_tick + measure.length);
            const int segment_duration = segment_end - start;
            const auto duration_info = find_duration_name_and_dots(segment_duration, tpq);

            auto segment = SegmentInfo{};
            segment.measure_index = static_cast<int>(measure_index);
            segment.voice = note_to_voice.at(note_index);
            segment.abs_start = start;
            segment.local_start = start - measure.start_tick;
            segment.duration = segment_duration;
            segment.pitch = note.pitch;
            segment.velocity = note.velocity;
            segment.tie_stop = !first_segment;
            segment.tie_start = segment_end < end;
            if (duration_info.has_value()) {
                segment.duration_name = duration_info->first;
                segment.duration_dots = duration_info->second;
            } else {
                segment.duration_name = measure_placeholder_duration_name();
                segment.duration_dots = 0;
                segment.needs_type_patch = true;
            }

            segments.push_back(segment);
            start = segment_end;
            first_segment = false;
        }
    }

    std::sort(segments.begin(), segments.end(), [](const auto& lhs, const auto& rhs) {
        return std::tie(lhs.measure_index, lhs.voice, lhs.local_start, lhs.duration, lhs.pitch, lhs.velocity)
            < std::tie(rhs.measure_index, rhs.voice, rhs.local_start, rhs.duration, rhs.pitch, rhs.velocity);
    });

    int current_group = -1;
    std::tuple<int, int, int> previous_key{-1, -1, -1};
    for (auto& segment : segments) {
        const auto key = std::tuple{segment.measure_index, segment.voice, segment.local_start};
        if (key != previous_key) { ++current_group; }
        segment.chord_group = current_group;
        previous_key = key;
    }

    return segments;
}

[[nodiscard]] std::map<int, std::vector<std::string>> group_lyrics_by_time(const Track<Tick>& track) {
    std::map<int, std::vector<std::string>> lyrics_by_time;
    for (const auto& lyric : *track.lyrics) { lyrics_by_time[lyric.time].push_back(lyric.text); }
    return lyrics_by_time;
}

[[nodiscard]] int export_channel_for_track(const bool is_drum, const int non_drum_index) {
    if (is_drum) { return kDrumMusicXmlChannel; }
    static constexpr std::array<int, 15> channels = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 16
    };
    return channels[static_cast<size_t>(non_drum_index % static_cast<int>(channels.size()))];
}

[[nodiscard]] ScoreData export_score_to_musicxml_data(
    const Score<Tick>& score,
    std::vector<std::map<int, std::vector<std::string>>>& part_lyrics_by_time,
    std::vector<std::map<OutputPatchKey, std::deque<OutputPatchInfo>>>& part_patch_lookup
) {
    ScoreData score_data{};
    score_data.musicXmlVersion = mx::api::MusicXmlVersion::ThreePointZero;
    score_data.ticksPerQuarter = sanitize_musicxml_tpq(score.ticks_per_quarter);

    const auto measures = build_measure_layouts(score);
    part_lyrics_by_time.clear();
    part_patch_lookup.clear();
    part_lyrics_by_time.reserve(score.tracks->size());
    part_patch_lookup.reserve(score.tracks->size());

    std::vector<mx::api::TimeSignatureData> measure_time_signatures(measures.size());
    {
        std::vector<TimeSignature<Tick>> time_signatures = score.time_signatures->collect();
        std::sort(time_signatures.begin(), time_signatures.end(), [](const auto& lhs, const auto& rhs) {
            return std::tie(lhs.time, lhs.numerator, lhs.denominator)
                < std::tie(rhs.time, rhs.numerator, rhs.denominator);
        });

        size_t ts_index = 0;
        TimeSignature<Tick> current_ts{0, 4, 4};
        bool current_explicit = false;
        for (size_t measure_index = 0; measure_index < measures.size(); ++measure_index) {
            current_explicit = false;
            if (ts_index < time_signatures.size()
                && time_signatures[ts_index].time == measures[measure_index].start_tick) {
                current_ts = time_signatures[ts_index];
                current_explicit = true;
                ++ts_index;
            }
            auto time_signature = mx::api::TimeSignatureData{};
            time_signature.beats = current_ts.numerator;
            time_signature.beatType = current_ts.denominator;
            time_signature.isImplicit = !current_explicit;
            measure_time_signatures[measure_index] = time_signature;
        }
    }

    std::vector<std::vector<mx::api::KeyData>> measure_keys(measures.size());
    for (const auto& key_signature : *score.key_signatures) {
        const auto measure_index = find_measure_index(measures, key_signature.time);
        auto       key_data = mx::api::KeyData{};
        key_data.fifths = key_signature.key;
        key_data.mode = key_signature.tonality == 1 ? mx::api::KeyMode::minor : mx::api::KeyMode::major;
        key_data.tickTimePosition = key_signature.time - measures[measure_index].start_tick;
        measure_keys[measure_index].push_back(std::move(key_data));
    }

    std::vector<std::vector<mx::api::DirectionData>> measure_directions(measures.size());
    for (const auto& tempo : *score.tempos) {
        const auto measure_index = find_measure_index(measures, tempo.time);
        auto       direction = mx::api::DirectionData{};
        direction.tickTimePosition = tempo.time - measures[measure_index].start_tick;
        direction.isStaffValueSpecified = false;

        auto tempo_data = mx::api::TempoData{};
        tempo_data.tempoType = mx::api::TempoType::beatsPerMinute;
        tempo_data.beatsPerMinute.durationName = DurationName::quarter;
        tempo_data.beatsPerMinute.dots = 0;
        tempo_data.beatsPerMinute.beatsPerMinute = static_cast<int>(std::lround(tempo.qpm()));
        direction.tempos.push_back(std::move(tempo_data));
        measure_directions[measure_index].push_back(std::move(direction));
    }

    int non_drum_index = 0;
    for (size_t track_index = 0; track_index < score.tracks->size(); ++track_index) {
        const auto& track = *score.tracks->at(track_index);
        auto        part = PartData{};
        part.uniqueId = fmt::format("P{}", track_index + 1);
        part.instrumentData.uniqueId = fmt::format("{}-I1", part.uniqueId);
        part.name = track.name.empty() ? fmt::format("Track {}", track_index + 1) : track.name;
        part.displayName = part.name;
        part.instrumentData.name = part.name;
        part.instrumentData.midiData.program = static_cast<int>(track.program) + 1;
        part.instrumentData.midiData.channel
            = export_channel_for_track(track.is_drum, non_drum_index);
        if (!track.is_drum) { ++non_drum_index; }

        part.measures.resize(measures.size());
        for (size_t measure_index = 0; measure_index < measures.size(); ++measure_index) {
            auto& measure = part.measures[measure_index];
            measure.timeSignature = measure_time_signatures[measure_index];
            measure.keys = measure_keys[measure_index];
            measure.staves.emplace_back(mx::api::StaffData{});
            measure.staves.front().directions = measure_directions[measure_index];
        }

        auto segments = split_track_into_segments(track, measures, score.ticks_per_quarter);
        std::vector<std::vector<TrackMeasureNote>> grouped_segments(measures.size());
        grouped_segments.reserve(measures.size());
        std::map<OutputPatchKey, std::deque<OutputPatchInfo>> patch_lookup;
        for (const auto& segment : segments) {
            auto note_data = TrackMeasureNote{};
            note_data.voice = segment.voice;
            note_data.chord_group = segment.chord_group;
            note_data.abs_time = segment.abs_start;
            note_data.local_time = segment.local_start;
            note_data.duration = segment.duration;
            note_data.pitch = segment.pitch;
            note_data.velocity = segment.velocity;
            note_data.tie_start = segment.tie_start;
            note_data.tie_stop = segment.tie_stop;
            note_data.needs_type_patch = segment.needs_type_patch;
            note_data.duration_name = segment.duration_name;
            note_data.duration_dots = segment.duration_dots;
            grouped_segments[segment.measure_index].push_back(std::move(note_data));

            auto patch_info = OutputPatchInfo{
                segment.measure_index,
                segment.local_start,
                segment.duration,
                segment.pitch,
                segment.voice,
                segment.velocity,
                segment.needs_type_patch
            };
            const auto patch_key = OutputPatchKey{
                segment.measure_index,
                segment.local_start,
                segment.duration,
                segment.pitch,
                segment.voice
            };
            patch_lookup[patch_key].push_back(std::move(patch_info));
        }

        for (size_t measure_index = 0; measure_index < grouped_segments.size(); ++measure_index) {
            auto& staff = part.measures[measure_index].staves.front();
            auto& notes = grouped_segments[measure_index];
            std::sort(notes.begin(), notes.end(), [](const auto& lhs, const auto& rhs) {
                return std::tie(lhs.voice, lhs.local_time, lhs.duration, lhs.chord_group, lhs.pitch)
                    < std::tie(rhs.voice, rhs.local_time, rhs.duration, rhs.chord_group, rhs.pitch);
            });

            std::map<int, mx::api::VoiceData> voices;
            int current_group = -1;
            int group_size = 0;
            for (size_t index = 0; index < notes.size(); ++index) {
                if (notes[index].chord_group != current_group) {
                    current_group = notes[index].chord_group;
                    group_size = 0;
                    for (size_t look_ahead = index;
                         look_ahead < notes.size() && notes[look_ahead].chord_group == current_group;
                         ++look_ahead) {
                        ++group_size;
                    }
                }

                auto note = NoteData{};
                note.tickTimePosition = notes[index].local_time;
                note.pitchData = pitch_data_from_midi_pitch(notes[index].pitch);
                note.durationData.durationTimeTicks = notes[index].duration;
                note.durationData.durationName = notes[index].duration_name;
                note.durationData.durationDots = notes[index].duration_dots;
                note.durationData.timeModificationActualNotes = 1;
                note.durationData.timeModificationNormalNotes = 1;
                note.noteType = mx::api::NoteType::normal;
                note.isChord = group_size > 1;
                note.isTieStart = notes[index].tie_start;
                note.isTieStop = notes[index].tie_stop;
                note.userRequestedVoiceNumber = notes[index].voice + 1;

                voices[notes[index].voice].notes.push_back(std::move(note));
            }
            staff.voices = std::move(voices);
        }

        part_lyrics_by_time.push_back(group_lyrics_by_time(track));
        part_patch_lookup.push_back(std::move(patch_lookup));
        score_data.parts.push_back(std::move(part));
    }

    return score_data;
}

void add_lyric_to_note(mx::core::Note& note, const std::string& text) {
    auto lyric = mx::core::makeLyric();
    lyric->getLyricTextChoice()->setChoice(
        mx::core::LyricTextChoice::Choice::syllabicTextGroup
    );
    lyric->getLyricTextChoice()
        ->getSyllabicTextGroup()
        ->getText()
        ->setValue(mx::core::XsString{text});
    note.addLyric(std::move(lyric));
}

void patch_exported_partwise_score(
    mx::core::ScorePartwise&                                 score_partwise,
    const std::vector<std::map<int, std::vector<std::string>>>& part_lyrics_by_time,
    std::vector<std::map<OutputPatchKey, std::deque<OutputPatchInfo>>>& part_patch_lookup,
    const int                                                global_tpq
) {
    const auto& parts = score_partwise.getPartwisePartSet();
    if (parts.size() != part_patch_lookup.size()) {
        throw std::runtime_error("Internal MusicXML patch bookkeeping mismatch");
    }

    for (size_t part_index = 0; part_index < parts.size(); ++part_index) {
        auto&               core_part = *parts.at(part_index);
        auto&               patch_lookup = part_patch_lookup.at(part_index);
        auto                remaining_lyrics = part_lyrics_by_time.at(part_index);
        mx::impl::MeasureCursor cursor{1, global_tpq};
        int absolute_measure_start = 0;

        const auto& measures = core_part.getPartwiseMeasureSet();
        for (size_t measure_index = 0; measure_index < measures.size(); ++measure_index) {
            cursor.reset();
            cursor.measureIndex = static_cast<int>(measure_index);
            int last_measure_tick = 0;

            last_measure_tick = traverse_measure_notes(
                *measures.at(measure_index),
                cursor,
                static_cast<int>(measure_index),
                absolute_measure_start,
                [&](const NoteData& note_data,
                    const mx::core::NotePtr& core_note_ptr,
                    const int current_measure_index,
                    const int,
                    const int absolute_time) {
                    if (note_data.isRest) { return; }
                    const auto actual_key = OutputPatchKey{
                        current_measure_index,
                        note_data.tickTimePosition,
                        note_data.durationData.durationTimeTicks,
                        midi_pitch_from_musicxml_pitch(note_data.pitchData),
                        note_data.userRequestedVoiceNumber - 1
                    };
                    const auto patch_iter = patch_lookup.find(actual_key);
                    if (patch_iter == patch_lookup.end() || patch_iter->second.empty()) {
                        throw std::runtime_error(
                            fmt::format(
                                "Internal MusicXML note patch lookup miss: measure={} "
                                "time={} duration={} pitch={} voice={}",
                                actual_key.measure_index,
                                actual_key.local_time,
                                actual_key.duration,
                                actual_key.pitch,
                                actual_key.voice
                            )
                        );
                    }
                    auto expected = patch_iter->second.front();
                    patch_iter->second.pop_front();
                    if (patch_iter->second.empty()) { patch_lookup.erase(patch_iter); }

                    auto& core_note = *core_note_ptr;
                    auto  attributes = core_note.getAttributes();
                    attributes->hasDynamics = true;
                    attributes->dynamics = mx::core::NonNegativeDecimal{
                        static_cast<long double>(std::clamp(expected.velocity, 0, 127))
                    };

                    if (expected.needs_type_patch) {
                        core_note.setHasType(false);
                        core_note.clearDotSet();
                        core_note.setHasTimeModification(false);
                    }

                    if (auto lyric_iter = remaining_lyrics.find(absolute_time);
                        lyric_iter != remaining_lyrics.end()) {
                        for (const auto& text : lyric_iter->second) { add_lyric_to_note(core_note, text); }
                        remaining_lyrics.erase(lyric_iter);
                    }
                }
            );
            absolute_measure_start += last_measure_tick;
        }

        if (!patch_lookup.empty()) {
            throw std::runtime_error("Internal MusicXML note patch lookup overflow");
        }
    }
}

template<TType T>
Score<T> parse_musicxml_score(const std::span<const u8> bytes) {
    const auto tick_score = import_musicxml_tick_score(bytes);
    if constexpr (std::is_same_v<T, Tick>) {
        return tick_score;
    } else {
        return convert<T>(tick_score);
    }
}

template<TType T>
vec<u8> dump_musicxml_score(const Score<T>& score) {
    const auto tick_score = [&]() -> Score<Tick> {
        if constexpr (std::is_same_v<T, Tick>) {
            return score;
        } else {
            return convert<Tick>(score);
        }
    }();

    std::vector<std::map<int, std::vector<std::string>>>             lyrics_by_time;
    std::vector<std::map<OutputPatchKey, std::deque<OutputPatchInfo>>> patch_lookup;
    auto score_data = export_score_to_musicxml_data(tick_score, lyrics_by_time, patch_lookup);

    mx::impl::ScoreWriter writer{score_data};
    auto                  score_partwise = writer.getScorePartwise();
    patch_exported_partwise_score(
        *score_partwise,
        lyrics_by_time,
        patch_lookup,
        score_data.ticksPerQuarter
    );

    auto document = mx::core::makeDocument(DocumentChoice::partwise);
    document->setScorePartwise(score_partwise);
    std::ostringstream stream;
    document->toStream(stream);
    const auto xml = stream.str();
    return {
        reinterpret_cast<const u8*>(xml.data()),
        reinterpret_cast<const u8*>(xml.data()) + xml.size()
    };
}

}   // namespace

template<>
template<>
Score<Tick> Score<Tick>::parse<DataFormat::MusicXML>(const std::span<const u8> bytes) {
    return parse_musicxml_score<Tick>(bytes);
}

template<>
template<>
Score<Quarter> Score<Quarter>::parse<DataFormat::MusicXML>(const std::span<const u8> bytes) {
    return parse_musicxml_score<Quarter>(bytes);
}

template<>
template<>
Score<Second> Score<Second>::parse<DataFormat::MusicXML>(const std::span<const u8> bytes) {
    return parse_musicxml_score<Second>(bytes);
}

template<>
template<>
vec<u8> Score<Tick>::dumps<DataFormat::MusicXML>() const {
    return dump_musicxml_score(*this);
}

template<>
template<>
vec<u8> Score<Quarter>::dumps<DataFormat::MusicXML>() const {
    return dump_musicxml_score(*this);
}

template<>
template<>
vec<u8> Score<Second>::dumps<DataFormat::MusicXML>() const {
    return dump_musicxml_score(*this);
}

template<>
Score<Tick> parse<DataFormat::MusicXML, Score<Tick>>(std::span<const u8> bytes) {
    return Score<Tick>::parse<DataFormat::MusicXML>(bytes);
}

template<>
Score<Tick> parse<DataFormat::MusicXML, Score<Tick>>(
    std::span<const u8> bytes, bool sanitize_data
) {
    if (sanitize_data) {
        throw std::invalid_argument("sanitize_data is only supported for MIDI input");
    }
    return Score<Tick>::parse<DataFormat::MusicXML>(bytes);
}

template<>
vec<u8> dumps<DataFormat::MusicXML, Score<Tick>>(const Score<Tick>& data) {
    return data.dumps<DataFormat::MusicXML>();
}

template<>
Score<Quarter> parse<DataFormat::MusicXML, Score<Quarter>>(std::span<const u8> bytes) {
    return Score<Quarter>::parse<DataFormat::MusicXML>(bytes);
}

template<>
Score<Quarter> parse<DataFormat::MusicXML, Score<Quarter>>(
    std::span<const u8> bytes, bool sanitize_data
) {
    if (sanitize_data) {
        throw std::invalid_argument("sanitize_data is only supported for MIDI input");
    }
    return Score<Quarter>::parse<DataFormat::MusicXML>(bytes);
}

template<>
vec<u8> dumps<DataFormat::MusicXML, Score<Quarter>>(const Score<Quarter>& data) {
    return data.dumps<DataFormat::MusicXML>();
}

template<>
Score<Second> parse<DataFormat::MusicXML, Score<Second>>(std::span<const u8> bytes) {
    return Score<Second>::parse<DataFormat::MusicXML>(bytes);
}

template<>
Score<Second> parse<DataFormat::MusicXML, Score<Second>>(
    std::span<const u8> bytes, bool sanitize_data
) {
    if (sanitize_data) {
        throw std::invalid_argument("sanitize_data is only supported for MIDI input");
    }
    return Score<Second>::parse<DataFormat::MusicXML>(bytes);
}

template<>
vec<u8> dumps<DataFormat::MusicXML, Score<Second>>(const Score<Second>& data) {
    return data.dumps<DataFormat::MusicXML>();
}

}   // namespace symusic
