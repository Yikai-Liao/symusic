#pragma once

#include "symusic/io/midi.h"

#include "utils/vector_bindings.h"
#include "../core/binding_common.h"

namespace symusic::py {

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
Return a new score converted to the marker class *ttype*. Notes shorter than *min_duration*, in the target unit, are extended to it; pass
``min_duration=1`` when converting to ticks so that no note rounds to duration 0.
)pbdoc";
constexpr const char* kScoreResampleDoc     = R"pbdoc(
Return a tick score with *ticks_per_quarter* ticks per quarter note, with every time rounded to the new grid.
Notes shorter than *min_duration* ticks are extended to it.
)pbdoc";
constexpr const char* kScorePianorollDoc    = R"pbdoc(
Return a ``uint8`` array of shape ``(modes, tracks, pitches, time)`` with one column per tick.
*modes* lists ``"frame"``, ``"onset"`` or ``"offset"``; *pitch_range* is the pitch interval ``[low,
high)``; *encode_velocity* writes velocities instead of 1. Raises ``ValueError`` for a note with
negative time or duration, or when the score ends before tick 0.
)pbdoc";

constexpr const char* kScoreSortDoc          = R"pbdoc(
Sort the event lists of every track and the global lists by their default sort key; ``tracks``
keeps its order.
)pbdoc";
constexpr const char* kScoreClipDoc          = R"pbdoc(
Keep the events that start in ``[start, end)``; with *clip_end*, notes must also end by
*end*. Times are not shifted. The tempo, time signature and key signature in effect at *start* move
to *start*. Returns a new score.
)pbdoc";
constexpr const char* kScoreTrimDoc          = R"pbdoc(
Keep notes that overlap ``[start, end)`` by at least *min_overlap*. At each boundary,
``"remove"`` drops crossing events, ``"truncate"`` cuts them at the boundary, and ``"retain"``
keeps them whole. Invalid modes and negative *min_overlap* raise ``ValueError``. Other events
are clipped by time, and the tempo and signatures in effect at *start* are kept there. Returns
a new score with independent tracks and events.
)pbdoc";
constexpr const char* kScoreShiftTimeDoc     = R"pbdoc(Add *offset* to every time.)pbdoc";
constexpr const char* kScoreShiftPitchDoc    = R"pbdoc(
Add *offset* to every note's pitch. Raises
``ValueError`` when a result leaves 0..127.
)pbdoc";
constexpr const char* kScoreShiftVelocityDoc = R"pbdoc(
Add *offset* to every note's velocity. Raises
``ValueError`` when a result leaves 0..127.
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
)pbdoc";
using ScoreValue                          = UnitValue<Score>;

inline nb::object abc_io(const char* name) {
    return nb::module_::import_("symusic._score_io").attr(name);
}

inline InvalidDataBytes invalid_data_byte_policy(const std::string& name) {
    if (name == "raise") return InvalidDataBytes::Raise;
    if (name == "clamp") return InvalidDataBytes::Clamp;
    throw std::invalid_argument("invalid_data_bytes must be 'raise' or 'clamp', got " + name);
}

template<TType T>
Score<T> parse_abc(nb::bytes input, nb::handle tune) {
    const auto parse_midi = nb::cpp_function([](const nb::bytes& data) {
        return ScoreValue(std::make_shared<Score<T>>(Score<T>::from_midi(bytes_span(data))));
    });
    auto       score      = nb::cast<ScoreValue>(abc_io("read_abc")(input, tune, parse_midi));
    return std::move(*score.template get<T>());
}

inline std::optional<DataFormat> explicit_format(const std::optional<std::string>& format) {
    if (!format) return std::nullopt;
    if (format->empty()) throw std::invalid_argument("format must be midi or abc");
    return score_file_format({}, *format);
}

inline bool is_path(nb::handle object) {
    return PyUnicode_Check(object.ptr()) || PyBytes_Check(object.ptr())
           || nb::hasattr(object, "__fspath__");
}

template<TType T>
Score<T> load_score(nb::handle source, std::optional<DataFormat> format, i32 ticks_per_quarter) {
    if (source.is_none()) return Score<T>(ticks_per_quarter);
    if (is_path(source)) {
        return read_score<T, SharedStorage>(
            nb::cast<std::filesystem::path>(source), format, [](std::span<const u8> data) {
                return parse_abc<T>(python_bytes(data), nb::none());
            }
        );
    }
    if (!nb::hasattr(source, "read")) {
        throw nb::type_error(
            "Score expects a path or binary file; pass ticks_per_quarter by keyword"
        );
    }
    if (!format) throw std::invalid_argument("format is required for a binary file");
    auto data = source.attr("read")();
    if (!nb::isinstance<nb::bytes>(data)) throw nb::type_error("Expected a binary file");
    auto bytes = nb::borrow<nb::bytes>(data);
    if (*format == DataFormat::ABC) return parse_abc<T>(bytes, nb::none());
    return Score<T>::from_midi(bytes_span(bytes));
}

inline nb::str score_to_abc(const ScoreValue& self) {
    return self.visit([](const auto& ptr) {
        return nb::cast<nb::str>(abc_io("write_abc")(python_bytes(ptr->to_midi())));
    });
}

inline void bind_score(nb::module_& module) {
    using TrackValue = UnitValue<Track>;
    auto type        = unit_class<ScoreValue>(
        module,
        "Score",
        "Score(path_or_file=None, /, *, ttype=Tick, format=None, ticks_per_quarter=960)"
        "\n\nA musical score with tracks and global events."
    );
    type.def(
            "__init__",
            [](ScoreValue*                       self,
               nb::handle                        source,
               nb::handle                        marker,
               const std::optional<std::string>& format,
               nb::handle                        ticks_per_quarter) {
                auto selected = explicit_format(format);
                new (self) ScoreValue(with_unit(marker, [&](auto unit) {
                    using T = decltype(unit);
                    return ScoreValue(
                        std::make_shared<Score<T>>(load_score<T>(
                            source, selected, read_int32(ticks_per_quarter, "ticks_per_quarter")
                        ))
                    );
                }));
            },
            nb::arg() = nb::none(),
            nb::kw_only(),
            "ttype"_a             = nb::type<Tick>(),
            "format"_a            = nb::none(),
            "ticks_per_quarter"_a = 960,
            nb::sig(
                "def __init__(self, path_or_file=None, /, *, ttype=Tick, format=None, "
                "ticks_per_quarter: int = 960) -> None"
            ),
            "Read a MIDI or ABC file, or build an empty score. Binary files require format."
    )
        .def_static(
            "from_midi",
            [](const nb::bytes& data, nb::handle marker, const std::string& invalid_data_bytes) {
                const MidiReadOptions options{invalid_data_byte_policy(invalid_data_bytes)};
                return with_unit(marker, [&](auto unit) {
                    using T = decltype(unit);
                    return ScoreValue(
                        std::make_shared<Score<T>>(Score<T>::from_midi(bytes_span(data), options))
                    );
                });
            },
            "data"_a,
            nb::kw_only(),
            "ttype"_a              = nb::type<Tick>(),
            "invalid_data_bytes"_a = "raise",
            "Read MIDI bytes. Use 'clamp' to reduce invalid channel data bytes to 127."
        )
        .def_static(
            "from_abc",
            [](const std::string& text, nb::handle marker, nb::handle tune) {
                return with_unit(marker, [&](auto unit) {
                    using T = decltype(unit);
                    return ScoreValue(
                        std::make_shared<Score<T>>(
                            parse_abc<T>(nb::bytes(text.data(), text.size()), tune)
                        )
                    );
                });
            },
            "text"_a,
            nb::kw_only(),
            "ttype"_a = nb::type<Tick>(),
            "tune"_a  = nb::none(),
            "Read an ABC tune by its X: number, defaulting to the first tune."
        )
        .def(
            "to_midi",
            [](const ScoreValue& self) {
                return self.visit([](const auto& ptr) { return python_bytes(ptr->to_midi()); });
            },
            "Encode the score as MIDI bytes."
        )
        .def("to_abc", score_to_abc, "Encode the score as ABC text.")
        .def(
            "dump",
            [](const ScoreValue&                 self,
               nb::handle                        target,
               const std::optional<std::string>& format) {
                auto                  selected    = explicit_format(format);
                const bool            path_target = is_path(target);
                std::filesystem::path path;
                if (path_target) {
                    path = nb::cast<std::filesystem::path>(target);
                    if (!selected) selected = score_file_format(path);
                } else {
                    if (!nb::hasattr(target, "write"))
                        throw nb::type_error("Expected a path or binary file");
                    if (!selected)
                        throw std::invalid_argument("format is required for a binary file");
                }
                auto bytes
                    = *selected == DataFormat::ABC ?
                          nb::cast<nb::bytes>(score_to_abc(self).attr("encode")("utf-8")) :
                          self.visit([](const auto& ptr) { return python_bytes(ptr->to_midi()); });
                if (path_target)
                    write_file(path, bytes_span(bytes));
                else {
                    auto written = target.attr("write")(bytes);
                    if (!written.equal(nb::int_(bytes.size()))) {
                        PyErr_SetString(PyExc_OSError, "Could not write the complete score");
                        nb::raise_python_error();
                    }
                }
            },
            "path_or_file"_a,
            nb::kw_only(),
            "format"_a = nb::none(),
            "Write the score as MIDI or ABC. Binary files require format."
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
            [](const ScoreValue& self, nb::handle marker, nb::handle min_duration) {
                return with_unit(marker, [&](auto unit) {
                    using T = decltype(unit);
                    const auto minimum
                        = min_duration.is_none() ? typename T::unit{0} : read_time<T>(min_duration);
                    return self.visit([&](const auto& ptr) {
                        return ScoreValue(std::make_shared<Score<T>>(convert<T>(*ptr, minimum)));
                    });
                });
            },
            "ttype"_a,
            "min_duration"_a = nb::none(),
            nb::sig(
                "def to(self, ttype: type[Tick] | type[Quarter] | type[Second], min_duration: int "
                "| "
                "float | None = None) -> Score"
            ),
            kScoreToDoc
        )
        .def(
            "resample",
            [](const ScoreValue& self, nb::handle ticks_per_quarter, nb::handle min_duration) {
                const auto minimum    = min_duration.is_none() ? 0 : read_time<Tick>(min_duration);
                const auto resolution = read_int32(ticks_per_quarter, "ticks_per_quarter");
                return self.visit([&](const auto& ptr) {
                    return ScoreValue(
                        std::make_shared<Score<Tick>>(resample(*ptr, resolution, minimum))
                    );
                });
            },
            nb::arg(),
            nb::kw_only(),
            "min_duration"_a = nb::none(),
            nb::sig(
                "def resample(self, ticks_per_quarter: int, /, *, "
                "min_duration: int | None = None) -> Score"
            ),
            kScoreResampleDoc
        )
        .def(
            "trim",
            [](const ScoreValue&  self,
               nb::handle         start,
               nb::handle         end,
               nb::handle         min_overlap,
               const std::string& start_mode,
               const std::string& end_mode) {
                return self.visit([&](const auto& ptr) {
                    using T = UnitOf<decltype(ptr)>;
                    return ScoreValue(
                        std::make_shared<Score<T>>(ptr->trim(
                            read_time<T>(start),
                            read_time<T>(end),
                            read_time<T>(min_overlap),
                            start_mode,
                            end_mode
                        ))
                    );
                });
            },
            "start"_a,
            "end"_a,
            "min_overlap"_a = 0,
            "start_mode"_a  = "remove",
            "end_mode"_a    = "remove",
            kScoreTrimDoc
        );

    bind_object_ops(type, [](const auto& score) {
        return std::make_shared<std::decay_t<decltype(score)>>(score.copy());
    });
    type.def_prop_rw(
        "ticks_per_quarter",
        [](const ScoreValue& self) {
            return self.visit([](const auto& ptr) { return ptr->ticks_per_quarter; });
        },
        [](const ScoreValue& self, nb::handle value) {
            const auto resolution = read_int32(value, "ticks_per_quarter");
            self.visit([&](const auto& ptr) { ptr->set_ticks_per_quarter(resolution); });
        },
        "Positive tick resolution. Standard MIDI files support 1..32767."
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

    type.def(
        "note_count",
        [](const ScoreValue& self) {
            return self.visit([](const auto& ptr) { return ptr->note_count(); });
        },
        kScoreNoteNumDoc
    );
    bind_queries(type, kScoreStartDoc, kScoreEndDoc, kScoreEmptyDoc);
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
