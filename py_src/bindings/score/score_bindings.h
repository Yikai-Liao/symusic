//
// Score bindings and file conversion utilities.
//

#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <span>
#include <string_view>

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/string.h>
#include <fmt/format.h>

#include "../../utils/python_helpers.h"
#include "../core/binding_common.h"
#include "../docstring_helpers.h"

namespace symusic {

namespace nb = nanobind;

namespace score_docstrings {
constexpr std::string_view kScoreDocTemplate = R"pbdoc(
{signature}

Container for a full MIDI score consisting of named tracks, tempo/key/time metadata, and helper
methods for serialization and transformations. This variant {timeline_sentence}

Parameters
----------
tpq:
    {tpq_desc}

Note
----
- {measurement}
- Optimized for {best_for}
- Score objects expose Pythonic semantics such as slicing, repr/eq, and pickling.
- ``tpq`` travels with the tempo map so conversions remain lossless.

Examples
--------
.. code-block:: python

{example}
)pbdoc";

template<TType T>
const char* doc() {
    constexpr auto flavor = docstring_helpers::time_flavor<T>();
    static const std::string rendered = fmt::format(
        fmt::runtime(kScoreDocTemplate),
        fmt::arg("signature", fmt::format("Score{}(tpq: int = 480)", flavor.suffix)),
        fmt::arg("timeline_sentence", flavor.timeline_sentence),
        fmt::arg("tpq_desc", flavor.tpq_parameter),
        fmt::arg("measurement", flavor.measurement_sentence),
        fmt::arg("best_for", flavor.best_for),
        fmt::arg(
            "example",
            fmt::format(
                "    from symusic import Score{0}\n"
                "    score = Score{0}(960)\n"
                "    dump_path = \"example.mid\"\n"
                "    score.dump_midi(dump_path)",
                flavor.suffix
            )
        )
    );
    return rendered.c_str();
}

constexpr const char* kScoreMidiFileCtorDoc = R"pbdoc(
Load a MIDI file from disk into a score. Accepts strings or ``pathlib.Path`` objects pointing to a
``.mid``/``.midi`` file.
)pbdoc";
constexpr const char* kScoreFromFileDoc = R"pbdoc(
Read a score from disk by auto-detecting a MIDI or ABC file. Use ``format`` (``"midi"`` or
``"abc"``) when the extension is ambiguous.
)pbdoc";
constexpr const char* kScoreFromMidiDoc = R"pbdoc(
Parse raw MIDI bytes into a score. When ``sanitize_data`` is ``True``, controller/pitch values are
clamped into the MIDI-safe range.
)pbdoc";
constexpr const char* kScoreFromAbcDoc = R"pbdoc(
Parse an ABC notation string into a score. Requires the ``SYMUSIC_ABC2MIDI`` environment variable
to point to the abc2midi executable.
)pbdoc";
constexpr const char* kScoreDumpMidiDoc
    = R"pbdoc(Write the score to a MIDI file using the current ticks-per-quarter resolution.)pbdoc";
constexpr const char* kScoreDumpsMidiDoc
    = R"pbdoc(Serialize the score into MIDI bytes for in-memory workflows.)pbdoc";
constexpr const char* kScoreDumpAbcDoc = R"pbdoc(
Dump the score to an ABC file via midi2abc. Temporary MIDI files are cleaned up automatically.
)pbdoc";
constexpr const char* kScoreDumpsAbcDoc
    = R"pbdoc(Return the score as an ABC notation string. Set *warn* to ``False`` to silence stderr.)pbdoc";
constexpr const char* kScoreToDoc = R"pbdoc(
Convert the score to another time unit (Tick, Quarter, Second). ``ttype`` may be a class object or
``"tick"/"quarter"/"second"``. ``min_dur`` snaps durations when downsampling.
)pbdoc";
constexpr const char* kScoreResampleDoc = R"pbdoc(
Resample the score to a new ticks-per-quarter resolution. ``min_dur`` controls rounding behaviour
when reducing resolution.
)pbdoc";
constexpr const char* kScorePianorollDoc = R"pbdoc(
Export a tick-based score as a pianoroll NumPy tensor. ``mode`` controls aggregation, ``pitch_range``
bounds the vertical axis, and ``encode_velocity`` switches from binary to velocity encoding.
)pbdoc";

constexpr const char* kCopyDoc = R"pbdoc(Return a shallow or deep copy depending on *deep*.)pbdoc";
constexpr const char* kCopyCtorDoc
    = R"pbdoc(Clone another score, deep-copying its tracks and events.)pbdoc";
constexpr const char* kCopyMethodDoc = R"pbdoc(Return a shallow copy of the score.)pbdoc";
constexpr const char* kDeepcopyDoc
    = R"pbdoc(Create a deep copy suitable for Python's copy module.)pbdoc";
constexpr const char* kReprDoc = R"pbdoc(Generate a textual summary of the score.)pbdoc";
constexpr const char* kEqDoc
    = R"pbdoc(Scores compare equal when their handles or contents match.)pbdoc";
constexpr const char* kGetStateDoc
    = R"pbdoc(Serialize the score into ZPP bytes for pickling.)pbdoc";
constexpr const char* kSetStateDoc = R"pbdoc(Rehydrate the score from serialized ZPP bytes.)pbdoc";
constexpr const char* kSortDoc
    = R"pbdoc(Sort tracks chronologically. When *inplace* is False the operation occurs on a copy.)pbdoc";
constexpr const char* kClipDoc
    = R"pbdoc(Clip the score to [start, end). When *inplace* is False returns a new score.)pbdoc";
constexpr const char* kShiftTimeDoc
    = R"pbdoc(Shift every event timestamp by *offset*. When *inplace* is False operate on a copy.)pbdoc";
constexpr const char* kShiftPitchDoc
    = R"pbdoc(Shift every note by *offset* semitones. When *inplace* is False operate on a copy.)pbdoc";
constexpr const char* kShiftVelocityDoc
    = R"pbdoc(Shift every note velocity by *offset*. When *inplace* is False operate on a copy.)pbdoc";
constexpr const char* kStartDoc = R"pbdoc(Return the earliest timestamp across all tracks.)pbdoc";
constexpr const char* kEndDoc
    = R"pbdoc(Return the exclusive end timestamp across all tracks.)pbdoc";
constexpr const char* kNoteNumDoc
    = R"pbdoc(Return the total number of notes across all tracks.)pbdoc";
constexpr const char* kEmptyDoc = R"pbdoc(Return True when the score contains no notes.)pbdoc";
constexpr const char* kAdjustTimeDoc
    = R"pbdoc(Remap timestamps across all tracks using aligned original/new time arrays. Operates in place unless specified.)pbdoc";
constexpr const char* kUseCountDoc
    = R"pbdoc(Return the reference count of the shared score pointer for debugging aliasing.)pbdoc";
}   // namespace score_docstrings

template<TType T>
typename T::unit cast_time(const nb::object& t) {
    using unit = typename T::unit;
    if (t.is_none()) { return static_cast<unit>(0); }
    return nb::cast<unit>(t);
}

template<typename To, typename From>
nb::object convert_score_inner(const shared<Score<From>>& self, const nb::object& min_dur) {
    return nb::cast(
        std::make_shared<Score<To>>(convert<To>(*self, cast_time<To>(min_dur))), nb::rv_policy::copy
    );
}

template<typename From>
nb::object convert_score(
    const shared<Score<From>>& self, const nb::object& ttype, const nb::object& min_dur
) {
    if (ttype.is_none()) throw std::invalid_argument("ttype must be specified");
    if (nb::isinstance<Tick>(ttype)) return convert_score_inner<Tick>(self, min_dur);
    if (nb::isinstance<Quarter>(ttype)) return convert_score_inner<Quarter>(self, min_dur);
    if (nb::isinstance<Second>(ttype)) return convert_score_inner<Second>(self, min_dur);
    if (nb::isinstance<nb::str>(ttype)) {
        const auto ttype_str = nb::cast<std::string>(ttype.attr("lower")());
        if (ttype_str == "tick") return convert_score_inner<Tick>(self, min_dur);
        if (ttype_str == "quarter") return convert_score_inner<Quarter>(self, min_dur);
        if (ttype_str == "second") return convert_score_inner<Second>(self, min_dur);
    }
    throw std::invalid_argument("ttype must be Tick, Quarter, Second or string");
}

template<TType T>
shared<Score<T>> midi2score(const std::filesystem::path& path) {
    auto     data = read_file(path);
    Score<T> s    = Score<T>::template parse<DataFormat::MIDI>(data);
    return std::make_shared<Score<T>>(std::move(s));
}

template<TType T>
void dump_midi(const shared<Score<T>>& self, const std::filesystem::path& path) {
    const auto data = self->template dumps<DataFormat::MIDI>();
    write_file(path, data);
}

template<TType T>
void dump_abc_fs(const shared<Score<T>>& self, const std::filesystem::path& path, const bool warn) {
    const auto midi2abc_env = getenv("SYMUSIC_MIDI2ABC");
    if (!midi2abc_env) {
        throw std::runtime_error("SYMUSIC_MIDI2ABC environment variable not set");
    }
    const std::filesystem::path midi2abc = midi2abc_env;
    if (midi2abc.empty()) { throw std::runtime_error("midi2abc path is empty"); }

    std::filesystem::path temp_midi_path
        = std::filesystem::temp_directory_path()
          / ("symusic_temp_" + std::to_string(std::rand()) + ".mid");

    try {
        dump_midi(self, temp_midi_path);
        std::string cmd = fmt::format(
            R"("{} "{}" -o "{}")", midi2abc.string(), temp_midi_path.string(), path.string()
        );
        int ret = std::system(cmd.c_str());

        if (std::filesystem::exists(temp_midi_path)) { std::filesystem::remove(temp_midi_path); }

        if (!std::filesystem::exists(path) || ret != 0) {
            throw std::runtime_error(
                fmt::format(
                    "midi2abc execution failed (return code: {}). Command: {}. Output file {} not "
                    "created.",
                    ret,
                    cmd,
                    path.string()
                )
            );
        }
    } catch (...) {
        if (std::filesystem::exists(temp_midi_path)) { std::filesystem::remove(temp_midi_path); }
        throw;
    }
}

template<TType T>
void dump_abc_path(
    const shared<Score<T>>& self, const std::filesystem::path& path, const bool warn
) {
    dump_abc_fs<T>(self, path, warn);
}

template<TType T>
std::string dumps_abc(const shared<Score<T>>& self, const bool warn) {
    std::filesystem::path temp_abc_path
        = std::filesystem::temp_directory_path()
          / ("symusic_temp_" + std::to_string(std::rand()) + ".abc");
    try {
        dump_abc_fs(self, temp_abc_path, warn);
        auto data = read_file(temp_abc_path);
        if (std::filesystem::exists(temp_abc_path)) { std::filesystem::remove(temp_abc_path); }
        return std::string(reinterpret_cast<const char*>(data.data()), data.size());
    } catch (...) {
        if (std::filesystem::exists(temp_abc_path)) { std::filesystem::remove(temp_abc_path); }
        throw;
    }
}

inline std::string get_format(const std::filesystem::path& path) {
    const auto ext = path.extension().string();
    if (ext == ".mid" || ext == ".midi" || ext == ".MID" || ext == ".MIDI") { return "midi"; }
    if (ext == ".abc") { return "abc"; }
    throw std::invalid_argument("Unknown file format");
}

template<TType T>
shared<Score<T>> from_abc_file(const std::filesystem::path& path) {
    const auto abc2midi_env = std::getenv("SYMUSIC_ABC2MIDI");
    if (!abc2midi_env) { throw std::runtime_error("abc2midi not found"); }
    const std::string abc2midi = abc2midi_env;
    if (abc2midi.empty()) { throw std::runtime_error("abc2midi not found"); }

    const std::string midi_path = std::tmpnam(nullptr);
    const auto cmd = fmt::format(R"({} "{}" -o "{}" -silent)", abc2midi, path.string(), midi_path);
    const auto ret = std::system(cmd.c_str());
    if (ret != 0) { throw std::runtime_error(fmt::format("abc2midi failed({}): {}", ret, cmd)); }

    const auto data = read_file(midi_path);
    std::filesystem::remove(midi_path);
    auto s = Score<T>::template parse<DataFormat::MIDI>(data);
    return std::make_shared<Score<T>>(std::move(s));
}

template<TType T>
shared<Score<T>> from_abc(const std::string& abc) {
    std::filesystem::path temp_abc_path
        = std::filesystem::temp_directory_path()
          / ("symusic_temp_" + std::to_string(std::rand()) + ".abc");
    try {
        write_file(temp_abc_path, std::span(reinterpret_cast<const u8*>(abc.data()), abc.size()));
        auto score = from_abc_file<T>(temp_abc_path);
        if (std::filesystem::exists(temp_abc_path)) { std::filesystem::remove(temp_abc_path); }
        return score;
    } catch (...) {
        if (std::filesystem::exists(temp_abc_path)) { std::filesystem::remove(temp_abc_path); }
        throw;
    }
}

template<TType T>
shared<Score<T>> from_file(
    const std::filesystem::path& path, const std::optional<std::string>& format
) {
    std::string format_ = format.has_value() ? *format : "";
    if (format_.empty()) {
        format_ = get_format(path);
    } else {
        std::transform(format_.begin(), format_.end(), format_.begin(), ::tolower);
    }

    if (format_ == "midi" || format_ == "mid") { return midi2score<T>(path); }
    if (format_ == "abc") { return from_abc_file<T>(path); }
    throw std::invalid_argument("Unknown file format");
}

template<TType T>
auto bind_score(nb::module_& m, const std::string& name_) {
    using unit   = typename T::unit;
    using self_t = shared<Score<T>>;

    const auto name      = "Score" + name_;
    const auto score_sig = fmt::format("class {}(Score[{}])", name, name_);

    auto copy_func     = [](const self_t& self) { return std::make_shared<Score<T>>(*self); };
    auto deepcopy_func = [](const self_t& self) {
        return std::make_shared<Score<T>>(std::move(self->deepcopy()));
    };

    // clang-format off
    auto score = nb::class_<self_t>(m, name.c_str(), nb::sig(score_sig.c_str()), score_docstrings::doc<T>())
        .def("__init__", &pyutils::pyinit<Score<T>, i32>, nb::arg("tpq"), "Create an empty score with the provided ticks-per-quarter resolution.")
        .def("__init__", [](self_t* self, const self_t& other) {
            new (self) self_t(std::make_shared<Score<T>>(other->deepcopy()));
        }, "Copy constructor", nb::arg("other"), score_docstrings::kCopyCtorDoc)
        .def("copy", [&](const self_t &self, const bool deep) {
            if (deep) return deepcopy_func(self);
            return copy_func(self);
        }, nb::arg("deep") = true, nb::rv_policy::copy, score_docstrings::kCopyDoc)
        .def("__copy__", copy_func, "Shallow copy", nb::rv_policy::copy, score_docstrings::kCopyMethodDoc)
        .def("__deepcopy__", [&](const self_t& self, const nb::handle, const nb::handle) {
            return deepcopy_func(self);
        }, "Deep copy", nb::arg("memo")=nb::none(), nb::arg("_nil")=nb::none(), nb::rv_policy::move, score_docstrings::kDeepcopyDoc)
        .def("__repr__", [](const self_t& self) { return self->to_string(); }, score_docstrings::kReprDoc)
        .def("__eq__", [](const self_t& self, const self_t& other) { return self == other || *self == *other; }, score_docstrings::kEqDoc)
        .def("__eq__", [](const self_t&, nb::handle) { return false; }, score_docstrings::kEqDoc)
        .def("__ne__", [](const self_t& self, const self_t& other) { return self != other && *self != *other; }, score_docstrings::kEqDoc)
        .def("__ne__", [](const self_t&, nb::handle) { return true; }, score_docstrings::kEqDoc)
        .def("__getstate__", [](const self_t& self) {
            const auto native = to_native(*self);
            const vec<unsigned char> data = dumps<DataFormat::ZPP>(native);
            return nb::bytes(reinterpret_cast<const char*>(data.data()), data.size());
        }, score_docstrings::kGetStateDoc)
        .def("__setstate__", [](self_t& self, const nb::bytes& bytes) {
            const auto      data = std::string_view(bytes.c_str(), bytes.size());
            const std::span span(reinterpret_cast<const unsigned char*>(data.data()), data.size());
            auto ans = symusic::parse<symusic::DataFormat::ZPP, ScoreNative<T>>(span);
            new (&self) self_t(std::make_shared<Score<T>>(std::move(to_shared(std::move(ans)))));
        }, score_docstrings::kSetStateDoc)
        .def("__init__", [](self_t* self, const std::filesystem::path& path) {
            new (self) self_t(midi2score<T>(path));
        }, nb::arg("path"), score_docstrings::kScoreMidiFileCtorDoc)
        .def_static("from_file", &from_file<T>, nb::arg("path"), nb::arg("format") = nb::none(), score_docstrings::kScoreFromFileDoc)
        .def_static("from_midi", [](const nb::bytes& data, bool sanitize_data) {
            const auto str  = std::string_view(data.c_str(), data.size());
            const auto span = std::span(reinterpret_cast<const u8*>(str.data()), str.size());
            return std::make_shared<Score<T>>(parse<DataFormat::MIDI, Score<T>>(span, sanitize_data));
        },
            nb::arg("data"),
            nb::arg("sanitize_data") = false,
            score_docstrings::kScoreFromMidiDoc
        )
        .def_static("from_abc", &from_abc<T>, nb::arg("abc"), score_docstrings::kScoreFromAbcDoc)
        .def("dump_midi", &dump_midi<T>, nb::arg("path"), score_docstrings::kScoreDumpMidiDoc)
        .def("dumps_midi", [](const self_t& self) {
            auto data = self->template dumps<DataFormat::MIDI>();
            return nb::bytes(reinterpret_cast<const char*>(data.data()), data.size());
        }, score_docstrings::kScoreDumpsMidiDoc)
        .def("dump_abc", &dump_abc_path<T>, nb::arg("path"), nb::arg("warn") = false, score_docstrings::kScoreDumpAbcDoc)
        .def("dumps_abc", &dumps_abc<T>, nb::arg("warn") = false, score_docstrings::kScoreDumpsAbcDoc)
        .def_prop_rw(RW_COPY(i32, "ticks_per_quarter", ticks_per_quarter), "Ticks per quarter note")
        .def_prop_rw(RW_COPY(i32, "tpq", ticks_per_quarter), "Alias of ticks_per_quarter")
        .def_prop_rw(RW_COPY(shared<vec<shared<Track<T>>>>, "tracks", tracks), "List of tracks")
        .def_prop_rw(RW_COPY(shared<pyvec<TimeSignature<T>>>, "time_signatures", time_signatures), "List of time signature events")
        .def_prop_rw(RW_COPY(shared<pyvec<KeySignature<T>>>, "key_signatures", key_signatures), "List of key signature events")
        .def_prop_rw(RW_COPY(shared<pyvec<Tempo<T>>>, "tempos", tempos), "List of tempo events")
        .def_prop_rw(RW_COPY(shared<pyvec<TextMeta<T>>>, "markers", markers), "List of text/marker events")
        .def_prop_ro("ttype", [](const self_t&) { return T(); }, "Time unit type — Tick/Quarter/Second")
        .def("__use_count", [](const self_t& self) { return self.use_count(); }, score_docstrings::kUseCountDoc)
        .def("to", &convert_score<T>, nb::arg("ttype"), nb::arg("min_dur") = nb::none(), score_docstrings::kScoreToDoc)
        .def("resample", [](const self_t& self, const i32 tpq, const std::optional<unit> min_dur) {
            const unit min_dur_ = min_dur.has_value() ? *min_dur : 0;
            return std::make_shared<Score<Tick>>(resample(*self, tpq, min_dur_));
        }, nb::arg("tpq"), nb::arg("min_dur") = nb::none(), nb::rv_policy::copy, score_docstrings::kScoreResampleDoc)
        .def("sort", [](self_t& self, const bool reverse, const bool inplace) {
            if (inplace) {
                self->sort_inplace(reverse);
                return self;
            }   return std::make_shared<Score<T>>(self->sort(reverse));
        }, nb::arg("reverse") = false, nb::arg("inplace") = true, score_docstrings::kSortDoc)
        .def("clip", [](self_t& self, const unit start, const unit end, const bool clip_end, const bool inplace) {
            if (inplace) {
                self->clip_inplace(start, end, clip_end);
                return self;
            }   return std::make_shared<Score<T>>(self->clip(start, end, clip_end));
        }, nb::arg("start"), nb::arg("end"), nb::arg("clip_end") = false, nb::arg("inplace") = false, score_docstrings::kClipDoc)
        .def("shift_time", [](self_t& self, const unit offset, const bool inplace) {
            if (inplace) {
                self->shift_time_inplace(offset);
                return self;
            }   return std::make_shared<Score<T>>(self->shift_time(offset));
        }, nb::arg("offset"), nb::arg("inplace") = false, score_docstrings::kShiftTimeDoc)
        .def("shift_pitch", [](self_t& self, const i8 offset, const bool inplace) {
            if (inplace) {
                self->shift_pitch_inplace(offset);
                return self;
            }   return std::make_shared<Score<T>>(self->shift_pitch(offset));
        }, nb::arg("offset"), nb::arg("inplace") = false, score_docstrings::kShiftPitchDoc)
        .def("shift_velocity", [](self_t& self, const i8 offset, const bool inplace) {
            if (inplace) {
                self->shift_velocity_inplace(offset);
                return self;
            }   return std::make_shared<Score<T>>(self->shift_velocity(offset));
        }, nb::arg("offset"), nb::arg("inplace") = false, score_docstrings::kShiftVelocityDoc)
        .def("start", [](const self_t& self) { return self->start(); }, score_docstrings::kStartDoc)
        .def("end", [](const self_t& self) { return self->end(); }, score_docstrings::kEndDoc)
        .def("note_num", [](const self_t& self) { return self->note_num(); }, score_docstrings::kNoteNumDoc)
        .def("empty", [](const self_t& self) { return self->empty(); }, score_docstrings::kEmptyDoc)
        .def("adjust_time", [](self_t& self, const vec<unit>& original_times, const vec<unit>& new_times, const bool inplace) {
            if (inplace) {
                ops::adjust_time_inplace(*self, original_times, new_times);
                return self;
            }   return std::make_shared<Score<T>>(ops::adjust_time(*self, original_times, new_times));
        }, nb::arg("original_times"), nb::arg("new_times"), nb::arg("inplace") = false, score_docstrings::kAdjustTimeDoc)
    ;
    // clang-format on

    if constexpr (std::is_same_v<T, Tick>) {
        score.def(
            "pianoroll",
            [](const shared<Score<Tick>>& self,
               const vec<std::string>&    modes,
               const std::pair<i64, i64>  pitch_range,
               const bool                 encode_velocity) {
                vec<PianorollMode> mode_enums(modes.size());
                for (size_t i = 0; i < modes.size(); ++i) {
                    mode_enums[i] = str_to_pianoroll_mode(modes[i]);
                }
                auto pianoroll = ScorePianoroll::from_score(
                    *self, mode_enums, validate_pitch_range(pitch_range), encode_velocity
                );
                auto*       data = const_cast<u8*>(pianoroll.release());
                nb::capsule owner(data, [](void* d) noexcept { delete[] static_cast<u8*>(d); });
                return nb::ndarray<nb::numpy, pianoroll_t>{
                    data,
                    {std::get<0>(pianoroll.dims()),
                     std::get<1>(pianoroll.dims()),
                     std::get<2>(pianoroll.dims()),
                     std::get<3>(pianoroll.dims())},
                    owner
                };
            },
            nb::arg("modes")           = vec<std::string>{"frame", "onset"},
            nb::arg("pitch_range")     = std::pair<i64, i64>(0, 128),
            nb::arg("encode_velocity") = false,
            score_docstrings::kScorePianorollDoc
        );
    }
    return score;
}

}   // namespace symusic
