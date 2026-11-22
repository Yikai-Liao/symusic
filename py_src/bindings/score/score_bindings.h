#pragma once

#include "utils/filesystem_error.h"
#include "utils/vector_bindings.h"
#include "../core/binding_common.h"

namespace symusic::py {

constexpr const char* kScoreTpqCtorDoc
    = R"pbdoc(Create an empty score with the given ticks per quarter note.)pbdoc";
constexpr const char* kScorePathCtorDoc = R"pbdoc(Read a MIDI or ABC file.)pbdoc";
constexpr const char* kScoreFromMidiDoc = R"pbdoc(
Read MIDI bytes. A channel message data byte above 127 raises ``RuntimeError``; with
*sanitize_data* it is clamped to 127 instead.
)pbdoc";
constexpr const char* kScoreDumpMidiDoc = R"pbdoc(
Write the score to a MIDI file. ``pedals`` are not written; the sustain pedal reaches the file
through control change 64 in ``controls``.
)pbdoc";
constexpr const char* kScoreDumpsMidiDoc
    = R"pbdoc(Return the score as MIDI bytes, as ``dump_midi`` writes them.)pbdoc";
constexpr const char* kScoreGetBeatsDoc     = R"pbdoc(
Return the beat times from *start_time* to the end of the score as a NumPy array. As in
pretty_midi, compound meters (numerator 6, 9, 12, ...) count one beat per three notes of the
denominator. Without a time signature a beat is a quarter note.
)pbdoc";
constexpr const char* kScoreGetDownbeatsDoc = R"pbdoc(
Return the time of the first beat of every bar from *start_time* to the end of the score as a
NumPy array. Without a time signature at *start_time*, 4/4 is assumed.
)pbdoc";
constexpr const char* kScoreToDoc           = R"pbdoc(
Return a new score converted to the marker class *ttype*. Notes and pedals shorter than *min_dur*, in the target unit, are extended to it; pass
``min_dur=1`` when converting to ticks so that no note rounds to duration 0.
)pbdoc";
constexpr const char* kScoreResampleDoc     = R"pbdoc(
Return a tick score with *tpq* ticks per quarter note, with every time rounded to the new grid.
Notes and pedals shorter than *min_dur* ticks are extended to it.
)pbdoc";
constexpr const char* kScorePianorollDoc    = R"pbdoc(
Return a ``uint8`` array of shape ``(modes, tracks, pitches, time)`` with one column per tick.
*modes* lists ``"frame"``, ``"onset"`` or ``"offset"``; *pitch_range* is the pitch interval ``[low,
high)``; *encode_velocity* writes velocities instead of 1. Raises ``ValueError`` for a note with
negative time or duration, or when the score ends before tick 0.
)pbdoc";

constexpr const char* kScoreCopyCtorDoc = R"pbdoc(Create a deep copy of *other*.)pbdoc";
constexpr const char* kScoreCopyDoc     = R"pbdoc(
Return a deep copy, or with ``deep=False`` a new score that shares the ``tracks`` list and every
event.
)pbdoc";
constexpr const char* kScoreSortDoc     = R"pbdoc(
Sort the event lists of every track and the global lists by their default sort key; ``tracks``
keeps its order. Works in place unless *inplace* is false.
)pbdoc";
constexpr const char* kScoreClipDoc     = R"pbdoc(
Keep the events that start in ``[start, end)``; with *clip_end*, notes and pedals must also end by
*end*. Times are not shifted. The tempo, time signature and key signature in effect at *start* move
to *start*. Returns a new score unless *inplace* is true.
)pbdoc";
constexpr const char* kScoreTrimDoc     = R"pbdoc(
Keep notes and pedals that overlap ``[start, end)`` by at least *min_overlap*. At each boundary,
``"remove"`` drops crossing events, ``"truncate"`` cuts them at the boundary, and ``"retain"``
keeps them whole. Invalid modes and negative *min_overlap* raise ``ValueError``. Other events are clipped by time, and the tempo and signatures in effect at
*start* are kept there. Returns a new score with independent tracks and events unless *inplace* is
true, in which case it modifies and returns this score.
)pbdoc";
constexpr const char* kScoreShiftTimeDoc
    = R"pbdoc(Add *offset* to every time. Returns a new score unless *inplace* is true.)pbdoc";
constexpr const char* kScoreShiftPitchDoc    = R"pbdoc(
Add *offset* to every note's pitch. Returns a new score unless *inplace* is true. Raises
``ValueError`` when a result leaves -128..127.
)pbdoc";
constexpr const char* kScoreShiftVelocityDoc = R"pbdoc(
Add *offset* to every note's velocity. Returns a new score unless *inplace* is true. Raises
``ValueError`` when a result leaves -128..127.
)pbdoc";
constexpr const char* kScoreStartDoc
    = R"pbdoc(Return the earliest event time, or 0 when the score has no events.)pbdoc";
constexpr const char* kScoreEndDoc
    = R"pbdoc(Return the latest event end, or 0 when the score has no events.)pbdoc";
constexpr const char* kScoreNoteNumDoc = R"pbdoc(Return the number of notes in all tracks.)pbdoc";
constexpr const char* kScoreEmptyDoc
    = R"pbdoc(Return whether the score has no tracks and no global events.)pbdoc";
constexpr const char* kScoreAdjustTimeDoc = R"pbdoc(
Map times piecewise linearly through the anchor pairs ``(original_times[i], new_times[i])``. The
lists must be the same length and have at least two anchors. Original times must be strictly
increasing, and new times must be nondecreasing. Events outside the anchor range are dropped.
Returns a new score unless *inplace* is true.
)pbdoc";
using ScoreValue                          = UnitValue<Score>;

inline ScoreValue score_from_midi(std::span<const u8> data, nb::handle marker, bool sanitize_data) {
    return with_unit(marker, [&](auto unit) {
        using Native = Score<decltype(unit)>;
        return ScoreValue(std::make_shared<Native>(Native::parse_midi_bytes(data, sanitize_data)));
    });
}

inline nb::object abc_io(const char* name) {
    return nb::module_::import_("symusic._score_io").attr(name);
}

inline ScoreValue score_from_abc(
    const char* function, nb::handle input, nb::handle marker, nb::handle tune
) {
    const auto parse_midi = nb::cpp_function([marker = nb::borrow(marker)](const nb::bytes& data) {
        return score_from_midi(bytes_span(data), marker, false);
    });
    return nb::cast<ScoreValue>(abc_io(function)(input, tune, parse_midi));
}

inline ScoreValue score_from_file(
    const std::filesystem::path&      path,
    nb::handle                        marker,
    const std::optional<std::string>& format,
    bool                              sanitize_data,
    nb::handle                        tune
) {
    if (score_file_format(path, format.value_or("")) == DataFormat::MIDI) {
        if (!tune.is_none()) throw std::invalid_argument("tune is only supported for ABC input");
        return score_from_midi(read_score_file(path), marker, sanitize_data);
    }
    if (sanitize_data)
        throw std::invalid_argument("sanitize_data is only supported for MIDI input");
    return score_from_abc("read_abc_file", decode_path(path), marker, tune);
}

inline void bind_score(nb::module_& module) {
    using TrackValue = UnitValue<Track>;
    auto type        = unit_class<ScoreValue>(
        module,
        "Score",
        "Score(ticks_per_quarter=960, *, ttype=Tick)\n\nA musical score with tracks and global "
        "events. Pass a file path to read MIDI or ABC."
    );
    const auto empty_score = [](i32 ticks_per_quarter, nb::handle marker) {
        return with_unit(marker, [&](auto unit) {
            return ScoreValue(std::make_shared<Score<decltype(unit)>>(ticks_per_quarter));
        });
    };
    type.def(
            "__init__",
            [empty_score](ScoreValue* self, i32 ticks_per_quarter, nb::handle marker) {
                new (self) ScoreValue(empty_score(ticks_per_quarter, marker));
            },
            "ticks_per_quarter"_a = 960,
            nb::kw_only(),
            "ttype"_a = nb::type<Tick>(),
            kScoreTpqCtorDoc
    )
        .def(
            "__init__",
            [](ScoreValue*                       self,
               const std::filesystem::path&      path,
               nb::handle                        marker,
               const std::optional<std::string>& format,
               bool                              sanitize_data,
               nb::handle                        tune) {
                new (self) ScoreValue(score_from_file(path, marker, format, sanitize_data, tune));
            },
            "path"_a,
            nb::kw_only(),
            "ttype"_a         = nb::type<Tick>(),
            "format"_a        = nb::none(),
            "sanitize_data"_a = false,
            "tune"_a          = nb::none(),
            kScorePathCtorDoc
        )
        .def_static(
            "from_tpq",
            empty_score,
            "tpq"_a = 960,
            nb::kw_only(),
            "ttype"_a = nb::type<Tick>(),
            kScoreTpqCtorDoc
        )
        .def_static(
            "from_file",
            &score_from_file,
            "path"_a,
            nb::kw_only(),
            "ttype"_a         = nb::type<Tick>(),
            "format"_a        = nb::none(),
            "sanitize_data"_a = false,
            "tune"_a          = nb::none(),
            "Read a MIDI or ABC file."
        )
        .def_static(
            "from_midi",
            [](const nb::bytes& data, nb::handle marker, bool sanitize_data) {
                return score_from_midi(bytes_span(data), marker, sanitize_data);
            },
            "data"_a,
            nb::kw_only(),
            "ttype"_a         = nb::type<Tick>(),
            "sanitize_data"_a = false,
            kScoreFromMidiDoc
        )
        .def_static(
            "from_abc",
            [](const std::string& abc, nb::handle marker, nb::handle tune) {
                return score_from_abc("read_abc", nb::cast(abc), marker, tune);
            },
            "abc"_a,
            nb::kw_only(),
            "ttype"_a = nb::type<Tick>(),
            "tune"_a  = nb::none(),
            "Read an ABC tune by its X: number, defaulting to the first tune."
        )
        .def_static(
            "from_abc_file",
            [](const std::filesystem::path& path, nb::handle marker, nb::handle tune) {
                return score_from_abc("read_abc_file", decode_path(path), marker, tune);
            },
            "path"_a,
            nb::kw_only(),
            "ttype"_a = nb::type<Tick>(),
            "tune"_a  = nb::none(),
            "Read an ABC file and select a tune by its X: number."
        )
        .def(
            "dump_midi",
            [](const ScoreValue& self, const std::filesystem::path& path) {
                self.visit([&](const auto& ptr) { write_file(path, ptr->dump_midi_bytes()); });
            },
            "path"_a,
            kScoreDumpMidiDoc
        )
        .def(
            "dumps_midi",
            [](const ScoreValue& self) {
                return self.visit([](const auto& ptr) {
                    return python_bytes(ptr->dump_midi_bytes());
                });
            },
            kScoreDumpsMidiDoc
        )
        .def(
            "dump_abc",
            [](const ScoreValue& self, const std::filesystem::path& path, bool warn) {
                self.visit([&](const auto& ptr) {
                    abc_io("write_abc_file")(decode_path(path), python_bytes(ptr->dump_midi_bytes()), warn);
                });
            },
            "path"_a,
            "warn"_a = false,
            "Write ABC, optionally reporting converter diagnostics."
        )
        .def(
            "dumps_abc",
            [](const ScoreValue& self, bool warn) {
                return self.visit([&](const auto& ptr) {
                    return nb::cast<std::string>(
                        abc_io("write_abc")(python_bytes(ptr->dump_midi_bytes()), warn)
                    );
                });
            },
            "warn"_a = false,
            "Return ABC text, optionally reporting converter diagnostics."
        )
        .def(
            "get_beats",
            [](const ScoreValue& self, nb::handle start) {
                return self.visit([&](const auto& ptr) -> nb::object {
                    return nb::cast(
                        to_numpy_array(ptr->get_beats(read_time<UnitOf<decltype(ptr)>>(start)))
                    );
                });
            },
            "start_time"_a = 0,
            nb::sig("def get_beats(self, start_time: int | float = 0) -> numpy.ndarray"),
            kScoreGetBeatsDoc
        )
        .def(
            "get_downbeats",
            [](const ScoreValue& self, nb::handle start) {
                return self.visit([&](const auto& ptr) -> nb::object {
                    return nb::cast(
                        to_numpy_array(ptr->get_downbeats(read_time<UnitOf<decltype(ptr)>>(start)))
                    );
                });
            },
            "start_time"_a = 0,
            nb::sig("def get_downbeats(self, start_time: int | float = 0) -> numpy.ndarray"),
            kScoreGetDownbeatsDoc
        )
        .def(
            "to",
            [](const ScoreValue& self, nb::handle marker, nb::handle min_dur) {
                return with_unit(marker, [&](auto unit) {
                    using T = decltype(unit);
                    const auto minimum
                        = min_dur.is_none() ? typename T::unit{0} : read_time<T>(min_dur);
                    return self.visit([&](const auto& ptr) {
                        return ScoreValue(std::make_shared<Score<T>>(convert<T>(*ptr, minimum)));
                    });
                });
            },
            "ttype"_a,
            "min_dur"_a = nb::none(),
            nb::sig(
                "def to(self, ttype: type[Tick] | type[Quarter] | type[Second], min_dur: int | "
                "float | None = None) -> Score"
            ),
            kScoreToDoc
        )
        .def(
            "resample",
            [](const ScoreValue& self, i32 tpq, nb::handle min_dur) {
                return self.visit([&](const auto& ptr) {
                    using T = UnitOf<decltype(ptr)>;
                    const auto minimum
                        = min_dur.is_none() ?
                              0 :
                              detail::f64_to_i32(static_cast<f64>(read_time<T>(min_dur)));
                    return ScoreValue(std::make_shared<Score<Tick>>(resample(*ptr, tpq, minimum)));
                });
            },
            "tpq"_a,
            "min_dur"_a = nb::none(),
            kScoreResampleDoc
        )
        .def(
            "trim",
            [](ScoreValue&        self,
               nb::handle         start,
               nb::handle         end,
               nb::handle         min_overlap,
               const std::string& start_mode,
               const std::string& end_mode,
               bool               inplace) {
                return self.visit([&](const auto& ptr) -> nb::object {
                    using T            = UnitOf<decltype(ptr)>;
                    const auto first   = read_time<T>(start);
                    const auto last    = read_time<T>(end);
                    const auto minimum = read_time<T>(min_overlap);
                    if (inplace) {
                        ptr->trim_inplace(first, last, minimum, start_mode, end_mode);
                        return nb::cast(&self, nb::rv_policy::reference);
                    }
                    return nb::cast(ScoreValue(
                        std::make_shared<Score<T>>(
                            ptr->trim(first, last, minimum, start_mode, end_mode)
                        )
                    ));
                });
            },
            "start"_a,
            "end"_a,
            "min_overlap"_a = 0,
            "start_mode"_a  = "remove",
            "end_mode"_a    = "remove",
            "inplace"_a     = false,
            nb::sig(
                "def trim(self, start: int | float, end: int | float, min_overlap: int | float = "
                "0, "
                "start_mode: str = 'remove', end_mode: str = 'remove', inplace: bool = False) -> "
                "Score"
            ),
            kScoreTrimDoc
        );

    bind_object_ops(type, [](const auto& score) {
        return std::make_shared<std::decay_t<decltype(score)>>(score.deepcopy());
    });
    bind_scalar_property(
        type,
        "ticks_per_quarter",
        [](auto& score) -> auto& { return score.ticks_per_quarter; },
        "Ticks per quarter note."
    );
    bind_list_property<TrackList, TrackValue>(
        type, "tracks", [](auto& score) -> auto& { return score.tracks; }, "The tracks."
    );
    const auto bind_events = [&]<template<class> class E>(
                                 const char* name, auto field, const char* doc
                             ) {
        bind_list_property<typename EventSequence<E>::Value, UnitValue<E>>(type, name, field, doc);
    };
    bind_events.template operator()<TimeSignature>(
        "time_signatures",
        [](auto& score) -> auto& { return score.time_signatures; },
        "The time signatures."
    );
    bind_events.template operator()<KeySignature>(
        "key_signatures",
        [](auto& score) -> auto& { return score.key_signatures; },
        "The key signatures."
    );
    bind_events.template operator()<Tempo>(
        "tempos", [](auto& score) -> auto& { return score.tempos; }, "The tempo events."
    );
    bind_events.template operator()<TextMeta>(
        "markers", [](auto& score) -> auto& { return score.markers; }, "The text markers."
    );

    bind_queries(type, kScoreStartDoc, kScoreEndDoc, kScoreNoteNumDoc, kScoreEmptyDoc);
    bind_transformations(
        type,
        kScoreSortDoc,
        kScoreClipDoc,
        kScoreAdjustTimeDoc,
        kScoreShiftTimeDoc,
        kScoreShiftPitchDoc,
        kScoreShiftVelocityDoc
    );
    bind_pianoroll<ScorePianoroll>(type, kScorePianorollDoc);
}

}   // namespace symusic::py
